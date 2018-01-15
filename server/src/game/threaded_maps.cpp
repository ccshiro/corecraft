#include "threaded_maps.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "Map.h"
#include "World.h"

threaded_maps::threaded_maps()
  : updates_(0), map_update_timer_(0), update_ms_(0), barrier_(nullptr)
{
    /* Empty */
}

threaded_maps::~threaded_maps()
{
    for (auto& elem : threads_)
    {
        elem->interrupt();
        delete elem;
    }
    delete barrier_;
}

void threaded_maps::initalize()
{
    thread_count_ = sWorld::Instance()->getConfig(CONFIG_UINT32_MAP_THREADS);

    delete barrier_;
    // +1 because we're including the main thread
    barrier_ = new boost::barrier(thread_count_ + 1);

    // Spawn as many threads as are specified in the configuration file
    for (unsigned int i = 0; i < thread_count_; ++i)
    {
        auto t = new boost::thread(&threaded_maps::thread_main, this);
        threads_.push_back(t);
    }
}

void threaded_maps::stop()
{
    // Wake up threads to make them exit cleanly
    barrier_->wait();
    for (auto t : threads_)
    {
        t->join();
        delete t;
    }
    threads_.clear();

    delete barrier_;
    barrier_ = nullptr;
}

void threaded_maps::thread_main()
{
    while (true)
    {
        // First wait for map processing to begin
        barrier_
            ->wait(); // All map threads are waiting for the main thread to join
        // them in this barrier; that indicates it's time to update maps

        if (World::IsStopped())
            return;

        // Then keep fetching maps until there's no left
        while (true)
        {
            Map* map = get_map();

            if (map == nullptr)
            {
                // We're done with all the work available to us. It's time to
                // check in
                check_in();
                break; // Go back to waiting for the barrier again
            }

#ifndef THREADED_MAPS_NO_PERFORMANCE_MONITOR
            uint32 before = WorldTimer::getMSTime();
#endif

            // Do the actual map update
            map->Update(map_update_timer_);

#ifndef THREADED_MAPS_NO_PERFORMANCE_MONITOR
            uint32 after = WorldTimer::getMSTime();
            uint32 delta = after - before;
            if (delta > 0)
            {
                std::lock_guard<std::mutex> lock(performance_lock_);
                performance_monitor_[map->GetId()] += delta;
            }
#endif
        }
    }
}

void threaded_maps::check_in()
{
    // We wait for the barrier to release. This makes sure all
    // threads finishes before the main thread goes on
    barrier_->wait();
}

Map* threaded_maps::get_map()
{
    std::lock_guard<std::mutex> lock(maps_lock_);
    if (maps_.empty())
        return nullptr;
    // Return first available map
    Map* map = maps_.front();
    maps_.pop_front();
    return map;
}

void threaded_maps::push_map(Map* map)
{
    // Make sure that ourdoor continents are put first into the queue, to cause
    // them to
    // begin updating as soon as possible. This makes sure the big maps are
    // chewing
    // away while the smaller maps are getting updated, rather than having the
    // big
    // maps wait for them to finish before it can begin processing.
    if (map->GetId() == 530 || map->GetId() == 0 ||
        map->GetId() == 1) // Outland, Eastern Kingdoms and Kalimdor
        maps_.push_front(map);
    else
        maps_.push_back(map);
}

void threaded_maps::run(time_t map_update_timer)
{
    if (World::IsStopped())
        return;

    map_update_timer_ = map_update_timer;

    // Join the barrier; this will cause it to release
    // all threads; effectively starting the map updates
    barrier_->wait();

#ifndef THREADED_MAPS_NO_PERFORMANCE_MONITOR
    uint32 before = WorldTimer::getMSTime();
#endif

    // Wait for all the threads to complete their work and check back in
    barrier_->wait();

#ifndef THREADED_MAPS_NO_PERFORMANCE_MONITOR
    uint32 after = WorldTimer::getMSTime();
    uint32 delta = after - before;
    update_ms_ += delta;
#endif

    ++updates_;
}

// FIXME: Move list_data to the comment that says here.
// In C++03 we can't use such local classess to instantiate templates
// (the vector), but that restriction was removed in C++11
struct list_data
{
    list_data(uint32 i, uint32 j) : map_id(i), total_ms(j) {}
    uint32 map_id;
    uint32 total_ms;
    bool operator<(const list_data& rhs) const
    {
        return total_ms < rhs.total_ms;
    }
};

std::string threaded_maps::performance_log() const
{
#ifdef THREADED_MAPS_NO_PERFORMANCE_MONITOR
    return "Core was built without performance logs for threaded maps.";
#endif

    /* FIXME: Move list_data here in C++11 */

    // Build a list sorted by total run time of map (ascendingly)
    std::vector<list_data> data;
    uint64 accumulated_time = 0; // Time spent on all maps combined
    for (const auto& elem : performance_monitor_)
    {
        accumulated_time += elem.second;
        data.push_back(list_data(elem.first, elem.second));
    }
    std::sort(data.begin(), data.end());

    if (accumulated_time == 0)
        return "No data gathered yet.";

    std::string str = "=== Threading performance log: ===\n";
    str +=
        "Format: Name: % time spent updating this map out of all maps (average "
        "time spent over all updates)\n";
    for (auto& elem : data)
    {
        const MapEntry* map_data = sMapStore.LookupEntry(elem.map_id);
        if (!map_data)
            continue;
        std::ostringstream ss;
        ss.precision(3);
        ss << map_data->name[0] << ": "
           << (static_cast<float>(elem.total_ms) / accumulated_time) * 100.0f
           << "%";
        ss.precision(5);
        ss << " (avg: " << (static_cast<float>(elem.total_ms) / updates_)
           << " ms/update)\n";
        str += ss.str();
    }

    std::ostringstream ss;
    ss << "World Updates since last reset: " << updates_ << ".\n";
    ss << "Average time spent per World Update: "
       << static_cast<float>(update_ms_) / updates_ << " ms.\n";
    str += ss.str();

    return str;
}
