#include "map_grid.h"
#include "Camera.h"
#include "Corpse.h"
#include "Creature.h"
#include "CreatureGroup.h"
#include "DynamicObject.h"
#include "GameObject.h"
#include "Map.h"
#include "Pet.h"
#include "Player.h"
#include "Object.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "SpecialVisCreature.h"
#include "TemporarySummon.h"
#include "Totem.h"
#include "World.h"
#include "profiling/map_updates.h"

#ifdef PERF_SAMPLING_MAP_UPDATE
namespace profiling
{
map_update map_update_;
}
#define PROFILE_BLOCK_START(id) \
    profiling::map_update_.block_time_start(map_->GetId(), id)
#define PROFILE_BLOCK_END(id) \
    profiling::map_update_.block_time_stop(map_->GetId(), id)
#else
#define PROFILE_BLOCK_START(id)
#define PROFILE_BLOCK_END(id)
#endif

namespace
{
template <typename T>
struct global_pending
{
    std::weak_ptr<maps::map_grid> grid;
    T* obj;
    maps::operation op;
    int x;
    int y;

    global_pending(std::weak_ptr<maps::map_grid> g, T* t, maps::operation o,
        int x_, int y_)
      : grid(g), obj(t), op(o), x(x_), y(y_)
    {
    }
};
std::mutex global_mutex_;
std::vector<global_pending<Player>> global_pending_;
std::mutex global_camera_mutex_;
std::vector<global_pending<Camera>> global_camera_pending_;
}

maps::map_grid::map_grid(Map* map)
  : grid_{[this](int x, int y)
        {
            handle_load_cell(x, y);
        }},
    map_{map}
{
    players_.set_empty_key(nullptr);
    players_.set_deleted_key(reinterpret_cast<Player*>(1));

    active_objects_.set_empty_key(nullptr);
    active_objects_.set_deleted_key(reinterpret_cast<Player*>(1));
}

maps::map_grid::~map_grid()
{
}

// FIXME:
// This should really be in the destructor, but due to twisted design that
// doesn't work (object's CleanupsBeforeDelete() some invoke methods on the
// already destructed Map... Yuck.)
void maps::map_grid::destroy()
{
    // Delete elevators
    for (auto& elevator : elevators_)
    {
        elevator->RemoveFromWorld();
        delete elevator;
    }
    elevators_.clear();

    // Cameras are owned by Player objects so we do not free their resources
    // (the player destructor does).
    framework::grid::single_visitor<Player, Creature, Pet, GameObject,
        DynamicObject, Corpse, SpecialVisCreature, Totem,
        TemporarySummon> visitor;

    // Remove all players queued for removal (no inserts should be pending)
    exec_pending_operations();
    exec_global_pending_operations();

    // Copy grid and clear current one, so visitors called from cleanup do not
    // do stuff with the grid
    // TODO: This design could preferably be reworked so that cleanups are just
    //       that, and don't do all kinds of strange game logic.
    auto grid_copy = grid_;
    grid_.clear();

    // Call CleanupsBeforeDelete() for all entities
    // NOTE: This may not spawn new entities, that would be malformed behavior
    for (auto&& pair : grid_copy)
    {
        auto cell = framework::grid::cell_coords(MAP_CELL_MID, pair.first);
        visitor(cell.first, cell.second, grid_copy, [](auto&& obj)
            {
                // It should not be posible for a player to remain, but let's
                // assert just in case
                assert(
                    obj->GetTypeId() != TYPEID_PLAYER &&
                    "Players cannot still be in grid when destroy() is called");
                obj->CleanupsBeforeDelete();
            });
    }

#ifndef NDEBUG
    // Assert that no new entities were spawned during cleanup
    auto& wops = std::get<womap>(pending_ops_);
    for (auto& op : wops)
        assert(op.op != operation::insert);
    for (auto& op : global_pending_)
        assert(op.op != operation::insert);
#endif

    // Free all resources
    // NOTE: This may not touch the grid or do any operation on other resources,
    //       that would be malformed behavior.
    for (auto&& pair : grid_copy)
    {
        auto cell = framework::grid::cell_coords(MAP_CELL_MID, pair.first);
        visitor(cell.first, cell.second, grid_copy, [](auto&& obj)
            {
                delete obj;
            });
    }
}

