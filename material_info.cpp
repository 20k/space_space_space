#include "material_info.hpp"

material_fixed_properties material_info::fetch(material_info::material_type type)
{
    material_fixed_properties fixed;

    if(type == material_info::HYDROGEN)
    {
        fixed.specific_heat = 1;
        fixed.reflectivity = 0.5;
        fixed.melting_point = 1;
    }

    if(type == material_info::IRON)
    {
        fixed.specific_heat = 0.2;
        fixed.reflectivity = 0.5;
        fixed.melting_point = 1811;
    }

    if(type == material_info::COPPER)
    {
        fixed.specific_heat = 0.5;
        fixed.reflectivity = 0.6;
        fixed.melting_point = 1358;
    }

    if(type == material_info::H2O)
    {
        fixed.specific_heat = 0.4;
        fixed.reflectivity = 0.8;
        fixed.melting_point = 273.15;
    }

    return fixed;
}

std::pair<material_dynamic_properties, material_fixed_properties> get_material_composite(const std::vector<material>& in)
{
    material_dynamic_properties dyn;
    material_fixed_properties fixed;

    for(const material& m : in)
    {
        dyn.volume += m.dynamic_desc.volume;

        auto them_fixed = material_info::fetch(m.type);

        fixed.reflectivity += them_fixed.reflectivity;

        if(fixed.melting_point == 0)
            fixed.melting_point = them_fixed.melting_point;
        else
            fixed.melting_point = std::min(fixed.melting_point, them_fixed.melting_point);

        fixed.specific_heat += m.dynamic_desc.volume * them_fixed.specific_heat;
    }

    if(in.size() == 0)
        return {dyn, fixed};

    fixed.specific_heat = fixed.specific_heat / dyn.volume;

    return {dyn, fixed};
}
