#include "time.hpp"
#include <chrono>

size_t get_time_ms()
{
    size_t milliseconds_since_epoch =
    std::chrono::duration_cast<std::chrono::milliseconds>
        (std::chrono::system_clock::now().time_since_epoch()).count();

    return milliseconds_since_epoch;
}