void maps::map_grid::spawn_elevators()
{
    if (!elevators_.empty())
        return;

    if (auto elevators =
            sObjectMgr::Instance()->get_static_elevators(map_->GetId()))
    {
        for (auto& data : *elevators)
        {
            if (!(data->spawnMask & (1 << map_->GetSpawnMode())))
                return;

            auto e = new GameObject;
            if (!e->LoadFromDB(data->guid, map_))
            {
                delete e;
                return;
            }

            elevators_.push_back(e);
            e->AddToWorld();
        }
    }
}

void maps::map_grid::insert(WorldObject* t)
{
    auto cell = framework::grid::coords_to_cell_pair(
        MAP_CELL_MID, MAP_CELL_SIZE, t->GetX(), t->GetY());
    std::get<womap>(pending_ops_)
        .emplace_back(t, operation::insert, cell.first, cell.second,
            t->isActiveObject(), make_wo_type(t));

    t->pending_map_insert = true;
}

void maps::map_grid::insert(Player* p)
{
    auto cell = framework::grid::coords_to_cell_pair(
        MAP_CELL_MID, MAP_CELL_SIZE, p->GetX(), p->GetY());

    p->in_global_transit = true;
    p->pending_map_insert = true;

    // Insert into global pending
    std::lock_guard<std::mutex> guard(global_mutex_);
    global_pending_.emplace_back(
        shared_from_this(), p, operation::insert, cell.first, cell.second);
}

void maps::map_grid::erase(WorldObject* t, bool destroy)
{
    auto op = destroy ? operation::destroy : operation::erase;
    auto cell = framework::grid::coords_to_cell_pair(
        MAP_CELL_MID, MAP_CELL_SIZE, t->GetX(), t->GetY());
    std::get<womap>(pending_ops_)
        .emplace_back(t, op, cell.first, cell.second, t->isActiveObject(),
            make_wo_type(t));
}

void maps::map_grid::erase(Player* p, bool destroy)
{
    auto op = destroy ? operation::destroy : operation::erase;
    auto cell = framework::grid::coords_to_cell_pair(
        MAP_CELL_MID, MAP_CELL_SIZE, p->GetX(), p->GetY());

    // Insert erase op into global pending
    std::lock_guard<std::mutex> guard(global_mutex_);
    global_pending_.emplace_back(
        shared_from_this(), p, op, cell.first, cell.second);
}

void maps::map_grid::relocate(WorldObject* t, float x, float y)
{
    auto current_cell = framework::grid::coords_to_cell_pair(
        MAP_CELL_MID, MAP_CELL_SIZE, t->GetX(), t->GetY());
    auto new_cell =
        framework::grid::coords_to_cell_pair(MAP_CELL_MID, MAP_CELL_SIZE, x, y);

    if (current_cell != new_cell)
    {
        std::get<womap>(pending_ops_)
            .emplace_back(t, operation::erase, current_cell.first,
                current_cell.second, false, make_wo_type(t), true);
        std::get<womap>(pending_ops_)
            .emplace_back(t, operation::insert, new_cell.first, new_cell.second,
                false, make_wo_type(t), true);

        t->GetViewPoint().Event_CellChanged(new_cell.first, new_cell.second);
    }
}

