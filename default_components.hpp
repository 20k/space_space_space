#ifndef DEFAULT_COMPONENTS_HPP_INCLUDED
#define DEFAULT_COMPONENTS_HPP_INCLUDED

struct component;
struct component_fixed_properties;

#include <vector>
#include <string>

#define POWER_TO_HEAT 1

namespace component_type
{
    enum type
    {
        THRUSTER,
        S_DRIVE,
        W_DRIVE,
        SHIELDS,
        LASER,
        SENSOR,
        //COMMS,
        ARMOUR,
        //LIFE_SUPPORT,
        RADIATOR,
        POWER_GENERATOR,
        //CREW,
        MISSILE_CORE,
        DESTRUCT,
        CARGO_STORAGE,
        STORAGE_TANK,
        STORAGE_TANK_HS,
        HEAT_BLOCK,
        MATERIAL,
        COMPONENT_LAUNCHER,
        MINING_LASER,
        REFINERY,
        FACTORY,
        RADAR,
        CPU,
        COUNT,
    };

    static inline std::vector<std::string> cpu_names
    {
        "IMPULSE",
        "S_D",
        "W_D",
        "SHIELD",
        "LASER",
        "SENSOR",
        "ARMOUR",
        "RADIATOR",
        "POWER",
        "M_CRE",
        "DESTRUCT",
        "CARGO",
        "TANK",
        "TANK_HS",
        "HEAT_BLOCK",
        "MATERIAL",
        "LAUNCHER",
        "M_LASER",
        "REFINERY",
        "FACTORY",
        "RADAR",
        "CPU"
    };
}

component get_component_default(component_type::type id, float scale);
const component_fixed_properties& get_component_fixed_props(component_type::type id, float scale);

#endif // DEFAULT_COMPONENTS_HPP_INCLUDED
