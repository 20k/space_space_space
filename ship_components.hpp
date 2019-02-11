#ifndef SHIP_COMPONENTS_HPP_INCLUDED
#define SHIP_COMPONENTS_HPP_INCLUDED

#include <vector>

namespace component_info
{
    enum does_type
    {
        THRUST,
        WARP,
        SHIELDS,
        WEAPONS,
        SENSORS,
        COMMS,
        SYSTEM, ///repair
        ARMOUR,
        LIFE_SUPPORT,
        COOLANT,
        COUNT
    };
}

struct does
{
    double capacity = 0;
    double recharge = 0;

    component_info::does_type type = component_info::COUNT;
};

struct component
{
    std::vector<does> info;
    double hp = 1;

    double satisfied_percentage(double dt_s, const std::vector<double>& res);
    void apply(double efficiency, double dt_s, std::vector<double>& res);
};

struct ship
{
    std::vector<component> components;

    std::vector<double> resource_status;

    ship();

    void tick(double dt_s);
};

#endif // SHIP_COMPONENTS_HPP_INCLUDED