void maps::map_grid::relocate(Player* p, float x, float y)
{
    auto current_cell = framework::grid::coords_to_cell_pair(
        MAP_CELL_MID, MAP_CELL_SIZE, p->GetX(), p->GetY());
    auto new_cell =
        framework::grid::coords_to_cell_pair(MAP_CELL_MID, MAP_CELL_SIZE, x, y);

    if (current_cell != new_cell)
    {
        // If in global transit currently; relocates upgrade to global ops
        if (unlikely(p->in_global_transit))
        {
            // Replace current pending global insert
            std::lock_guard<std::mutex> guard(global_mutex_);
            auto itr = std::find_if(global_pending_.begin(),
                global_pending_.end(), [p](const auto& op)
                {
                    return op.obj == p && op.op == operation::insert;
                });
            if (itr != global_pending_.end())
            {
                itr->x = new_cell.first;
                itr->y = new_cell.second;
            }
            return; // Don't do camera ops
        }
        // Local relocation
        else
        {
            std::get<plmap>(pending_ops_)
                .emplace_back(p, operation::erase, current_cell.first,
                    current_cell.second, false, wo_type::ignore, true);
            std::get<plmap>(pending_ops_)
                .emplace_back(p, operation::insert, new_cell.first,
                    new_cell.second, false, wo_type::ignore, true);
        }

        p->GetViewPoint().Event_CellChanged(new_cell.first, new_cell.second);
    }
}

void maps::map_grid::update_object(
    Creature* c, uint32 diff, google::dense_hash_set<CreatureGroup*>& groups)
{
    if (unlikely(c->GetGroup() != nullptr))
    {
        groups.insert(c->GetGroup());
        return;
    }

#ifdef PERF_SAMPLING_MAP_UPDATE
    profiling::map_update_.unit_time_start(map_->GetId());
#endif

    c->Update(c->GetUpdateTracker().timeElapsed(), diff);
    c->GetUpdateTracker().Reset();

#ifdef PERF_SAMPLING_MAP_UPDATE
    profiling::map_update_.unit_time_stop(map_->GetId(), c->GetEntry());
#endif
}

void maps::map_grid::update_object(TemporarySummon* c, uint32 diff,
    google::dense_hash_set<CreatureGroup*>& groups)
{
    if (unlikely(c->GetGroup() != nullptr))
    {
        groups.insert(c->GetGroup());
        return;
    }
#ifdef PERF_SAMPLING_MAP_UPDATE
    profiling::map_update_.unit_time_start(map_->GetId());
#endif

    c->Update(c->GetUpdateTracker().timeElapsed(), diff);
    c->GetUpdateTracker().Reset();

#ifdef PERF_SAMPLING_MAP_UPDATE
    profiling::map_update_.unit_time_stop(map_->GetId(), c->GetEntry());
#endif
}

void maps::map_grid::update_object(SpecialVisCreature* c, uint32 diff,
    google::dense_hash_set<CreatureGroup*>& groups)
{
    if (unlikely(c->GetGroup() != nullptr))
    {
        groups.insert(c->GetGroup());
        return;
    }
#ifdef PERF_SAMPLING_MAP_UPDATE
    profiling::map_update_.unit_time_start(map_->GetId());
#endif

    c->Update(c->GetUpdateTracker().timeElapsed(), diff);
    c->GetUpdateTracker().Reset();

#ifdef PERF_SAMPLING_MAP_UPDATE
    profiling::map_update_.unit_time_stop(map_->GetId(), c->GetEntry());
#endif
}

template <typename T>
void maps::map_grid::update_object(
    T* t, uint32 diff, google::dense_hash_set<CreatureGroup*>&)
{
#ifdef PERF_SAMPLING_MAP_UPDATE
    if (t->GetTypeId() == TYPEID_PLAYER)
        profiling::map_update_.unit_time_start(map_->GetId());
#endif

    t->Update(t->GetUpdateTracker().timeElapsed(), diff);
    t->GetUpdateTracker().Reset();

#ifdef PERF_SAMPLING_MAP_UPDATE
    if (t->GetTypeId() == TYPEID_PLAYER)
        profiling::map_update_.unit_time_stop(map_->GetId(), -1);
#endif
}

