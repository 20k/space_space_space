#include "material_info.hpp"

material_fixed_properties material_info::fetch(material_info::material_type type)
{
    material_fixed_properties fixed;

    if(type == material_info::HYDROGEN)
    {
        fixed.specific_heat = 1;
        fixed.reflectivity = 0.5;
    }

    if(type == material_info::IRON)
    {
        fixed.specific_heat = 0.2;
        fixed.reflectivity = 0.5;
    }

    if(type == material_info::COPPER)
    {
        fixed.specific_heat = 0.5;
        fixed.reflectivity = 0.6;
    }

    if(type == material_info::H2O)
    {
        fixed.specific_heat = 0.4;
        fixed.reflectivity = 0.8;
    }

    return fixed;
}
