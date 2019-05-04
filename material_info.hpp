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
        HTX,
        COMPOSITE,
        LITHIUM,
        SILICON,
        COUNT,
    };

    static std::vector<std::string> long_names
    {
        "Hydrogen",
        "Iron",
        "Copper",
        "H2O",
        "HTX",
        "Composite",
        "Lithium",
        "Silicon",
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
    float melting_point = 0;
    float specific_explosiveness = 0; ///damage per volume
    float density = 0;

    SERIALISE_SIGNATURE()
    {
        DO_SERIALISE(specific_heat);
        DO_SERIALISE(reflectivity);
        DO_SERIALISE(melting_point);
        DO_SERIALISE(specific_explosiveness);
        DO_SERIALISE(density);
    }
};

struct material_dynamic_properties : serialisable
{
    //float total_heat = 0;
    float volume = 0;

    SERIALISE_SIGNATURE()
    {
        //DO_SERIALISE(total_heat);
        DO_SERIALISE(volume);
    }
};

struct material : serialisable
{
    material_dynamic_properties dynamic_desc;
    material_info::material_type type = material_info::COUNT;

    SERIALISE_SIGNATURE()
    {
        DO_SERIALISE(dynamic_desc);
        DO_SERIALISE(type);
    }
};

std::pair<material_dynamic_properties, material_fixed_properties> get_material_composite(const std::vector<material>& in);

struct component;

bool is_equivalent_material(std::vector<material> m_1, std::vector<material> m_2);
bool is_equivalent_material(const component& c1, const component& c2);

void material_deduplicate(std::vector<std::vector<material>>& in);
void material_merge(std::vector<material>& into, std::vector<material> old);
bool material_satisfies(const std::vector<std::vector<material>>& requirements, const std::vector<std::vector<material>>& available); ///must be deduplicated

float material_volume(const std::vector<material>& m);

#endif // MATERIAL_INFO_HPP_INCLUDED