void maps::map_grid::update_objects(uint32 diff)
{
    auto vis_range =
        map_->GetVisibilityDistance() + World::GetRelocationLowerLimit() * 2.0f;

    // Carry out saved history that might've accumulated in network handlers
    exec_pending_operations();

    // Object to visit inhabitants of a single cell
    game_entity_visitor_t visitor;

    // CreatureGroups that should have all their members updated
    // Creatures in these group are not updated during the update_square call
    google::dense_hash_set<CreatureGroup*> update_groups;
    update_groups.set_empty_key(nullptr);

    // Lambda to invoke map_grid::update_object
    auto invoke_update_object = [this, diff, &update_groups](auto&& obj)
    {
        // XXX: Explicit this due to bug in GCC.
        this->update_object(obj, diff, update_groups);
    };

    // Lambda to update cell squares an active game entity (obj) can see
    auto update_square =
        [this, vis_range, &visitor, &invoke_update_object](auto&& obj)
    {
        auto bounds = framework::grid::cell_bounds_from_circle(
            MAP_CELL_MID, MAP_CELL_SIZE, obj->GetX(), obj->GetY(), vis_range);
        for (int y = bounds.y_lower; y < bounds.y_upper; ++y)
            for (int x = bounds.x_lower; x < bounds.x_upper; ++x)
            {
                auto cell_id = framework::grid::cell_id(MAP_CELL_MID, x, y);
                if (!updated_cells_[cell_id])
                {
                    visitor(x, y, grid_, invoke_update_object);
                    updated_cells_[cell_id] = true;
                }
            }
    };

    // Set all cells to not updated yet
    updated_cells_.reset();

    // Visit all cells in square formed by map's visibility range from the
    // center of any point a player occupies
    PROFILE_BLOCK_START("grid update - update player squares");
    for (auto& player : players_)
        update_square(player);
    PROFILE_BLOCK_END("grid update - update player squares");

    // Do the same thing for active non-players
    PROFILE_BLOCK_START("grid update - update active obj squares");
    for (auto& obj : active_objects_)
        update_square(obj);
    PROFILE_BLOCK_END("grid update - update active obj squares");

    // Update CreatureGroup members
    PROFILE_BLOCK_START("grid update - update group members");
    for (auto& group : update_groups)
    {
        auto update_fn = [diff](Creature* c)
        {
            if (c->IsPet())
            {
                static_cast<Pet*>(c)->Update(
                    c->GetUpdateTracker().timeElapsed(), diff);
            }
            else if (c->IsSpecialVisCreature())
            {
                static_cast<SpecialVisCreature*>(c)->Update(
                    c->GetUpdateTracker().timeElapsed(), diff);
            }
            else if (c->IsTemporarySummon())
            {
                static_cast<TemporarySummon*>(c)->Update(
                    c->GetUpdateTracker().timeElapsed(), diff);
            }
            else
            {
                static_cast<Creature*>(c)->Update(
                    c->GetUpdateTracker().timeElapsed(), diff);
            }

            c->GetUpdateTracker().Reset();
        };

        // NOTE: Members can be removed from temporary groups during update
        if (group->IsTemporaryGroup())
        {
            std::vector<Creature*> copy = group->GetMembers();
            for (auto& c : copy)
                update_fn(c);
        }
        // For permanent groups we need not make a copy
        else
        {
            for (auto& c : *group)
                update_fn(c);
        }
    }
    PROFILE_BLOCK_END("grid update - update group members");

    // Update elevators
    PROFILE_BLOCK_START("grid update - update elevators");
    for (auto& elevator : elevators_)
    {
        elevator->Update(elevator->GetUpdateTracker().timeElapsed(), diff);
        elevator->GetUpdateTracker().Reset();
    }
    PROFILE_BLOCK_END("grid update - update elevators");

    // Send world objects and item update field changes
    PROFILE_BLOCK_START("grid update - send obj updates");
    map_->SendObjectUpdates();
    PROFILE_BLOCK_END("grid update - send obj updates");

    // Carry out saved history that might've accumulated during the update
    exec_pending_operations();
}

void maps::map_grid::add_active(Player* object)
{
    std::get<actmap<Player>>(pending_active_states_).emplace_back(object, true);
}

void maps::map_grid::add_active(WorldObject* object)
{
    std::get<actmap<WorldObject>>(pending_active_states_)
        .emplace_back(object, true);
}

void maps::map_grid::remove_active(Player* object)
{
    std::get<actmap<Player>>(pending_active_states_)
        .emplace_back(object, false);
}

