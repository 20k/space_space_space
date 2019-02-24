#ifndef FIXED_CLOCK_HPP_INCLUDED
#define FIXED_CLOCK_HPP_INCLUDED

#include <SFML/System/Clock.hpp>

struct fixed_clock
{
    static inline thread_local size_t global_ticks = 0;
    static inline thread_local double time_between_ticks_ms = 16;
    static inline thread_local bool init = false;

    size_t start_tick = global_ticks;

    sf::Time getElapsedTime()
    {
        if(!init)
            throw std::runtime_error("Bad elapsed");

        return sf::milliseconds(time_between_ticks_ms * (global_ticks - start_tick));
    }

    sf::Time restart()
    {
        if(!init)
            throw std::runtime_error("Bad elapsed");

        auto backup = getElapsedTime();

        start_tick = global_ticks;

        return backup;
    }

    static void increment()
    {
        global_ticks++;
    }
};

#endif // FIXED_CLOCK_HPP_INCLUDED
