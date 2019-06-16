#include "material_info.hpp"
#include "ship_components.hpp"

material_fixed_properties material_info::fetch(material_info::material_type type)
{
    material_fixed_properties fixed;

    if(type == material_info::HYDROGEN)
    {
        fixed.specific_heat = 14;
        fixed.reflectivity = 0.5;
        fixed.melting_point = 1;
        fixed.specific_explosiveness = 1;
        fixed.density = 0.01;
    }

    if(type == material_info::IRON)
    {
        fixed.specific_heat = 0.4;
        fixed.reflectivity = 0.5;
        fixed.melting_point = 1811;
        fixed.density = 2;
    }

    if(type == material_info::COPPER)
    {
        fixed.specific_heat = 0.385;
        fixed.reflectivity = 0.6;
        fixed.melting_point = 1358;
        fixed.density = 2.1;
    }

    if(type == material_info::H2O)
    {
        fixed.specific_heat = 4;
        fixed.reflectivity = 0.8;
        fixed.melting_point = 273.15;
        fixed.density = 1;
    }

    if(type == material_info::HTX)
    {
        fixed.specific_heat = 2;
        fixed.reflectivity = 0.1;
        fixed.melting_point = 1523;
        fixed.specific_explosiveness = 5000;
        fixed.density = 0.5;
    }

    if(type == material_info::LITHIUM)
    {
        fixed.specific_heat = 3.58;
        fixed.reflectivity = 0.8;
        fixed.melting_point = 453;
        fixed.density = 2;
    }

    if(type == material_info::SILICON)
    {
        fixed.specific_heat = 0.71;
        fixed.reflectivity = 0.9;
        fixed.melting_point = 1414;
        fixed.density = 2.3;
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

        if(fixed.melting_point == 0)
            fixed.melting_point = them_fixed.melting_point;
        else
            fixed.melting_point = std::min(fixed.melting_point, them_fixed.melting_point);

        fixed.reflectivity += m.dynamic_desc.volume * them_fixed.reflectivity;
        fixed.specific_heat += m.dynamic_desc.volume * them_fixed.specific_heat;
        fixed.specific_explosiveness += m.dynamic_desc.volume * them_fixed.specific_explosiveness;
        fixed.density += m.dynamic_desc.volume * them_fixed.density;
    }

    if(in.size() == 0)
        return {dyn, fixed};

    if(dyn.volume <= 0.00001)
        return {dyn, fixed};

    fixed.reflectivity = fixed.reflectivity / dyn.volume;
    fixed.specific_heat = fixed.specific_heat / dyn.volume;
    fixed.specific_explosiveness = fixed.specific_explosiveness / dyn.volume;
    fixed.density = fixed.density / dyn.volume;

    return {dyn, fixed};
}

bool is_equivalent_material(std::vector<material> m_1, std::vector<material> m_2)
{
    if(m_1.size() != m_2.size())
        return false;

    auto sorter =
    [](const material& one, const material& two)
    {
        return one.type < two.type;
    };

    std::sort(m_1.begin(), m_1.end(), sorter);
    std::sort(m_2.begin(), m_2.end(), sorter);

    float one_vol = 0;
    float two_vol = 0;

    for(material& i : m_1)
    {
        one_vol += i.dynamic_desc.volume;
    }

    for(material& i : m_2)
    {
        two_vol += i.dynamic_desc.volume;
    }

    for(int i=0; i < (int)m_1.size(); i++)
    {
        if(m_1[i].type != m_2[i].type)
            return false;
    }

    if(one_vol < 0.0001 || two_vol < 0.0001)
        return true;

    for(int i=0; i < (int)m_1.size(); i++)
    {
        float frac_1 = m_1[i].dynamic_desc.volume / one_vol;
        float frac_2 = m_2[i].dynamic_desc.volume / two_vol;

        if(!approx_equal(frac_1, frac_2, 0.001))
            return false;
    }

    return true;
}

bool is_equivalent_material(const component& c1, const component& c2)
{
    if(c1.flows != c2.flows)
        return false;

    if(c1.phase != c2.phase)
        return false;

    return is_equivalent_material(c1.composition, c2.composition);
}

void material_deduplicate(std::vector<std::vector<material>>& in)
{
    for(int i=0; i < (int)in.size(); i++)
    {
        for(int j=i + 1; j < (int)in.size(); j++)
        {
            if(is_equivalent_material(in[i], in[j]))
            {
                material_merge(in[i], in[j]);

                in.erase(in.begin() + j);
                j--;
                continue;
            }
        }
    }
}

void material_merge(std::vector<material>& m_1, std::vector<material> m_2)
{
    auto sorter =
    [](const material& one, const material& two)
    {
        return one.type < two.type;
    };

    std::sort(m_1.begin(), m_1.end(), sorter);
    std::sort(m_2.begin(), m_2.end(), sorter);

    assert(m_1.size() == m_2.size());

    for(int i=0; i < (int)m_1.size(); i++)
    {
        assert(m_1[i].type == m_2[i].type);

        m_1[i].dynamic_desc.volume += m_2[i].dynamic_desc.volume;
    }
}

bool material_satisfies(const std::vector<std::vector<material>>& requirements, const std::vector<std::vector<material>>& available)
{
    std::vector<int> satisfied;
    satisfied.resize(requirements.size());

    for(auto& available_type : available)
    {
        for(int i=0; i < (int)requirements.size(); i++)
        {
            if(satisfied[i])
                continue;

            auto& required_type = requirements[i];

            if(is_equivalent_material(available_type, required_type))
            {
                float vol_available = material_volume(available_type);
                float vol_required = material_volume(required_type);

                if(vol_available < vol_required)
                    return false;

                satisfied[i] = 1;
            }
        }
    }

    for(auto& i : satisfied)
    {
        if(!i)
            return false;
    }

    return true;
}

void material_deplete(std::vector<material>& ms, float amount)
{
    float vol = material_volume(ms);

    if(vol <= 0.000001)
        return;

    amount = clamp(amount, 0, vol);

    for(material& m : ms)
    {
        float frac = m.dynamic_desc.volume / vol;

        m.dynamic_desc.volume -= frac * amount;
    }
}

std::vector<float> material_partial_deplete(std::vector<material>& store, std::vector<std::vector<material>>& deplete)
{
    std::vector<float> ret;
    ret.resize(deplete.size());

    for(int kk=0; kk < (int)deplete.size(); kk++)
    {
        auto& i = deplete[kk];

        if(is_equivalent_material(store, i))
        {
            float vol_store = material_volume(store);
            float vol_deplete = material_volume(i);

            vol_deplete = clamp(vol_deplete, 0, vol_store);

            material_deplete(store, vol_deplete);
            material_deplete(i, vol_deplete);

            ret[kk] += vol_deplete;
        }
    }

    return ret;
}

float material_volume(const std::vector<material>& m)
{
    float vol = 0;

    for(const material& ms : m)
    {
        vol += ms.dynamic_desc.volume;
    }

    return vol;
}
