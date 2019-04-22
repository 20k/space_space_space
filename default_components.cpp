#include "default_components.hpp"
#include <array>
#include "ship_components.hpp"

component make_default(const std::string& long_name, const std::string& short_name, bool flows = false)
{
    component ret;
    ret.long_name = long_name;
    ret.short_name = short_name;
    ret.flows = flows;

    return ret;
}

std::array<component, component_type::COUNT> get_default_component_map()
{
    std::array<component, component_type::COUNT> ret;

    ret[component_type::THRUSTER] = make_default("Thruster", "THRST");
    ret[component_type::WARP] = make_default("Warp Drive", "WARP");
    ret[component_type::SHIELDS] = make_default("Shields", "SHLD");
    ret[component_type::LASER] = make_default("Laser", "LAS");
    ret[component_type::SENSOR] = make_default("Sensors", "SENS");
    ret[component_type::COMMS] = make_default("Communications", "COMM");
    ret[component_type::ARMOUR] = make_default("Armour", "ARMR");
    ret[component_type::LIFE_SUPPORT] = make_default("Life Support", "LS");
    ret[component_type::RADIATOR] = make_default("Radiator", "RAD");
    ret[component_type::POWER_GENERATOR] = make_default("Power Generator", "PWR");
    ret[component_type::CREW] = make_default("Crew", "CRW");
    ret[component_type::MISSILE_CORE] = make_default("Missile Core", "MCRE");
    ret[component_type::DESTRUCT] = make_default("Self Destruct", "SDST");
    ret[component_type::STORAGE_TANK] = make_default("Storage Tank", "STNK");
    ret[component_type::HEAT_BLOCK] = make_default("Heat Block", "HBLK");

    return ret;
}

std::array<component_fixed_properties, component_type::COUNT>> get_default_fixed_component_map()
{
    std::array<component_fixed_properties, component_type::COUNT> ret;

    return ret;
}

const component& get_component_default(component_type::type id)
{
    static std::array<component, component_type::COUNT> built = get_default_component_map();

    return built[id];
}

const component_fixed_properties& get_component_fixed_props(component_type::type id)
{
    static std::array<component, component_type::COUNT> built = get_default_fixed_component_map();

    return built[id];
}
