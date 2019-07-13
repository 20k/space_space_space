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
        T_BEAM, ///tractor beam
        SHIPYARD, ///external factory
        COUNT,
    };

    static inline std::vector<std::string> cpu_names
    {
        "IMPULSE",
        "S_DRIVE",
        "W_DRIVE",
        "SHIELD",
        "LASER",
        "SENSOR",
        "ARMOUR",
        "RADIATOR",
        "POWER",
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
        "CPU",
        "T_BEAM",
        "SHIPYARD",
    };
}

component get_component_default(component_type::type id, float scale);
const component_fixed_properties& get_component_fixed_props(component_type::type id, float scale);

#endif // DEFAULT_COMPONENTS_HPP_INCLUDED
