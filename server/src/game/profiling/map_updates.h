#ifndef GAME__PROFILING__UNIT_UPDATES_H
#define GAME__PROFILING__UNIT_UPDATES_H

#include <cstdio>
#include <chrono>
#include <map>
#include <mutex>
#include <string>

namespace profiling
{
struct map_update
{
    struct unit_entry
    {
        int hits = 0;
        float time_ms = 0;
    };

    struct block_entry
    {
        std::chrono::high_resolution_clock::time_point begin;
        std::chrono::high_resolution_clock::time_point end;
    };

    std::mutex mut;
    int map_id = -1;
    float map_time = 0;
    std::map<int /*entry, -1 for players*/, unit_entry> unit_data;
    std::map<const char*, block_entry> block_data;
    std::chrono::high_resolution_clock::time_point begin;
    std::chrono::high_resolution_clock::time_point begin_map;

    void start(int id)
    {
        std::lock_guard<std::mutex> guard(mut);
        map_id = id;
        map_time = 0;
        unit_data.clear();
    }

    void map_start(int map)
    {
        // Unguarded check
        if (map_id == -1)
            return;

        std::lock_guard<std::mutex> guard(mut);
        if (map != map_id)
            return;

        map_time = 0;
        begin_map = std::chrono::high_resolution_clock::now();
    }

    void map_end(int map)
    {
        // Unguarded check
        if (map_id == -1)
            return;

        // Take end time befoer locking mutex, so we don't get our timer
        // affected by it in case it blocks
        auto end = std::chrono::high_resolution_clock::now();

        std::lock_guard<std::mutex> guard(mut);
        if (map != map_id)
            return;

        auto duration =
            std::chrono::duration_cast<std::chrono::duration<double>>(
                end - begin_map);

        map_time = duration.count() * 1000.0;

        _write();

        map_id = -1;
    }

    void unit_time_start(int map)
    {
        // Unguarded check
        if (map_id == -1)
            return;

        std::lock_guard<std::mutex> guard(mut);
        if (map != map_id)
            return;

        begin = std::chrono::high_resolution_clock::now();
    }

    void unit_time_stop(int map, int entry)
    {
        // Unguarded check
        if (map_id == -1)
            return;

        // Take end time befoer locking mutex, so we don't get our timer
        // affected by it in case it blocks
        auto end = std::chrono::high_resolution_clock::now();

        std::lock_guard<std::mutex> guard(mut);
        if (map != map_id)
            return;

        auto duration =
            std::chrono::duration_cast<std::chrono::duration<double>>(
                end - begin);

        unit_data[entry].hits += 1;
        unit_data[entry].time_ms += duration.count() * 1000.0;
    }

    void block_time_start(int map, const char* id)
    {
        // Unguarded check
        if (map_id == -1)
            return;

        std::lock_guard<std::mutex> guard(mut);
        if (map != map_id)
            return;

        block_data[id].begin = std::chrono::high_resolution_clock::now();
    }

    void block_time_stop(int map, const char* id)
    {
        // Unguarded check
        if (map_id == -1)
            return;

        // Take end time befoer locking mutex, so we don't get our timer
        // affected by it in case it blocks
        auto end = std::chrono::high_resolution_clock::now();

        std::lock_guard<std::mutex> guard(mut);
        if (map != map_id)
            return;

        block_data[id].end = end;
    }

    // Must hold mutex to call
    void _write()
    {
        float time = 0;

        auto fp = fopen("map_updates.txt", "w");
        if (!fp)
            return;

        std::vector<std::string> block_output;
        for (auto& pair : block_data)
        {
            auto& entry = pair.second;
            std::string str("Spent ");

            auto ms = std::chrono::duration_cast<std::chrono::duration<double>>(
                          entry.end - entry.begin).count() *
                      1000.0;

            str += std::to_string(ms) + " ms ON \"" + pair.first + "\"";

            block_output.emplace_back(std::move(str));
        }

        std::sort(block_output.begin(), block_output.end(),
            std::greater<std::string>());

        std::vector<std::string> unit_output;
        for (auto& pair : unit_data)
        {
            time += pair.second.time_ms;

            std::string str("Spent ");

            str += std::to_string(pair.second.time_ms) + " ms ON ";

            if (pair.first == -1)
                str += "Players (count: ";
            else
                str += "NPC with entry " + std::to_string(pair.first) +
                       " (count: ";
            str += std::to_string(pair.second.hits) + ")";

            unit_output.emplace_back(std::move(str));
        }

        std::sort(unit_output.begin(), unit_output.end(),
            std::greater<std::string>());

        fprintf(fp, "A single Map::Update() snapshot for map %d.\n", map_id);
        fprintf(fp,
            "The update took %f ms. Below is a summary of time spent.\n\n",
            map_time);

        fprintf(fp, "Time spent on blocks:\n");
        for (auto& line : block_output)
            fprintf(fp, "%s\n", line.c_str());

        fprintf(fp, "\nTime spent on unit updates (total time: %f):\n", time);
        for (auto& line : unit_output)
            fprintf(fp, "%s\n", line.c_str());

        fclose(fp);

        map_id = -1;
    }
};
}

#endif
