#ifndef MATERIAL_INFO_HPP_INCLUDED
#define MATERIAL_INFO_HPP_INCLUDED

#include <networking/serialisable.hpp>

struct material_fixed_properties;

namespace material_info
{
    enum material_type
    {
        HYDROGEN,
        IRON,
        COPPER,
        H2O,
        COUNT,
    };

    static std::vector<std::string> long_names
    {
        "Hydrogen",
        "Iron",
        "Copper",
        "H2O",
    };

    material_fixed_properties fetch(material_type type);
}

///making the executive decision that most of this won't be physically based
///could also use non real materials but ehh
struct material_fixed_properties : serialisable
{
    ///heat per unit volume
    float specific_heat = 0;
    float reflectivity = 0;

    virtual void serialise(nlohmann::json& data, bool encode)
    {
        DO_SERIALISE(specific_heat);
        DO_SERIALISE(reflectivity);
    }
};

struct material_dynamic_properties : serialisable
{
    float total_heat = 0;
    float volume = 0;

    virtual void serialise(nlohmann::json& data, bool encode)
    {
        DO_SERIALISE(total_heat);
        DO_SERIALISE(volume);
    }
};

struct material : serialisable
{
    material_dynamic_properties dynamic_desc;
    material_info::material_type type = material_info::COUNT;

    virtual void serialise(nlohmann::json& data, bool encode)
    {
        DO_SERIALISE(dynamic_desc);
        DO_SERIALISE(type);
    }
};

#endif // MATERIAL_INFO_HPP_INCLUDED