void maps::map_grid::remove_active(WorldObject* object)
{
    std::get<actmap<WorldObject>>(pending_active_states_)
        .emplace_back(object, false);
}

void maps::map_grid::insert_camera(Camera* camera, int x, int y)
{
    // Insert erase op into global camera pending
    std::lock_guard<std::mutex> guard(global_camera_mutex_);
    global_camera_pending_.emplace_back(
        shared_from_this(), camera, operation::insert, x, y);
}

void maps::map_grid::erase_camera(Camera* camera, bool destroy, int x, int y)
{
    auto op = destroy ? operation::destroy : operation::erase;
    // Insert erase op into global camera pending
    std::lock_guard<std::mutex> guard(global_camera_mutex_);
    global_camera_pending_.emplace_back(shared_from_this(), camera, op, x, y);
}

void maps::map_grid::relocate_camera(
    Camera* camera, int from_x, int from_y, int x, int y)
{
    // x/y is cell x, cell y not world coords
    if (from_x != x || from_y != y)
    {
        std::get<camap>(pending_ops_)
            .emplace_back(camera, operation::erase, from_x, from_y, false,
                wo_type::ignore, true);
        std::get<camap>(pending_ops_)
            .emplace_back(
                camera, operation::insert, x, y, false, wo_type::ignore, true);
    }
}

void maps::map_grid::load_nearby_cells(float x, float y)
{
    // NPCs can have up to 300 yards special visibility
    float load_dist = 300.0f;

    auto bounds = framework::grid::cell_bounds_from_circle(
        MAP_CELL_MID, MAP_CELL_SIZE, x, y, load_dist);

    for (int y = bounds.y_lower; y < bounds.y_upper; ++y)
        for (int x = bounds.x_lower; x < bounds.x_upper; ++x)
            grid_.create_cell(x, y);
}

template <typename T, typename Data>
void maps::map_grid::spawn(const Data* data, Map* map)
{
    if (!(data->spawnMask & (1 << map->GetSpawnMode())))
        return;

    auto t = new T;
    if (!t->LoadFromDB(data->guid, map))
    {
        delete t;
        return;
    }

    map->insert(t);

    // Delay visibility update. This is sort of a hack to bypass the order of
    // loading (the player who makes the cell load will not see any entity
    // because they spawn before he's in the cell).
    t->queue_action(100, [t]()
        {
            if (t->IsInWorld())
                t->GetMap()->UpdateObjectVisibility(t);
        });

    // Need to wait until object is actually in map for BG callback
    auto bg_map = dynamic_cast<BattleGroundMap*>(map);
    if (bg_map && bg_map->GetBG())
    {
        t->queue_action(0, [t, bg_map]()
            {
                if (t->GetMap() == bg_map)
                    bg_map->GetBG()->OnObjectDBLoad(t);
            });
    }
}

// corpse
void maps::map_grid::spawn(uint32 guid, uint32 instance, Map* map)
{
    if (map->GetInstanceId() != instance)
        return;

    auto corpse = sObjectAccessor::Instance()->GetCorpseForPlayerGUID(
        ObjectGuid(HIGHGUID_PLAYER, guid));
    if (!corpse)
        return;

    map->insert(corpse);
}

void maps::map_grid::handle_load_cell(int x, int y)
{
    auto data_cell = to_data_cell(x, y);
    map_->LoadMapAndVMap(data_cell.first, data_cell.second);

    // Load static DB objects
    if (auto objs =
            sObjectMgr::Instance()->get_static_creatures(map_->GetId(), x, y))
        for (auto obj : *objs)
        {
            if (obj->special_visibility)
                spawn<SpecialVisCreature>(obj, map_);
            else
                spawn<Creature>(obj, map_);
        }
    if (auto objs = sObjectMgr::Instance()->get_static_game_objects(
            map_->GetId(), x, y))
        for (auto obj : *objs)
            spawn<GameObject>(obj, map_);
    if (auto objs =
            sObjectMgr::Instance()->get_static_corpses(map_->GetId(), x, y))
        for (auto obj : *objs)
            spawn(obj.first, obj.second, map_);

    // Load instance specific persistent objects
    if (auto state = map_->GetPersistentState())
    {
        if (auto objs = state->get_persistent_creatures(x, y))
            for (auto obj : *objs)
                spawn<Creature>(obj, map_);
        if (auto objs = state->get_persistent_game_objects(x, y))
            for (auto obj : *objs)
                spawn<GameObject>(obj, map_);
    }

    map_->BalanceDynamicTree();
}

