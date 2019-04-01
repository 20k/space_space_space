#ifndef COMMON_RENDERABLE_HPP_INCLUDED
#define COMMON_RENDERABLE_HPP_INCLUDED

#include <networking/serialisable.hpp>
#include <vec/vec.hpp>
#include "fixed_clock.hpp"
#include "entity.hpp"

struct uncertain
{
    fixed_clock elapsed;
    fixed_clock received_signal;

    bool begin_cleanup = false;
    bool is_unknown = false;

    bool bad(vec2f player_pos, vec2f my_pos)
    {
        return elapsed.getElapsedTime().asMilliseconds() > 500 && begin_cleanup;

        //return elapsed.getElapsedTime().asSeconds() > 1 && (player_pos - my_pos).length() < 10;
    }

    void unknown(bool state)
    {
        is_unknown = state;
    }

    void no_signal()
    {
        if(received_signal.getElapsedTime().asMicroseconds() / 1000. < 1000)
            return;

        if(begin_cleanup)
            return;

        begin_cleanup = true;
        elapsed.restart();
    }

    void got_signal()
    {
        //is_unknown = false;

        if(!begin_cleanup)
            return;

        begin_cleanup = false;
        received_signal.restart();
    }
};

struct common_renderable : serialisable, uncertain
{
    ///0 = low detail, 1 = high
    int type = 0;

    client_renderable r;
    vec2f velocity = {0,0};

    SERIALISE_SIGNATURE()
    {
        DO_SERIALISE(type);
        DO_SERIALISE(r);
        DO_SERIALISE(velocity);
        DO_SERIALISE(is_unknown);
    }
};

#endif // COMMON_RENDERABLE_HPP_INCLUDED
