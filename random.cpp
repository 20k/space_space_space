#include "random.hpp"
#include <random>

uint32_t get_random_value()
{
    static std::minstd_rand0 rng;
    static bool init = false;

    if(!init)
    {
        rng.seed(0);
        init = true;
    }

    return rng();
}