void maps::map_grid::exec_pending_operations()
{
    PROFILE_BLOCK_START("grid update - exec pending ops");
    // Lambda to map worldobject to actual type
    auto world_object_to_real_type = [](WorldObject* obj, wo_type type, auto f)
    {
        switch (type)
        {
        case wo_type::npc:
            f(static_cast<Creature*>(obj));
            break;
        case wo_type::pet:
            f(static_cast<Pet*>(obj));
            break;
        case wo_type::go:
            f(static_cast<GameObject*>(obj));
            break;
        case wo_type::dyn:
            f(static_cast<DynamicObject*>(obj));
            break;
        case wo_type::corpse:
            f(static_cast<Corpse*>(obj));
            break;
        case wo_type::special_vis:
            f(static_cast<SpecialVisCreature*>(obj));
            break;
        case wo_type::totem:
            f(static_cast<Totem*>(obj));
            break;
        case wo_type::temp_summon:
            f(static_cast<TemporarySummon*>(obj));
            break;
        case wo_type::ignore:
        default:
            throw std::runtime_error("Unexpected type");
        }
    };

    // Take content of all containers, as they might be inserted to again while
    // we process operations (cells might be loaded into memory)
    auto wo_ops = std::move(std::get<womap>(pending_ops_));
    auto pl_ops = std::move(std::get<plmap>(pending_ops_));
    auto ca_ops = std::move(std::get<camap>(pending_ops_));
    auto wo_act =
        std::move(std::get<actmap<WorldObject>>(pending_active_states_));
    auto pl_act = std::move(std::get<actmap<Player>>(pending_active_states_));
    // Clear the operations we're about to execute from the pending ones
    std::get<womap>(pending_ops_).clear();
    std::get<plmap>(pending_ops_).clear();
    std::get<camap>(pending_ops_).clear();
    std::get<actmap<WorldObject>>(pending_active_states_).clear();
    std::get<actmap<Player>>(pending_active_states_).clear();

    // World Object operations
    for (auto& op : wo_ops)
    {
        if (op.op == operation::insert)
        {
            world_object_to_real_type(op.obj, op.type, [this, &op](auto&& obj)
                {
                    grid_.insert(op.x, op.y, obj);
                    if (!op.relocate)
                    {
                        obj->pending_map_insert = false;
                        map_->inserted_callback(obj);
                    }
                });
            if (op.active)
            {
                // Don't load nearby cells for active non-player
                add_active(op.obj);
            }
        }
        else
        {
            world_object_to_real_type(op.obj, op.type, [this, &op](auto&& obj)
                {
                    if (!op.relocate)
                        map_->erased_callback(obj, op.op == operation::destroy);
                    grid_.erase(op.x, op.y, obj);
                });
            if (op.active)
                remove_active(op.obj);
            if (op.op == operation::destroy)
                delete op.obj;
        }
    }

    auto do_camera_op = [this](auto& op)
    {
        if (op.op == operation::insert)
        {
            grid_.insert(op.x, op.y, op.obj);
        }
        else
        {
            grid_.erase(op.x, op.y, op.obj);
            if (op.op == operation::destroy)
                delete op.obj;
        }
    };

    // Player operations
    for (auto& op : pl_ops)
    {
        if (op.op == operation::insert)
        {
            grid_.insert(op.x, op.y, op.obj);
            if (!op.relocate)
            {
                op.obj->pending_map_insert = false;
                map_->inserted_callback(op.obj);
            }
            load_nearby_cells(op.obj->GetX(), op.obj->GetY());
            if (op.active)
                add_active(op.obj);
        }
        else
        {
            if (!op.relocate)
                map_->erased_callback(op.obj, op.op == operation::destroy);
            grid_.erase(op.x, op.y, op.obj);
            if (op.active)
                remove_active(op.obj);
        }

        // Find camera associated to player and carry out operation
        auto itr =
            std::find_if(ca_ops.begin(), ca_ops.end(), [op](const auto& cam_op)
                {
                    return cam_op.obj->GetOwner() == op.obj;
                });
        if (itr != ca_ops.end())
        {
            do_camera_op(*itr);
            ca_ops.erase(itr);
        }

        if (op.op == operation::destroy)
            delete op.obj;
    }

    // Camera operations not tied to player (is that possible?)
    for (auto& op : ca_ops)
        do_camera_op(op);

    // Activate states for World Objects
    for (auto& state : wo_act)
    {
        if (state.second)
            active_objects_.insert(state.first);
        else
            active_objects_.erase(state.first);
    }

    // Activate states for Players
    for (auto& state : pl_act)
    {
        if (state.second)
            players_.insert(state.first);
        else
            players_.erase(state.first);
    }
    PROFILE_BLOCK_END("grid update - exec pending ops");
}

