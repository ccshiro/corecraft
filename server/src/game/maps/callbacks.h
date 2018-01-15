/**
 * callbacks.h
 *
 * Here are callbacks meant to be used together with visitor functors which you
 * can find in visitors.h. Only common callbacks are put here.
 *
 * NOTE: If a check is a one-off thing, you should just use a lambda instead of
 *       adding code here.
 */

#ifndef GAME__MAPS__CALLBACKS_H
#define GAME__MAPS__CALLBACKS_H

#include "Player.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include <memory>
#include <vector>

class Player;

namespace maps
{
namespace callbacks
{
// Provides a cache for localized text packets so they need not be recreated for
// the same locale. Use with WorldPacket.
template <typename Worker>
struct localize_packet
{
    std::vector<std::shared_ptr<WorldPacket>> cache;
    Worker worker;

    // worker: callback to construct a packet, function prototype:
    //         void f(WorldPacket&, uint32 locale_index)
    localize_packet(Worker w) : worker{w} {}

    void operator()(Player* p)
    {
        auto locale_index = p->GetSession()->GetSessionDbLocaleIndex();
        // locale is in range [-1,max_locale-1)
        auto cache_index = locale_index + 1;

        if (cache_index >= static_cast<int>(cache.size()))
            cache.resize(cache_index + 1);

        auto& data = cache[cache_index];
        if (!data)
        {
            data.reset(new WorldPacket{SMSG_MESSAGECHAT});
            worker(*data, locale_index);
        }

        p->GetSession()->send_packet(data.get());
    }
};

// Provides a cache for localized text packets so they need not be recreated for
// the same locale. Use with std::vector<WorldPacket*>
template <typename Worker>
struct localize_packets
{
    std::vector<std::shared_ptr<std::vector<WorldPacket*>>> cache;
    Worker worker;

    // worker: callback to construct a packet, function prototype:
    //         void f(WorldPacket&, uint32 locale_index)
    localize_packets(Worker w) : worker{w} {}

    void operator()(Player* p)
    {
        auto locale_index = p->GetSession()->GetSessionDbLocaleIndex();
        // locale is in range [-1,max_locale-1)
        auto cache_index = locale_index + 1;

        if (cache_index >= static_cast<int>(cache.size()))
            cache.resize(cache_index + 1);

        auto& data = cache[cache_index];
        if (!data)
        {
            data.reset(new std::vector<WorldPacket*>{});
            worker(*data, locale_index);
        }

        for (auto& elem : *data)
            p->GetSession()->send_packet(elem);
    }
};

template <typename Worker>
localize_packet<Worker> make_localize_packet(Worker w)
{
    return localize_packet<Worker>{w};
}

template <typename Worker>
localize_packets<Worker> make_localize_packets(Worker w)
{
    return localize_packets<Worker>{w};
}
}
}

#endif
