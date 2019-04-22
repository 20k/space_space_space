#ifndef DEFAULT_COMPONENTS_HPP_INCLUDED
#define DEFAULT_COMPONENTS_HPP_INCLUDED

struct component;
struct component_fixed_properties;

namespace component_type
{
    enum type
    {
        THRUSTER,
        WARP,
        SHIELDS,
        LASER,
        SENSOR,
        COMMS,
        ARMOUR,
        LIFE_SUPPORT,
        RADIATOR,
        POWER_GENERATOR,
        CREW,
        MISSILE_CORE,
        DESTRUCT,
        STORAGE_TANK,
        HEAT_BLOCK,
        COUNT,
    };
}

const component& get_component_default(component_type::type id);
const component_fixed_properties& get_component_fixed_props(component_type::type id);

#endif // DEFAULT_COMPONENTS_HPP_INCLUDED
