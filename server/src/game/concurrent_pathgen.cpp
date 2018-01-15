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

#include "concurrent_pathgen.h"
#include "Map.h"
#include "Unit.h"

void concurrent_pathgen::on_erased_map(Map* map)
{
    std::lock_guard<std::mutex> guard(container_.mutex);
    container_.completed.erase(map);
    container_.queued.remove_if([map](const auto& path)
        {
            return path->map == map;
        });
}

void concurrent_pathgen::start()
{
    is_running_ = true;
    worker_thread_ = std::thread(&concurrent_pathgen::run, this);
}

void concurrent_pathgen::stop()
{
    {
        std::lock_guard<std::mutex> guard(running_mutex_);
        if (!is_running_)
            return;
        is_running_ = false;
    } // release lock so thread can exit
    worker_thread_.join();
}

void concurrent_pathgen::run()
{
    while (true)
    {
        {
            std::lock_guard<std::mutex> guard(running_mutex_);
            if (!is_running_)
                return;
        }

        path_container::path_data* path = nullptr;

        {
            std::lock_guard<std::mutex> guard(container_.mutex);
            if (!container_.queued.empty())
            {
                path = container_.queued.front();
                container_.queued.pop_front();
            }
        }

        if (path)
        {
            build_path(*path);
            std::lock_guard<std::mutex> guard(container_.mutex);
            if (container_.completed.count(path->map) == 1)
                container_.completed[path->map].push_back(path);
            else
                delete path;
        }
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void concurrent_pathgen::build_path(path_container::path_data& data)
{
    data.success = false;

    if (data.builder.get_maxlen() > 0)
        data.finder.setPathLengthLimit(data.builder.get_maxlen());

    if (!data.finder.calculate(data.builder.get_src().x,
            data.builder.get_src().y, data.builder.get_src().z,
            data.builder.get_dst().x, data.builder.get_dst().y,
            data.builder.get_dst().z, data.force_dest))
        return;

    if (data.finder.getPathType() &
        (PATHFIND_NOPATH | PATHFIND_INCOMPLETE | PATHFIND_SHORT))
        return;

    data.builder.set_path(data.finder.getPath());
    data.builder.set_eop_index(data.finder.getEndOfPathIndex());

    data.success = true;
}

uint32 concurrent_pathgen::queue_path(Map* map,
    const movement::retail_like_path_builder& builder, bool force_dest)
{
    // Initialize path metadata
    auto path = new path_container::path_data;
    path->map = map;
    path->src_guid = builder.get_src_obj()->GetObjectGuid();
    if (builder.get_dst_obj())
        path->dst_guid = builder.get_dst_obj()->GetObjectGuid();
    path->builder = builder;
    path->force_dest = force_dest;

    uint32 id = ++next_path_id_;
    path->path_id = id;

    // Initialize PathFinder data
    path->finder.delayedInit(builder.get_src_obj(), map->GetId());

    // Queue path for generation
    std::lock_guard<std::mutex> guard(container_.mutex);
    container_.queued.push_back(std::move(path));

    return id;
}

std::vector<path_container::path_out> concurrent_pathgen::get_completed_paths(
    Map* map)
{
    std::vector<path_container::path_out> paths;
    std::vector<path_container::path_data*> queued_paths;

    {
        std::lock_guard<std::mutex> guard(container_.mutex);
        if (container_.completed[map].empty())
            return paths;
        std::swap(queued_paths, container_.completed[map]);
    }

    for (auto& path : queued_paths)
    {
        // Check so pointers in the builder class are still valid
        auto src = map->GetUnit(path->src_guid);
        if (src == nullptr)
        {
            delete path;
            continue;
        }
        path->builder.set_src(src);

        path_container::path_out out;

        out.src = src;
        out.receiver = path->builder.get_receiver();
        out.path_id = path->path_id;

        // Update dst pointer; distribute empty on disappeared
        if (path->dst_guid)
        {
            auto dst = map->GetUnit(path->dst_guid);
            if (dst == nullptr)
            {
                paths.push_back(std::move(out));
                delete path;
                continue;
            }
            path->builder.set_dst(dst);
        }

        // No path was generated; distribute empty path
        if (!path->success)
        {
            paths.push_back(std::move(out));
            delete path;
            continue;
        }

        // Not threadable PathFinder work
        path->finder.finalize(path->builder.get_src_obj());

        // PathFinder::finalize() can mark path as invalid; give empty path
        if (path->finder.getPathType() &
            (PATHFIND_NOPATH | PATHFIND_INCOMPLETE | PATHFIND_SHORT))
        {
            paths.push_back(std::move(out));
            delete path;
            continue;
        }

        // Not threadable builder work
        path->builder.do_finish_phase(true);
        out.path = path->builder.path();

        paths.push_back(std::move(out));
        delete path;
    }

    return paths;
}
