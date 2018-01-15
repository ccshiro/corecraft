/*
 * Copyright (C) 2014 Corecraft <https://www.worldofcorecraft.com/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef MANGOS__CONCURRENT_PATHGEN_H
#define MANGOS__CONCURRENT_PATHGEN_H

#include "Common.h"
#include "ObjectGuid.h"
#include "PathFinder.h"
#include "SharedDefines.h"
#include "G3D/Vector3.h"
#include "Policies/Singleton.h"
#include <sparsehash/dense_hash_map>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

class Map;

struct path_container
{
    // Internal data
    struct path_data
    {
        Map* map; // Not safe to dereference; threaded
        ObjectGuid src_guid;
        ObjectGuid dst_guid;
        movement::retail_like_path_builder builder;
        PathFinder finder;
        bool force_dest;
        bool success;
        uint32 path_id;
    };

    // Data returned to whomever requested a path
    struct path_out
    {
        Unit* src;
        std::vector<G3D::Vector3> path;
        std::function<void(Unit*, std::vector<G3D::Vector3>, uint32)> receiver;
        uint32 path_id;
    };

    path_container()
    {
        completed.set_empty_key(nullptr);
        completed.set_deleted_key((Map*)0x1);
    }

    std::mutex mutex;
    // Intentionally std::list
    std::list<path_data*> queued;
    google::dense_hash_map<Map*, std::vector<path_data*>> completed;
};

class concurrent_pathgen
{
public:
    void on_erased_map(Map* map);

    void start();
    void stop();

    // returns: id of queued path
    uint32 queue_path(Map*, const movement::retail_like_path_builder& builder,
        bool force_dest);

    std::vector<path_container::path_out> get_completed_paths(Map* map);

    // returns: state of config option "Concurrency.UsePathgenThread" when
    // server was started
    bool in_use() const { return is_running_; }

private:
    std::thread worker_thread_;
    std::mutex running_mutex_;
    bool is_running_ = false;
    std::atomic<uint32> next_path_id_{0};
    path_container container_;

    void run();
    void build_path(path_container::path_data& data);
};

#define sConcurrentPathgen MaNGOS::UnlockedSingleton<concurrent_pathgen>

#endif