void maps::map_grid::exec_global_pending_operations()
{
    // NOTE: Need not lock mutex, called when only one thread is executing

    auto ops = std::move(global_pending_);
    auto ca_ops = std::move(global_camera_pending_);
    global_pending_.clear();
    global_camera_pending_.clear();

    auto do_camera_op = [](auto& op)
    {
        auto ptr = op.grid.lock();
        if (!ptr)
            return;

        if (op.op == operation::insert)
        {
            ptr->grid_.insert(op.x, op.y, op.obj);
        }
        else
        {
            ptr->grid_.erase(op.x, op.y, op.obj);
            if (op.op == operation::destroy)
                delete op.obj;
        }
    };

    for (auto& op : ops)
    {
        auto ptr = op.grid.lock();
        if (!ptr)
            continue;

        if (op.op == operation::insert)
        {
            op.obj->in_global_transit = false;
            op.obj->pending_map_insert = false;
            ptr->grid_.insert(op.x, op.y, op.obj);
            ptr->map_->inserted_callback(op.obj);
            ptr->load_nearby_cells(op.obj->GetX(), op.obj->GetY());
            ptr->add_active(op.obj);
        }
        else
        {
            ptr->map_->erased_callback(op.obj, op.op == operation::destroy);
            ptr->grid_.erase(op.x, op.y, op.obj);
            ptr->remove_active(op.obj);
        }

        // Find camera associated to player and carry out operation
        auto itr =
            std::find_if(ca_ops.begin(), ca_ops.end(), [op](const auto& cam_op)
                {
                    return cam_op.obj->GetOwner() == op.obj;
                });
        if (itr != ca_ops.end())
        {
            do_camera_op(*itr);
            ca_ops.erase(itr);
        }

        if (op.op == operation::destroy)
            delete op.obj;
    }

    // Camera operations not tied to player (is that possible?)
    for (auto& op : ca_ops)
        do_camera_op(op);
}

maps::map_grid::wo_type maps::map_grid::make_wo_type(WorldObject* obj)
{
    switch (obj->GetTypeId())
    {
    case TYPEID_UNIT:
    {
        auto c = static_cast<Creature*>(obj);
        if (c->IsPet())
            return wo_type::pet;
        else if (c->IsSpecialVisCreature())
            return wo_type::special_vis;
        else if (c->IsTemporarySummon())
            return wo_type::temp_summon;
        else if (c->IsTotem())
            return wo_type::totem;
        else
            return wo_type::npc;
        break;
    }
    case TYPEID_GAMEOBJECT:
        return wo_type::go;
        break;
    case TYPEID_DYNAMICOBJECT:
        return wo_type::dyn;
        break;
    case TYPEID_CORPSE:
        return wo_type::corpse;
        break;
    default:
        break;
    }

    throw std::runtime_error("Unrecognized type");
}
