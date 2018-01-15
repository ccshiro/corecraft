#ifndef MANGOS__THREADED_MAPS_H
#define MANGOS__THREADED_MAPS_H

#include "Common.h"
#include <boost/thread.hpp>
#include <list>
#include <map>
#include <string>
#include <vector>

class Map;

/* Threaded maps is run() from a single-threaded scenario,
   and so is push_map(). Once run() is called threaded_maps
   becomes multi-threaded and push_map() will throw if called
   before run() returns */
class threaded_maps
{
public:
    threaded_maps();
    ~threaded_maps();

    void initalize();

    // push_map() is NOT safe to call before the current run() completes
    void push_map(Map* map);
    void run(time_t map_update_timer);

    // Single-threaded functions, called from ChatHandler
    std::string performance_log() const;
    void reset_performance_log()
    {
        performance_monitor_.clear();
        updates_ = 0;
        update_ms_ = 0;
    }

    // Called from WorldThread when server shuts down
    void stop();

private:
    void thread_main(); // Every running thread runs this function
    Map*
    get_map(); // Gets a map for the calling thread; causes a lock of maps_lock_
    void check_in(); // Threads call this when they're done with their work

    unsigned int thread_count_; // Number of threads used for map updating
    uint32 updates_;            // Number of updates since server start
    time_t map_update_timer_; // Passed to Map::Update(). Does not change while
                              // in threaded-mode, so no lock required
    std::mutex maps_lock_;    // This is the lock for access to the maps_ list
    std::list<Map*> maps_;    // List used for conveniant fron-insertion; its
                              // performance is of no signficiance at all
    std::mutex performance_lock_; // Lock for updating performance_monitor_.
                                  // Required because more than one map with the
                                  // same id might be updated.
    std::map<uint32 /*map_id*/, uint32 /*ran_time*/>
        performance_monitor_; // Monitors process time of each map, can be
                              // turned off by defining
                              // THREADED_MAPS_NO_PERFORMANCE_MONITOR
    uint32 update_ms_; // Combined time of all updates (in ms) since updates_
                       // was last reset
    std::vector<boost::thread*> threads_; // Running threads, these are the
                                          // threads used to process maps_
    boost::barrier* barrier_; // Barrier for when threads are in waiting
};

#endif
