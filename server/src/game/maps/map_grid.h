#ifndef GAME__MAPS__MAP_GRIDS_H
#define GAME__MAPS__MAP_GRIDS_H

#include "framework/grid/grid.h"
#include "framework/grid/grid_helpers.h"
#include "Common.h"
#include "ObjectGuid.h"
#include <sparsehash/dense_hash_set>
#include <bitset>
#include <memory>

class Camera;
class Corpse;
class Creature;
class CreatureGroup;
class DynamicObject;
class GameObject;
class Map;
class Pet;
class Player;
class SpecialVisCreature;
class TemporarySummon;
class Totem;
class WorldObject;

// NOTE: The game-grid is made up of 64x64 533.33333x533.33333 sized cells.
//       Cells are a bit bigger than what we want, so we're changing the sides
//       to 1/8 of that, meaning we of course also get 8 times the cell count.
//       Each map therefore has up to 512x512 total cells.
#define MAP_CELL_SIZE 66.6666f           // Length of one side in the cell
#define MAP_CELL_DENOMINATOR 8           // 1/8th the size of a WoW cell
#define MAP_CELL_COUNT 512               // Number of rows or columns
#define MAP_CELL_MID 256                 // Cell at (256, 256) is center of grid
#define MAP_GRID_TOTAL_CELLS (512 * 512) // Total amount of cells in grid
#define MAP_DATA_FMT_CELL_COUNT 64       // Number of rows or cols for data fmt

namespace maps
{
// Grid pending operations
enum class operation
{
    insert,
    erase,
    destroy // erase + free resources
};

// Management of grids per map. Responsible for loading cells (i.e. spawning
// NPCs, etc) when they're entered, interfacing visits to the underlying grid,
// doing the map tick update for each object and keeping track of active
// objects.
class map_grid : public std::enable_shared_from_this<map_grid>
{
public:
    using grid_type = framework::grid::grid<MAP_CELL_COUNT, Player, Creature,
        Pet, GameObject, DynamicObject, Corpse, Camera, SpecialVisCreature,
        Totem, TemporarySummon>;
    using game_entity_visitor_t =
        framework::grid::single_visitor<Player, Creature, Pet, GameObject,
            DynamicObject, Corpse, SpecialVisCreature, Totem, TemporarySummon>;

    map_grid(Map* map);
    ~map_grid();
    // This does the job of a destructor. See FIXME in implementation file
    void destroy();

    // Map elevators (GO type=11, transport)
    void spawn_elevators();
    const std::vector<GameObject*>& elevators() const { return elevators_; }

    // Insert object into grid
    // NOTE: Inserting the same object twice (without erase() in between) is
    // undefined behavior.
    void insert(WorldObject* t);
    void insert(Player* t);

    // Remove object from grid
    // destroy: if true, the object's resources is also freed
    // NOTE: Removing not previously inserted object is undefined behavior.
    void erase(WorldObject* t, bool destroy);
    void erase(Player* p, bool destroy);

    // Move object to new X and Y
    // NOTE: The object's X and Y must NOT have changed since last interaction
    //       with grid. That'd result in undefined behavior.
    //       The object must also have been insert()'ed prior to this call.
    void relocate(WorldObject* t, float x, float y);
    void relocate(Player* t, float x, float y);

    // Update active objects and players in grid. All cells touched by these
    // forementioned entities also have the rest of their inhabitants updated.
    void update_objects(uint32 diff);

    // Attempt making object active
    void add_active(Player* object);
    void add_active(WorldObject* object);
    // Attempt making object inactive
    void remove_active(Player* object);
    void remove_active(WorldObject* object);

    // Returns reference to underlying grid, needed to pass to visitors
    grid_type& get_grid() { return grid_; }

    // TODO: There's no real reason why camera has to have this special
    //       implementaion.
    // NOTE: Same notes as for normal insert, erase and relocate apply
    void insert_camera(Camera* camera, int x, int y);
    void erase_camera(Camera* camera, bool destroy, int x, int y);
    void relocate_camera(Camera* camera, int from_x, int from_y, int x, int y);

    // Force load a single cell
    void load_cell(int x, int y) { grid_.create_cell(x, y); }

    // Executes pending inserts and erases
    void exec_pending_operations();
    // Executes pending global inserts and erases (player inter-map transfer)
    static void exec_global_pending_operations();

private:
    grid_type grid_;

    // Cells we've already updated. To avoid updating a cell more than once per
    // update_objects() call.
    std::bitset<MAP_GRID_TOTAL_CELLS> updated_cells_;

    // Handle to map
    Map* map_;

    // Players in grid, part of deriving what cells must be updated
    google::dense_hash_set<Player*> players_;
    // Active non-players in grid, part of deriving what cells must be updated
    google::dense_hash_set<WorldObject*> active_objects_;

    // Stationary transports (GO type == 11)
    std::vector<GameObject*> elevators_;

    // Temporary history holder for insert/erase operations. Will be played back
    // in the same order in a safe manner (to not have calling insert/erase be a
    // dangerous operation that needs to be considered carefully).
    enum class wo_type
    {
        ignore, // Player and camera
        npc,
        pet,
        go,
        dyn,
        corpse,
        special_vis,
        totem,
        temp_summon
    };
    template <typename T>
    struct pending
    {
        T* obj;
        operation op;
        int x;
        int y;
        wo_type type;
        bool active;
        bool relocate;

        pending(T* t, operation o, int i, int j, bool a,
            wo_type h = wo_type::ignore, bool r = false)
          : obj{t}, op{o}, x{i}, y{j}, type{h}, active{a}, relocate{r}
        {
        }
    };
    using plmap = std::vector<pending<Player>>;
    using camap = std::vector<pending<Camera>>;
    using womap = std::vector<pending<WorldObject>>;
    std::tuple<plmap, camap, womap> pending_ops_;

    // Set active/inactive history (true = activate, false = deactivate)
    template <typename T>
    using actmap = std::vector<std::pair<T*, bool>>;
    std::tuple<actmap<WorldObject>, actmap<Player>> pending_active_states_;

    // These functions are used by update_objects to do the actual updating
    void update_object(Creature* c, uint32 diff,
        google::dense_hash_set<CreatureGroup*>& groups);
    void update_object(TemporarySummon* c, uint32 diff,
        google::dense_hash_set<CreatureGroup*>& groups);
    void update_object(SpecialVisCreature* c, uint32 diff,
        google::dense_hash_set<CreatureGroup*>& groups);
    template <typename T>
    void update_object(
        T* t, uint32 diff, google::dense_hash_set<CreatureGroup*>& groups);

    // Force nearby cells to be loaded if they aren't already
    void load_nearby_cells(float x, float y);

    // Spawning object helpers, used by handle_load_cell
    template <typename T, typename Data>
    void spawn(const Data* data, Map* map);
    void spawn(uint32 guid, uint32 instance, Map* map);

    // Callback once a cell is created. Expected to spawn inhabitants.
    void handle_load_cell(int x, int y);

    wo_type make_wo_type(WorldObject* obj);
};
}

// Helper functions
namespace maps
{
// Returns cell (X, Y) from world coordinates
inline std::pair<int, int> coords_to_cell_pair(float x, float y)
{
    return framework::grid::coords_to_cell_pair(
        MAP_CELL_MID, MAP_CELL_SIZE, x, y);
}

// Returns cell id from cell x, y
inline int cell_id(int x, int y)
{
    return framework::grid::cell_id(MAP_CELL_MID, x, y);
}

// Returns cell id from world coordinates
inline int cell_id(float x, float y)
{
    auto cell =
        framework::grid::coords_to_cell_pair(MAP_CELL_MID, MAP_CELL_SIZE, x, y);
    return cell_id(cell.first, cell.second);
}

inline bool verify_coord(float x_or_y)
{
    auto cell =
        framework::grid::coord_to_cell(MAP_CELL_MID, MAP_CELL_SIZE, x_or_y);
    return 0 <= cell && cell < MAP_CELL_COUNT;
}

// Returns true if coordinates are inside the map_grid
inline bool verify_coords(float x, float y)
{
    return verify_coord(x) && verify_coord(y);
}

// Data formats use WoW's cell size, this function translates from cell
// coordinates to Data cell X,Y
inline std::pair<int, int> to_data_cell(int x, int y)
{
    return std::make_pair(
        63 - x / MAP_CELL_DENOMINATOR, 63 - y / MAP_CELL_DENOMINATOR);
}

// Data formats use WoW's cell size, this function translates from world
// coordinates to Data cell X,Y
inline std::pair<int, int> world_coords_to_data_cell(float x, float y)
{
    auto cell =
        framework::grid::coords_to_cell_pair(MAP_CELL_MID, MAP_CELL_SIZE, x, y);
    return to_data_cell(cell.first, cell.second);
}
}

#endif
