#include "ship_components.hpp"
#include <algorithm>
#include <assert.h>
#include <math.h>
#include "format.hpp"
#include <iostream>
#include <imgui/imgui.h>
#include "imGuiX.h"
#include <SFML/Graphics.hpp>
#include "radar_field.hpp"
#include "random.hpp"
#include <set>
#include "aoe_damage.hpp"
#include "player.hpp"
#include "playspace.hpp"
#include "colours.hpp"
#include "script_execution.hpp"
#include "ui_util.hpp"
#include "material_info.hpp"
#include "design_editor.hpp"
#include "data_model.hpp"

double apply_to_does(double amount, does_dynamic& d, const does_fixed& fix);

ship::ship()
{
    original_blueprint = std::make_shared<blueprint>();

    last_sat_percentage.resize(component_info::COUNT);

    for(auto& i : last_sat_percentage)
        i = 1;

    vec2f dim = {2, 1};

    r.init_rectangular(dim);

    for(auto& i : r.vert_cols)
    {
        i = {0.5, 1, 0.5, 1};
    }

    r.scale = 2;

    r.vert_cols[2] = {1, 0.2, 0.2, 1};

    phys_drag = true;

    data_track.resize(component_info::COUNT);

    radar_frequency_composition.push_back(2000);
    radar_intensity_composition.push_back(1);
    accumulates_samples = true;
}

void component_fixed_properties::add(component_info::does_type type, double amount)
{
    for(does_fixed& d : d_info)
    {
        if(d.type == component_info::HP && type == component_info::HP)
            assert(false);
    }

    does_fixed d;
    d.type = type;
    d.recharge = amount;

    d_info.push_back(d);
}

void component_fixed_properties::add(component_info::does_type type, double amount, double capacity)
{
    for(does_fixed& d : d_info)
    {
        if(d.type == component_info::HP && type == component_info::HP)
            assert(false);
    }

    does_fixed d;
    d.type = type;
    d.recharge = amount;
    d.capacity = capacity;

    d_info.push_back(d);
}

void component_fixed_properties::add_unconditional(component_info::does_type type, double amount)
{
    does_fixed d;
    d.type = type;
    d.recharge_unconditional = amount;

    d_info.push_back(d);
}

void component_fixed_properties::add(tag_info::tag_type type)
{
    tag t;
    t.type = type;

    tags.push_back(t);
}

void component_fixed_properties::add_on_use(component_info::does_type type, double amount, double time_between_use_s)
{
    does_fixed d;
    d.type = type;
    d.capacity = amount;
    d.time_between_use_s = time_between_use_s;

    d_activate_requirements.push_back(d);
}

void component_fixed_properties::set_heat(double heat)
{
    heat_produced_at_full_usage = heat;
}

void component_fixed_properties::set_heat_scales_by_production(bool status, component_info::does_type primary)
{
    production_heat_scales = status;
    primary_type = primary;
}

void component_fixed_properties::set_no_drain_on_full_production()
{
    no_drain_on_full_production = true;
}

void component_fixed_properties::set_complex_no_drain_on_full_production()
{
    complex_no_drain_on_full_production = true;
}

/*double component::satisfied_percentage(double dt_s, const std::vector<double>& res)
{
    assert(res.size() == component_info::COUNT);

    std::vector<double> needed;
    needed.resize(component_info::COUNT);

    double hp = get_held()[component_info::HP];
    double max_hp = get_capacity()[component_info::HP];

    assert(max_hp > 0);

    for(does& d : info)
    {
        if(d.recharge >= 0)
            continue;

        //needed[d.type] += fabs(d.recharge) * dt_s;
        needed[d.type] += fabs(d.recharge) * dt_s * (hp / max_hp);
    }

    double satisfied = 1;

    for(int i=0; i < (int)res.size(); i++)
    {
        if(needed[i] < 0.000001)
            continue;

        if(res[i] < 0.000001)
        {
            satisfied = 0;
            continue;
        }

        satisfied = std::min(satisfied, res[i] / needed[i]);
        satisfied = std::min(satisfied, hp / max_hp);
    }

    return satisfied;
}*/

#if 0
void component::apply(const std::vector<double>& all_sat, double dt_s, std::vector<double>& res)
{
    assert(res.size() == component_info::COUNT);

    double hp = get_held()[component_info::HP];
    double max_hp = get_capacity()[component_info::HP];

    assert(max_hp > 0);

    /*for(does& d : info)
    {
        res[d.type] += d.recharge * efficiency * dt_s * (hp / max_hp);
    }*/

    double min_sat = 1;

    for(does& d : info)
    {
        min_sat = std::min(all_sat[d.type], min_sat);
    }

    for(does& d : info)
    {
        res[d.type] += d.recharge * min_sat * dt_s * (hp / max_hp);
    }

    last_sat = min_sat;
}
#endif // 0

std::vector<double> component::get_theoretical_produced()
{
    std::vector<double> needed;
    needed.resize(component_info::COUNT);

    double hp = get_held()[component_info::HP];
    double max_hp = get_capacity()[component_info::HP];

    assert(max_hp > 0);

    const component_fixed_properties& fixed = get_fixed_props();

    for(const does_fixed& unscaled : fixed.d_info)
    {
        does_fixed d = unscaled.scale(current_scale);

        double mod = last_sat;

        if(d.recharge < 0 || d.recharge_unconditional < 0)
            continue;

        float hacky_lpf = last_production_frac;

        if(fixed.complex_no_drain_on_full_production)
            hacky_lpf = 1;

        needed[d.type] += d.recharge * (hp / max_hp) * mod * activation_level * hacky_lpf + d.recharge_unconditional;
    }

    return needed;
}

std::vector<double> component::get_theoretical_consumed()
{
    std::vector<double> needed;
    needed.resize(component_info::COUNT);

    double hp = get_held()[component_info::HP];
    double max_hp = get_capacity()[component_info::HP];

    assert(max_hp > 0);

    const component_fixed_properties& fixed = get_fixed_props();

    for(const does_fixed& unscaled : fixed.d_info)
    {
        does_fixed d = unscaled.scale(current_scale);

        if(d.recharge > 0 || d.recharge_unconditional > 0)
            continue;

        needed[d.type] += fabs(d.recharge) * (hp / max_hp) * activation_level * last_production_frac + fabs(d.recharge_unconditional);
    }

    return needed;
}

std::vector<double> component::get_produced()
{
    std::vector<double> needed;
    needed.resize(component_info::COUNT);

    double hp = get_held()[component_info::HP];
    double max_hp = get_capacity()[component_info::HP];

    assert(max_hp > 0);

    const component_fixed_properties& fixed = get_fixed_props();

    for(const does_fixed& unscaled : fixed.d_info)
    {
        does_fixed d = unscaled.scale(current_scale);

        double mod = last_sat;

        if(d.recharge < 0 || d.recharge_unconditional < 0)
            continue;

        needed[d.type] += d.recharge * (hp / max_hp) * mod * last_production_frac * activation_level + d.recharge_unconditional;
    }

    return needed;
}

std::vector<double> component::get_needed()
{
    std::vector<double> needed;
    needed.resize(component_info::COUNT);

    double hp = get_held()[component_info::HP];
    double max_hp = get_capacity()[component_info::HP];

    assert(max_hp > 0);

    const component_fixed_properties& fixed = get_fixed_props();

    for(const does_fixed& unscaled : fixed.d_info)
    {
        does_fixed d = unscaled.scale(current_scale);

        double mod = 1;

        if(d.recharge > 0)
        {
            mod = last_sat;
        }

        needed[d.type] += d.recharge * (hp / max_hp) * mod  * last_production_frac * activation_level + d.recharge_unconditional;
    }

    return needed;
}

std::vector<double> component::get_capacity()
{
    std::vector<double> needed;
    needed.resize(component_info::COUNT);

    const component_fixed_properties& fixed = get_fixed_props();

    for(const does_fixed& unscaled : fixed.d_info)
    {
        does_fixed d = unscaled.scale(current_scale);

        needed[d.type] += d.capacity;
    }

    return needed;
}

std::vector<double> component::get_held()
{
    std::vector<double> needed;
    needed.resize(component_info::COUNT);

    for(const does_dynamic& d : dyn_info)
    {
        needed[d.type] += d.held;
    }

    return needed;
}

void component::deplete_me(const std::vector<double>& diff, const std::vector<double>& free_storage, const std::vector<double>& used_storage)
{
    const component_fixed_properties& fixed = get_fixed_props();

    for(int i=0; i < (int)dyn_info.size(); i++)
    {
        does_dynamic& does_dyn = dyn_info[i];
        const does_fixed& does_fix = fixed.d_info[i].scale(current_scale);

        double my_held = does_dyn.held;
        double my_cap = does_fix.capacity;

        if(diff[does_fix.type] > 0)
        {
            double my_free = my_cap - my_held;

            if(my_free <= 0)
                continue;

            double free_of_type = free_storage[does_fix.type];

            if(fabs(free_of_type) <= 0.00001)
                continue;

            double deplete_frac = my_free / free_of_type;

            double to_change = deplete_frac * diff[does_fix.type];

            double next = clamp(my_held + to_change, 0, my_cap);

            does_dyn.held = next;
        }
        else
        {
            double my_used = does_dyn.held;

            if(my_used <= 0)
                continue;

            double used_of_type = used_storage[does_fix.type];

            if(fabs(used_of_type) <= 0.00001)
                continue;

            double deplete_frac = my_used / used_of_type;

            double to_change = deplete_frac * diff[does_fix.type];

            double next = clamp(my_held + to_change, 0, my_cap);

            does_dyn.held = next;
        }
    }
}

bool component::can_use(const std::vector<double>& res)
{
    //for(does& d : activate_requirements)

    const component_fixed_properties& fixed = get_fixed_props();

    for(int i=0; i < (int)dyn_activate_requirements.size(); i++)
    {
        does_dynamic& does_dyn = dyn_activate_requirements[i];
        const does_fixed& does_fix = fixed.d_activate_requirements[i].scale(current_scale);

        if(does_fix.capacity >= 0)
            continue;

        if(fabs(does_fix.capacity) > res[does_fix.type])
            return false;

        if(does_dyn.last_use_s < does_fix.time_between_use_s)
            return false;
    }

    if(has(component_info::HP))
    {
        does_dynamic& d = get_dynamic(component_info::HP);
        const does_fixed& dfix = get_fixed(component_info::HP);

        assert(dfix.capacity > 0);

        if((d.held / dfix.capacity) < 0.2)
            return false;
    }

    if(activation_level < 0.2)
        return false;

    return true;
}

void component::use(std::vector<double>& res)
{
    const component_fixed_properties& fixed = get_fixed_props();

    for(const does_fixed& unscaled : fixed.d_activate_requirements)
    {
        does_fixed d = unscaled.scale(current_scale);

        res[d.type] += d.capacity;
    }

    for(does_dynamic& d : dyn_activate_requirements)
    {
        d.last_use_s = 0;
    }

    try_use = false;
    force_use = false;
}

float component::get_use_heat()
{
    float heat = 0;

    const component_fixed_properties& fixed = get_fixed_props();

    for(const does_fixed& unscaled : fixed.d_activate_requirements)
    {
        does_fixed d = scale(unscaled);

        if(d.type != component_info::POWER && d.type != component_info::CAPACITOR)
            continue;

        if(d.type == component_info::POWER)
            heat += fabs(d.capacity) * POWER_TO_HEAT * 10;

        if(d.type == component_info::CAPACITOR)
            heat += fabs(d.capacity) * POWER_TO_HEAT * 100;
    }

    return heat;
}

float component::get_my_volume() const
{
    float amount = 0;

    for(const material& m : composition)
    {
        amount += m.dynamic_desc.volume;
    }

    return amount;
}

/*void component::normalise_volume()
{
    float real = get_my_volume();

    if(real < 0.0001)
        return;

    real *= MAX_COMPONENT_SHIP_AMOUNT;

    for(material& m : composition)
    {
        m.dynamic_desc.volume /= real;
    }

    for_each_stored([&](auto& c)
    {
        for(material& m : c.composition)
        {
            m.dynamic_desc.volume /= real;
        }
    });

    internal_volume /= real;
}*/

float component::get_stored_volume() const
{
    float vol = 0;

    //for(const ship& c : stored)

    for_each_stored([&](auto& c)
    {
        vol += c.get_my_volume();
    });

    return vol;
}

/*float component::drain_volume(float vol)
{
    float to_drain = clamp(vol, 0, get_my_volume());

    my_volume -= to_drain;

    return to_drain;
}*/

float component::get_my_temperature()
{
    return my_temperature;
}

float component::get_my_contained_heat()
{
    float final_temp = 0;

    for(material& m : composition)
    {
        final_temp += my_temperature * m.dynamic_desc.volume * fetch(m.type).specific_heat;
    }

    return final_temp;
}

float component::get_stored_temperature()
{
    float temp = 0;
    float total_volume = 0;

    for_each_stored([&](component& c)
    {
        temp += c.get_my_temperature() * c.get_my_volume();
        total_volume += c.get_my_volume();
    });

    if(total_volume <= 0.0001)
        return 0;

    return temp / total_volume;
}

float component::get_stored_heat_capacity()
{
    float temp = 0;

    for_each_stored([&](component& c)
    {
        temp += c.get_my_contained_heat();
    });

    return temp;
}

float component::get_my_heat_x_volume()
{
    float total_heat_capacity = 0;

    for(material& m : composition)
    {
        total_heat_capacity += m.dynamic_desc.volume * material_info::fetch(m.type).specific_heat;
    }

    return total_heat_capacity;
}

float component::get_stored_heat_x_volume()
{
    float temp = 0;

    for_each_stored([&](component& c)
    {
        temp += c.get_my_heat_x_volume();
    });

    return temp;
}

float component::get_internal_volume()
{
    const component_fixed_properties& fixed = get_fixed_props();

    return fixed.get_internal_volume(current_scale);
}

void component::add_heat_to_me(float heat)
{
    float total_heat = 0;

    for(material& m : composition)
    {
        total_heat += material_info::fetch(m.type).specific_heat * m.dynamic_desc.volume;
    }

    if(total_heat < 0.0001)
        return;

    my_temperature += heat / total_heat;
}

void component::remove_heat_from_me(float heat)
{
    float total_heat = 0;

    for(material& m : composition)
    {
        total_heat += material_info::fetch(m.type).specific_heat * m.dynamic_desc.volume;
    }

    if(total_heat < 0.0001)
        return;

    my_temperature -= heat / total_heat;

    if(my_temperature < 0)
        my_temperature = 0;
}

void component::add_heat_to_stored(float heat)
{
    float stored_num = stored.size();

    for_each_stored([&](component& c)
    {
        c.add_heat_to_me(heat / stored_num);
    });
}

void component::remove_heat_from_stored(float heat)
{
    float stored_num = stored.size();

    for_each_stored([&](component& c)
    {
        c.remove_heat_from_me(heat / stored_num);
    });
}

bool component::can_store(const component& c)
{
    const component_fixed_properties& fixed = get_fixed_props();

    if(fixed.get_internal_volume(current_scale) <= 0)
        return false;

    float storeable = fixed.get_internal_volume(current_scale) - get_stored_volume();

    return (storeable - c.get_my_volume()) >= 0;
}

bool component::can_store(const ship& s)
{
    const component_fixed_properties& fixed = get_fixed_props();

    if(fixed.get_internal_volume(current_scale) <= 0)
        return false;

    float storeable = fixed.get_internal_volume(current_scale) - get_stored_volume();

    float to_store_volume = 0;

    for(const component& c : s.components)
    {
        to_store_volume += c.get_my_volume();
    }

    return (storeable - to_store_volume) >= 0;
}

struct fp_fixer
{
    component* to_fix = nullptr;

    void fix(component& c)
    {
        to_fix = &c;
    }

    ~fp_fixer()
    {
        if(!to_fix)
            return;

        double max_internal_store = to_fix->get_internal_volume();

        double cur_internal = to_fix->get_internal_volume();

        if(cur_internal <= max_internal_store)
            return;

        double to_remove = cur_internal - max_internal_store;

        to_fix->for_each_stored
        ([&](component& c)
        {
            if(c.base_id != component_type::MATERIAL)
                return;

            c.remove_composition(to_remove);
        });
    }
};

void component::store(const component& c, bool force_and_fixup)
{
    if(!force_and_fixup && !can_store(c))
        throw std::runtime_error("Cannot store component");

    fp_fixer fix;

    if(force_and_fixup)
        fix.fix(*this);

    for(ship& existing : stored)
    {
        ///TODO: BIT HACKY THIS
        /*if(existing.components.size() != 1)
            continue;*/

        if(existing.is_ship || existing.is_build_holder)
            continue;

        ///TODO: DOES NOT CORRECTLY HANDLE TEMPERATURE
        for(component& existing_component : existing.components)
        {
            if(is_equivalent_material(existing_component, c))
            {
                for(const material& m : c.composition)
                {
                    existing_component.add_composition(m);
                }

                existing_component.bad_time = fixed_clock();

                return;
            }
        }
    }

    ship nship;
    nship.add(c);

    stored.push_back(nship);
}

void component::store(const ship& s)
{
    if(!can_store(s))
        throw std::runtime_error("Cannot store ship");

    if(!s.is_ship)
    {
        for(const component& c : s.components)
        {
            store(c);
        }

        return;
    }

    ship nship = s;

    stored.push_back(nship);
}

bool component::is_storage()
{
    const component_fixed_properties& fixed = get_fixed_props();

    return fixed.get_internal_volume(current_scale) > 0;
}

bool component::should_flow()
{
    return flows;
}

float component::get_hp_frac()
{
    double hp_frac = 1;

    if(has(component_info::HP) && get_fixed(component_info::HP).capacity > 0.00001)
    {
        hp_frac = get_dynamic(component_info::HP).held / get_fixed(component_info::HP).capacity;
    }

    return hp_frac;
}

float component::get_operating_efficiency()
{
    return std::min(last_sat, last_production_frac) * activation_level * get_hp_frac();
}

/*void component::scale(float size)
{
    current_scale = size;

    for(does& d : info)
    {
        d.capacity *= size;
        d.held *= size;
        d.recharge *= size;
        d.time_between_use_s *= size;
    }

    for(does& d : activate_requirements)
    {
        d.capacity *= size;
        d.held *= size;
        d.recharge *= size;
        d.time_between_use_s *= size;
    }

    for(material& m : composition)
    {
        m.dynamic_desc.volume *= size;
    }

    internal_volume *= size;
    heat_produced_at_full_usage *= size;

    for_each_stored([&](component& c)
    {
        c.scale(size);
    });
}*/

void component::scale(float size)
{
    current_scale *= size;

    for(does_dynamic& d : dyn_info)
    {
        d.held *= size;
    }

    for(does_dynamic& d : dyn_activate_requirements)
    {
        d.held *= size;
    }

    for(material& m : composition)
    {
        m.dynamic_desc.volume *= size;
    }

    for_each_stored([&](component& c)
                    {
                        //if(!c.flows)
                        //    return;

                        c.scale(size);
                    });
}

std::optional<ship> component::remove_first_stored_item()
{
    if(stored.size() == 0)
        return std::nullopt;

    ship first = stored[0];

    stored.erase(stored.begin());

    return first;
}

void component::add_composition(material_info::material_type type, double volume)
{
    for(material& m : composition)
    {
        if(m.type == type)
        {
            assert(false);
        }
    }

    material new_mat;
    new_mat.type = type;
    new_mat.dynamic_desc.volume = volume;

    //my_volume += volume;

    composition.push_back(new_mat);
}

void component::add_composition(const material& m)
{
    for(material& m_old : composition)
    {
        if(m_old.type == m.type)
        {
            m_old.dynamic_desc.volume += m.dynamic_desc.volume;
            return;
        }
    }

    assert(false);
}

void component::add_composition_ratio(const std::vector<material_info::material_type>& type, const std::vector<double>& volume)
{
    if(type.size() != volume.size())
        throw std::runtime_error("Bad Composition, hard error?");

    double total_volume_in = 0;

    for(auto& i : volume)
    {
        total_volume_in += i;
    }

    if(total_volume_in <= 0.00001)
        throw std::runtime_error("Bad volume in, composition");

    const component_fixed_properties& fixed = get_fixed_props();

    for(int i=0; i < (int)type.size(); i++)
    {
        add_composition(type[i], fixed.base_volume * volume[i] / total_volume_in);
    }
}

std::vector<material> component::remove_composition(float amount)
{
    float total = 0;

    for(material& m : composition)
    {
        total += m.dynamic_desc.volume;
    }

    amount = clamp(amount, 0, total);

    std::vector<material> ret;

    if(total < 0.0001 && amount > total)
    {
        ret = composition;

        for(material& m : composition)
        {
            m.dynamic_desc.volume = 0;
        }

        return ret;
    }

    if(total < 0.0001)
        return ret;

    if(amount == 0)
        return ret;

    for(material& m : composition)
    {
        if(m.dynamic_desc.volume < 0.0001)
        {
            m.dynamic_desc.volume = 0;
            continue;
        }

        float removed = amount * m.dynamic_desc.volume / total;

        material nm = m;
        nm.dynamic_desc.volume = removed;

        m.dynamic_desc.volume -= removed;

        if(m.dynamic_desc.volume < 0)
        {
            m.dynamic_desc.volume = 0;
            throw std::runtime_error("Bad Volume in remove_composition");
        }

        ret.push_back(nm);
    }

    return ret;
}

std::vector<double> ship::get_net_resources(double dt_s, const std::vector<double>& all_sat)
{
    std::vector<double> produced_resources;
    produced_resources.resize(component_info::COUNT);

    for(component& c : components)
    {
        float hp_frac = c.get_hp_frac();

        double min_sat = c.get_sat(all_sat);

        const component_fixed_properties& fixed = c.get_fixed_props();

        for(const does_fixed& unscaled : fixed.d_info)
        {
            does_fixed d = c.scale(unscaled);

            if(d.recharge > 0 || d.recharge_unconditional > 0)
                produced_resources[d.type] += d.recharge * hp_frac * min_sat * dt_s * c.last_production_frac * c.activation_level + d.recharge_unconditional * dt_s;
            else
                produced_resources[d.type] += d.recharge * hp_frac * dt_s * c.last_production_frac * c.activation_level + d.recharge_unconditional * dt_s;
        }

        c.last_sat = min_sat;
    }

    return produced_resources;
}

double component::get_sat(const std::vector<double>& sat)
{
    double min_sat = 1;

    const component_fixed_properties& fixed = get_fixed_props();

    for(const does_fixed& unscaled : fixed.d_info)
    {
        does_fixed d = scale(unscaled);

        if(d.recharge > 0 || d.recharge_unconditional > 0)
            continue;

        min_sat = std::min(sat[d.type], min_sat);
    }

    return min_sat;
}

float component::drain(float amount)
{
    if(amount <= 0)
        return 0;

    float total_drainable = 0;

    for_each_stored([&](component& c)
    {
        if(!c.should_flow())
            return;

        float vstored = c.get_my_volume();

        total_drainable += vstored;
    });

    if(total_drainable <= 0.00001f)
        return 0;

     if(amount > total_drainable)
        amount = total_drainable;

    for_each_stored([&](component& c)
    {
        if(!c.should_flow())
            return;

        float vstored = c.get_my_volume();

        if(vstored < 0.00001f)
            return;

        float my_frac = vstored / total_drainable;

        float to_drain = my_frac * amount;

        if(to_drain < 0.00001f)
            return;

        for(material& m : c.composition)
        {
            float material_drain_volume = (m.dynamic_desc.volume / vstored) * to_drain;

            material_drain_volume = clamp(material_drain_volume, 0, m.dynamic_desc.volume);

            m.dynamic_desc.volume -= material_drain_volume;
        }
    });

    return amount;
}

///ok need to do transferring solids
///take 1 second to transfer a ship of size 1
float component::drain_material_from_to(component& c1_in, component& c2_in, float amount)
{
    ///this is for peformance, not correctness
    if(amount == 0)
        return 0;

    if(amount < 0)
        return drain_material_from_to(c2_in, c1_in, -amount);

    float total_drainable = 0;

    c1_in.for_each_stored([&](component& c)
    {
        if(!c.should_flow())
            return;

        float stored = c.get_my_volume();

        total_drainable += stored;
    });

    if(total_drainable <= 0.00001f)
        return 0;

    if(amount > total_drainable)
        amount = total_drainable;

    const component_fixed_properties& c2_fixed = c2_in.get_fixed_props();

    float internal_storage = c2_fixed.get_internal_volume(c2_in.current_scale);

    float free_volume = internal_storage - c2_in.get_stored_volume();

    if(amount > free_volume)
    {
        amount = free_volume;
    }

    float xferred = 0;

    c1_in.for_each_stored([&](component& c)
    {
        if(!c.should_flow())
            return;

        float stored = c.get_my_volume();

        if(stored < 0.00001f)
            return;

        float my_frac = stored / total_drainable;

        float to_drain = my_frac * amount;

        if(to_drain < 0.00001f)
            return;

        component* found = nullptr;

        ///asserts that there is only ever one flowable component
        c2_in.for_each_stored([&](component& other)
        {
            /*if(!other.should_flow())
                return;*/

            if(!is_equivalent_material(c, other))
                return;

            found = &other;
        });

        if(found == nullptr)
        {
            /*component next;
            next.add(component_info::HP, 0, 1);
            next.long_name = "Fluid";
            next.flows = true;*/

            component next = get_component_default(component_type::MATERIAL, 1);
            next.my_temperature = c.my_temperature;
            next.phase = c.phase;
            next.long_name = c.long_name;
            next.short_name = c.short_name;

            ship nnext;
            nnext.add(next);

            c2_in.stored.push_back(nnext);

            found = &c2_in.stored.back().components.back();
        }

        assert(found);

        ///temp * specific heat * volume
        float old_stored_heat_capacity = found->get_my_contained_heat();

        float donator_temp = c.get_my_temperature();

        for(material& m : c.composition)
        {
            bool processed = false;

            float material_drain_volume = (m.dynamic_desc.volume / stored) * to_drain;

            material_drain_volume = clamp(material_drain_volume, 0, m.dynamic_desc.volume);

            m.dynamic_desc.volume -= material_drain_volume;

            xferred += material_drain_volume;

            float heat_increase = donator_temp * material_drain_volume * material_info::fetch(m.type).specific_heat;

            old_stored_heat_capacity += heat_increase;

            for(material& om : found->composition)
            {
                if(m.type == om.type)
                {
                    om.dynamic_desc.volume += material_drain_volume;

                    processed = true;

                    break;
                }
            }

            if(!processed)
            {
                material next;
                next.type = m.type;
                next.dynamic_desc.volume += material_drain_volume;

                found->composition.push_back(next);
            }
        }

        float next_stored_heat_capacity = old_stored_heat_capacity;

        float total_heat_capacity = 0;

        for(material& m : found->composition)
        {
            total_heat_capacity += m.dynamic_desc.volume * material_info::fetch(m.type).specific_heat;
        }

        float ntemp = 0;

        if(total_heat_capacity > 0.00001f)
            ntemp = next_stored_heat_capacity / total_heat_capacity;

        found->my_temperature = ntemp;
    });

    return xferred;
}

float component::drain_ship_from_to(component& c1_in, component& c2_in, float amount)
{
    if(amount == 0)
        return 0;

    if(amount < 0)
        return drain_ship_from_to(c2_in, c1_in, -amount);

    float xferred = 0;

    for(int sid=0; sid < (int)c1_in.stored.size(); sid++)
    {
        ship& s1 = c1_in.stored[sid];

        if(!s1.is_ship)
            continue;

        if(!c2_in.can_store(s1))
            continue;

        float my_vol = s1.get_my_volume();

        c2_in.store(s1);

        c1_in.stored.erase(c1_in.stored.begin() + sid);
        sid--;

        xferred += my_vol;
    }

    return xferred;
}

std::vector<double> ship::get_sat_percentage()
{
    std::vector<double> all_needed;
    all_needed.resize(component_info::COUNT);

    std::vector<double> all_produced;
    all_produced.resize(component_info::COUNT);

    for(component& c : components)
    {
        float hp_frac = c.get_hp_frac();

        const component_fixed_properties& fixed = c.get_fixed_props();

        for(const does_fixed& unscaled : fixed.d_info)
        {
            does_fixed d = c.scale(unscaled);

            if(d.recharge < 0)
                all_needed[d.type] += d.recharge * hp_frac * c.last_production_frac * c.activation_level + d.recharge_unconditional;

            if(d.recharge > 0)
                all_produced[d.type] += d.recharge * hp_frac * c.get_sat(last_sat_percentage) * c.last_production_frac * c.activation_level + d.recharge_unconditional;
        }
    }

    std::vector<double> resource_status = sum<double>([](component& c)
                                                      {
                                                          return c.get_held();
                                                      });

    //std::cout << "needed " << resource_status[component_info::POWER] << std::endl;

    std::vector<double> sats;
    sats.resize(component_info::COUNT);

    for(auto& i : sats)
        i = 1;

    for(int i=0; i < component_info::COUNT; i++)
    {
        if(all_needed[i] >= -0.00001)
            continue;

        sats[i] = fabs((resource_status[i] + all_produced[i]) / all_needed[i]);

        if(sats[i] > 1)
            sats[i] = 1;
    }

    //std::cout << "theoretical power = " << sats[component_info::POWER] * all_produced[component_info::POWER] + all_needed[component_info::POWER] << std::endl;

    return sats;
}

template<typename T>
void add_to_vector(double val, T& in, int max_data)
{
    while((int)in.size() < max_data - 1)
    {
        in.push_back(0);
        //in.insert(in.begin(), 0);
    }

    in.push_back(val);

    while((int)in.size() > max_data)
    {
        in.erase(in.begin());
    }
}

void data_tracker::add(double sat, double held)
{
    add_to_vector(sat, vsat, max_data);
    add_to_vector(held, vheld, max_data);
}

float get_mass_effect(float mass, float quantity)
{
    if(mass <= 0.00001)
        return 0;

    ///example
    ///we have shields with a value of 1
    ///we have a ship with 1 mass
    ///when shields have value > mass, we get positive mass effect
    ///when shields have value < mass, we get diminishing returns

    ///concrete example
    ///say we have shields with a value of 5
    ///and a ship with a mass of 4
    ///we should get... 5/4 right
    ///ok maybe its just fractional mass then?

    return quantity / mass;
}

struct laser : projectile
{
    sf::Clock clk;
    float damage = 0;

    std::shared_ptr<alt_radar_field> field;

    laser(std::shared_ptr<alt_radar_field>& _field) : field(_field)
    {

    }

    virtual void on_collide(entity_manager& em, entity& other) override
    {
        if(dynamic_cast<ship*>(&other))
        {
            dynamic_cast<ship*>(&other)->take_damage(damage);
        }

        cleanup = true;
    }

    virtual void tick(double dt_s) override
    {
        ///lasers won't reflect
        //projectile::tick(dt_s);

        if(clk.getElapsedTime().asMicroseconds() / 1000. > 200)
        {
            phys_ignore.clear();
        }

        if((clk.getElapsedTime().asMicroseconds() / 1000. / 1000.) > 20)
        {
            cleanup = true;
        }

        alt_frequency_packet em;
        em.make(10000, 3000);

        field->emit(em, r.position, *this);
    }
};

struct laser_base : projectile
{
    sf::Clock clk;
    ship* parent_ship = nullptr;
    float power = 0;

    std::shared_ptr<alt_radar_field> field;

    laser_base(std::shared_ptr<alt_radar_field>& _field) : field(_field)
    {
        float SoL = field->speed_of_light_per_tick;

        r.init_rectangular({SoL/1.8, 0.2});

        is_collided_with = false;
    }

    virtual void tick(double dt_s) override
    {
        if(parent_ship && parent_ship->cleanup)
            parent_ship = nullptr;

        if(clk.getElapsedTime().asMicroseconds() / 1000. > 1000)
        {
            phys_ignore.clear();
        }

        if((clk.getElapsedTime().asMicroseconds() / 1000. / 1000.) > 20)
        {
            cleanup = true;
        }

        alt_frequency_packet em;
        em.make(10000, 3000);

        field->emit(em, r.position, *this);
    }
};

struct mining_laser : laser_base
{
    uint64_t parent_component = -1;

    mining_laser(std::shared_ptr<alt_radar_field>& _field) : laser_base(_field)
    {

    }

    virtual void tick(double dt_s) override
    {
        ///lasers won't reflect
        //projectile::tick(dt_s);

        laser_base::tick(dt_s);
    }

    virtual void on_collide(entity_manager& em, entity& other) override
    {
        cleanup = true;

        if(parent_ship == nullptr)
            return;

        asteroid* hit_asteroid = dynamic_cast<asteroid*>(&other);

        if(hit_asteroid == nullptr)
            return;

        for(component& c : parent_ship->components)
        {
            //if(c._pid == parent_component)
            if(c.base_id == component_type::CARGO_STORAGE)
            {
                float free = c.get_internal_volume() - c.get_stored_volume();

                float to_store = power;

                if(to_store > free)
                    to_store = free;

                float iron_ratio = 0.1;
                float silicate_ratio = 0.85;
                float copper_ratio = 0.05;

                float quantity = to_store;

                if(to_store <= 0.00001)
                    continue;

                /*component my_iron = get_component_default(component_type::MATERIAL, 1);
                component my_junk = get_component_default(component_type::MATERIAL, 1);
                component my_copper = get_component_default(component_type::MATERIAL, 1);

                my_iron.add_composition(material_info::IRON, iron_ratio * quantity);
                my_junk.add_composition(material_info::H2O, silicate_ratio * quantity); ///TODO: NOT THIS
                my_copper.add_composition(material_info::COPPER, copper_ratio * quantity);

                ///does not correctly handle temperature
                c.store(my_iron);
                c.store(my_junk);
                c.store(my_copper);*/

                component my_comp = get_component_default(component_type::MATERIAL, 1);

                my_comp.long_name = "Ore";
                my_comp.short_name = "ORE";

                my_comp.add_composition(material_info::IRON, iron_ratio * quantity);
                my_comp.add_composition(material_info::SILICON, silicate_ratio * quantity); ///TODO: NOT THIS
                my_comp.add_composition(material_info::COPPER, copper_ratio * quantity);

                c.store(my_comp, true);

                break;
            }
        }

        ///ok so
        ///if we collide with an asteroid, instantly transfer back to the ship but fire a counter laser back (?)
        ///or maybe we fire a counter laser back but it has to hit the ship to work (but kind of dodgy but does open up degrees of resource stealing)

        //assert(dynamic_cast<mining_laser*>(&other) == nullptr);
    }
};

struct tractor_laser : laser_base
{
    tractor_laser(std::shared_ptr<alt_radar_field>& _field) : laser_base(_field)
    {

    }

    virtual void tick(double dt_s) override
    {
        laser_base::tick(dt_s);
    }

    virtual void on_collide(entity_manager& em, entity& other) override
    {
        cleanup = true;

        if(parent_ship == nullptr)
            return;

        /*ship* hit_ship = dynamic_cast<ship*>(&other);

        if(hit_ship == nullptr)
            return;*/

        float their_mass = other.mass;

        ship* hit_ship = dynamic_cast<ship*>(&other);

        if(hit_ship)
        {
            auto held = hit_ship->sum<double>(
            [](component& c)
            {
                return c.get_held();
            });

            their_mass += get_mass_effect(other.mass, held[component_type::SHIELDS]) * other.mass * 10;
        }

        float move_ability = 0;

        if(their_mass > 0.0001)
            move_ability = power/their_mass;

        if(move_ability <= 0)
            return;

        float vlen = other.velocity.length();

        //if(vlen > 0.0001)
        {
            vec2f to_me = (parent_ship->r.position - other.r.position).norm();

            vec2f bad_component = projection(other.velocity, to_me.rot(M_PI/2));

            float pre_len = bad_component.length();

            float post_len = pre_len - move_ability;

            move_ability = post_len < 0 ? fabs(post_len) : 0;

            if(post_len < 0)
                post_len = 0;

            vec2f change = bad_component.norm() * (post_len - pre_len);

            vec2f extra = to_me * move_ability;

            //float min_vel = vlen - move_ability;

            /*float extra = min_vel < 0 ? fabs(min_vel) : 0;


            other.velocity = other.velocity.norm() * min_vel + to_me.norm() * extra;*/

            other.velocity += extra + change;
        }

        for(component& c : parent_ship->components)
        {
            if(c.base_id == component_type::CARGO_STORAGE)
            {
                float free = c.get_internal_volume() - c.get_stored_volume();


                break;
            }
        }
    }
};

void ship::handle_cleanup()
{
    for(int i=0; i < (int)components.size(); i++)
    {
        if(components[i].cleanup)
        {
            components.erase(components.begin() + i);
            i--;
            continue;
        }

        for(ship& s : components[i].stored)
        {
            s.handle_cleanup();
        }
    }
}

void ship::resume_building(size_t component_pid, size_t object_pid)
{
    component* found_comp = nullptr;

    for(auto& i : components)
    {
        if(i._pid == component_pid)
        {
            found_comp = &i;
        }
    }

    if(found_comp == nullptr)
        return;

    if(found_comp->base_id != component_type::FACTORY)
        return;

    for(auto& i : found_comp->build_queue)
    {
        if(i->in_progress_pid == object_pid)
            return;
    }

    std::optional<ship*> fship = fetch_ship_by_id(object_pid);

    if(!fship && my_room && found_comp->has_tag(tag_info::TAG_EFACTORY))
    {
        fship = my_room->get_nearby_unfinished_ship(*this, object_pid);
    }

    if(!fship)
        return;

    fship.value()->is_build_holder = true;

    build_in_progress build;
    build.make(*fship.value()->original_blueprint);

    build.in_progress_pid = object_pid;

    found_comp->building = true;
    found_comp->build_queue.push_back(std::make_shared<build_in_progress>(build));

    printf("Resumed\n");
}

void ship::cancel_building(size_t component_pid, size_t object_pid)
{
    component* found_comp = nullptr;

    for(auto& i : components)
    {
        if(i._pid == component_pid)
        {
            found_comp = &i;
        }
    }

    if(found_comp == nullptr)
        return;

    if(found_comp->base_id != component_type::FACTORY)
        return;

    for(int i=0; i < (int)found_comp->build_queue.size(); i++)
    {
        auto& build_item = found_comp->build_queue[i];

        if(build_item->in_progress_pid == object_pid)
        {
            found_comp->build_queue.erase(found_comp->build_queue.begin() + i);
            i--;
            continue;
        }
    }

    if(found_comp->build_queue.size() == 0)
        found_comp->building = false;
}

void handle_manufacturing(ship& s, component& fac, double dt_s)
{
    for(auto id : fac.unchecked_blueprints)
    {
        if(s.persistent_data)
        {
            auto found = s.persistent_data->blueprint_manage.fetch(id);

            if(found)
            {
                ///creates the ghost ship, is empty
                fac.manufacture_blueprint(*found.value(), s);
            }
        }
    }

    fac.unchecked_blueprints.clear();

    if(fac.build_queue.size() > 0)
    {
        build_in_progress& in_progress = *fac.build_queue[0];

        ship* ship_placeholder = dynamic_cast<ship*>(find_by_id(s, in_progress.in_progress_pid));

        if(ship_placeholder == nullptr && s.my_room && fac.has_tag(tag_info::TAG_EFACTORY))
        {
            auto ship_opt = s.my_room->get_nearby_unfinished_ship(s, in_progress.in_progress_pid);

            printf("Looking for opt\n");

            if(ship_opt.has_value())
            {
                ship_placeholder = ship_opt.value();
                printf("Has opt\n");
            }
        }

        if(ship_placeholder == nullptr)
        {
            printf("Did not find\n");
            fac.build_queue.erase(fac.build_queue.begin());
            return;
        }

        std::vector<std::vector<material>> total_required_mats = in_progress.result.get_cost();

        std::vector<std::vector<material>> already_drained;
        get_ship_cost(*ship_placeholder, already_drained);

        std::vector<material>* material_unsat = nullptr;

        float unsat_amount = 0;

        for(auto& material_vec : total_required_mats)
        {
            bool sat = false;

            float sat_amount = 0;

            for(auto& existing : already_drained)
            {
                if(is_equivalent_material(material_vec, existing))
                {
                    sat_amount += material_volume(existing);

                    if(material_satisfies({material_vec}, {existing}))
                    {
                        sat = true;
                        break;
                    }
                }
            }

            if(!sat)
            {
                unsat_amount = material_volume(material_vec) - sat_amount;

                if(unsat_amount < 0)
                {
                    printf("Unsat amount < 0?\n");
                    unsat_amount = 0;
                }

                material_unsat = &material_vec;
                break;
            }
        }

        bool all_sat = material_unsat == nullptr;

        if(material_unsat)
        {
            float total_moveable = fac.get_produced()[component_info::MANUFACTURING] * dt_s / SIZE_TO_TIME;

            component* found_parent = nullptr;
            component* found_material_container = nullptr;

            for(component& fcomp : s.components)
            {
                for(ship& stored : fcomp.stored)
                {
                    if(stored.is_ship)
                        continue;

                    for(component& storage_material : stored.components)
                    {
                        if(storage_material.base_id != component_type::MATERIAL)
                            continue;

                        if(is_equivalent_material(*material_unsat, storage_material.composition))
                        {
                            found_parent = &fcomp;
                            found_material_container = &storage_material;
                        }
                    }
                }
            }

            if(found_parent && found_material_container)
            {
                ///inaccurate (> max container volume) in the event that we're draining from the same cargo container
                ///into itself
                ///this is correct for this use case but confusing
                float free_volume = 0;

                ///not taking into account that taken materials and
                ///the ship itself are probably in the same cargo container
                for(component& c : s.components)
                {
                    for(ship& fs : c.stored)
                    {
                        if(&fs == ship_placeholder)
                        {
                            if(&c == found_parent)
                            {
                                ///same container
                                free_volume = (c.get_internal_volume() - c.get_stored_volume()) + total_moveable;
                            }
                            else
                            {
                                ///different container
                                free_volume = c.get_internal_volume() - c.get_stored_volume();
                            }

                            break;
                        }
                    }
                }

                std::vector<material>& raw_fodder = found_material_container->composition;

                std::vector<material>* drain_into = nullptr;
                component* drain_into_component = nullptr;

                ///so raw_fodder is the material that is raw fodder
                ///material_unsat is the *type* of material that we want, but not the actual destination for it
                for(component& c : ship_placeholder->components)
                {
                    assert(c.is_build_holder);

                    if(is_equivalent_material(*material_unsat, c.composition))
                    {
                        drain_into = &c.composition;
                        drain_into_component = &c;
                        break;
                    }
                }

                if(drain_into == nullptr)
                {
                    component new_comp = get_component_default(component_type::MATERIAL, 1);
                    new_comp.long_name = "(Unfinished) " + in_progress.result.name;
                    new_comp.is_build_holder = true;

                    for(const material& m : raw_fodder)
                    {
                        material nm = m;
                        nm.dynamic_desc.volume = 0;

                        new_comp.composition.push_back(nm);
                    }

                    ship_placeholder->components.push_back(new_comp);

                    drain_into = &ship_placeholder->components.back().composition;
                    drain_into_component = &ship_placeholder->components.back();
                }

                assert(drain_into->size() == raw_fodder.size());

                ///so
                ///take from raw fodder, put into drain_into
                ///ensure we don't exceed maximum free space (already accounted for the corner cases so use directly)

                float total_mat = material_volume(raw_fodder);

                float to_take = total_moveable;

                to_take = std::min(to_take, total_mat);
                to_take = std::min(to_take, free_volume);
                to_take = std::min(to_take, unsat_amount);

                float my_vol = material_volume(drain_into_component->composition);

                if(my_vol + to_take > 0.00001)
                {
                    float total_new_vol = my_vol + to_take;

                    float mix_fraction = (my_vol / total_new_vol);

                    mix_fraction = clamp(mix_fraction, 0, 1);

                    ///?????
                    //drain_into_component->my_temperature += (found_material_container->my_temperature - drain_into_component->my_temperature) * (1 - mix_fraction) * dt_s;
                    drain_into_component->my_temperature = mix(drain_into_component->my_temperature, found_material_container->my_temperature, 1 - mix_fraction);
                }

                ///to_take is guaranteed <= sum of raw fodder
                std::vector<material> requested_drain = material_normalise(raw_fodder, to_take);

                ship_placeholder->construction_amount += material_volume(requested_drain);

                material_merge(*drain_into, requested_drain);
                material_deplete(raw_fodder, to_take);
            }
        }

        if(fac.build_queue.size() > 0 && all_sat)
        {
            build_in_progress& next_in_progress = *fac.build_queue[0];

            ship spawn = next_in_progress.result.to_ship();

            ///so this is a fun one
            *ship_placeholder = spawn;
        }
    }
    else
    {
        fac.building = false;
    }
}

void ship::tick(double dt_s)
{
    handle_cleanup();

    mass = get_mass();

    if(room_type != space_type::REAL_SPACE)
        thrusters_active = false;

    std::vector<double> resource_status = sum<double>([](component& c)
    {
        return c.get_held();
    });

    std::vector<double> max_resource_status = sum<double>([](component& c)
    {
        return c.get_capacity();
    });

    std::vector<double> theoretical_resource_produced = sum<double>([](component& c)
    {
        return c.get_theoretical_produced();
    });

    std::vector<double> theoretical_resource_consumed = sum<double>([](component& c)
    {
        return c.get_theoretical_consumed();
    });

    has_s_power = resource_status[component_info::S_POWER] >= my_size;
    has_w_power = resource_status[component_info::W_POWER] >= my_size;

    for(component& c : components)
    {
        bool any_under = false;

        double max_theoretical_activation_fraction = 0;

        const component_fixed_properties& fixed = c.get_fixed_props();

        for(const does_fixed& unscaled : fixed.d_info)
        {
            does_fixed d = c.scale(unscaled);

            if(d.recharge <= 0 || d.recharge_unconditional < 0)
                continue;

            if(resource_status[d.type] < max_resource_status[d.type])
            {
                any_under = true;
            }

            if(theoretical_resource_produced[d.type] <= 0.00001f)
                continue;

            double theoretical_delta = theoretical_resource_consumed[d.type];

            if(theoretical_resource_produced[d.type] < 0.00001)
                continue;

            double global_production_fraction = fabs(theoretical_delta) / theoretical_resource_produced[d.type];

            max_theoretical_activation_fraction = std::max(max_theoretical_activation_fraction, global_production_fraction);
        }

        if(!any_under && fixed.no_drain_on_full_production)
        {
            c.last_production_frac = 0.1;
        }
        else
        {
            c.last_production_frac = 1;
        }

        if(fixed.complex_no_drain_on_full_production)
        {
            if(!any_under)
            {
                max_theoretical_activation_fraction = clamp(max_theoretical_activation_fraction, 0, 1);

                c.last_production_frac = max_theoretical_activation_fraction + 0.01;
            }
            else
            {
                c.last_production_frac = 1;
            }
        }
    }

    for(component& c : components)
    {
        if(c.has(component_info::THRUST))
        {
            c.last_production_frac = thrusters_active;
        }

        if(c.has(component_info::MANUFACTURING))
        {
            if(c.building)
            {
                c.last_production_frac = 1;
            }
            else
            {
                c.last_production_frac = 0;
            }
        }
    }

    std::vector<double> all_sat = get_sat_percentage();

    //std::cout << "SAT " << all_sat[component_info::LIFE_SUPPORT] << std::endl;

    std::vector<double> produced_resources = get_net_resources(dt_s, all_sat);

    ///manufacturing and refining
    for(component& c : components)
    {
        if(c.has(component_info::MANUFACTURING))
        {
            handle_manufacturing(*this, c, dt_s);
        }

        ///so: approaches
        ///1 would be that we sequester off materials automatically into the refinery
        ///use the existing smelting approach, and then cart them back
        ///2. approach is to make refining a mechanical process like manufacturing
        ///advantages of 1. easier to implement, reuses existing things, no arbitraryness, keeps heat mechanics
        ///disadvantages of 1. have to implement control system logic for refinery rather than just saying "smelt xyz"
        ///doesn't necessarily add gameplay compared to 2
        ///advantages of 2. Simple, leads to more ED style gameplay for refining in space
        ///disadvanges of 2. Maybe too simple - does not feel physical, bit arbitrary
        ///ok with 1 we can drag/drop resources in too which means breaking down existing stuff
        ///1 is probably best because can reuse heat mechanics for salvaging

        ///ok so
        ///material transfer into and out of refinery should be manual but easily automateable
        ///(hello this is a game about automation)
        ///so refinery needs no changes but we do need ui options to handle transfer of ore
        if(c.has_tag(tag_info::TAG_REFINERY))
        {
            //for(component& c : )
        }
    }

    std::vector<double> next_resource_status;
    next_resource_status.resize(component_info::COUNT);

    for(int i=0; i < (int)component_info::COUNT; i++)
    {
        next_resource_status[i] = resource_status[i] + produced_resources[i];
    }

    for(component& c : components)
    {
        if(c.has_tag(tag_info::TAG_MISSILE_BEHAVIOUR))
        {
            //tick_missile_behaviour(dt_s);
        }
    }

    for(component& c : components)
    {
        if(!c.has(component_info::RADAR))
            continue;

        does_fixed d = c.get_fixed(component_info::RADAR);

        alt_radar_field& radar = *current_radar_field;

        assert(radar_frequency_composition.size() == radar_intensity_composition.size());

        alt_frequency_packet em;
        ///getting a bit complex to determine this value
        //em.make(20000 * c.get_operating_efficiency() * d.recharge, 2000);

        for(int i=0; i < (int)radar_intensity_composition.size(); i++)
        {
            em.make(20000 * c.get_operating_efficiency() * d.recharge * radar_intensity_composition[i], radar_frequency_composition[i]);
        }

        em.restrict(c.radar_offset_angle, c.radar_restrict_angle);

        radar.emit(em, r.position, *this);
    }

    ///item uses
    for(component& c : components)
    {
        const component_fixed_properties& fixed = c.get_fixed_props();

        if(c.try_use || c.force_use)
        {
            if(c.can_use(next_resource_status) || c.force_use)
            {
                c.last_activation_successful = true;

                c.use(next_resource_status);

                c.add_heat_to_me(c.get_use_heat());

                if(c.has_tag(tag_info::TAG_WEAPON))
                {
                    double eangle = c.use_angle;
                    double ship_rotation = r.rotation;

                    vec2f evector = (vec2f){1, 0}.rot(eangle);
                    vec2f ship_vector = (vec2f){1, 0}.rot(ship_rotation);
                    alt_radar_field& radar = *current_radar_field;

                    if(fabs(angle_between_vectors(ship_vector, evector)) > fixed.max_use_angle)
                    {
                        float angle_signed = signed_angle_between_vectors(ship_vector, evector);

                        evector = ship_vector.rot(signum(angle_signed) * fixed.max_use_angle);
                    }


                    if(fixed.subtype == "laser")
                    {
                        laser* l = parent->make_new<laser>(current_radar_field);
                        l->r.position = r.position;
                        l->r.rotation = evector.angle();
                        ///speed of light is notionally a constant
                        l->velocity = evector.norm() * (float)(radar.speed_of_light_per_tick / radar.time_between_ticks_s);
                        l->phys_ignore.push_back(_pid);

                        l->damage = c.get_activate_fixed(component_info::LASER).capacity;
                    }

                    ///ok so:
                    ///for the moment have a fixed range just as a hack
                    ///look for asteroids within that range that we hit
                    ///get closest hit, and then deposit ore into the storage
                    ///also need to do some rendering
                    ///could uuh
                    ///just make it a laser projectile
                    ///after all, speed of light and that
                    ///actually I really like that - sol to target, sol transport back as packets
                    if(fixed.subtype == "mining")
                    {
                        mining_laser* l = parent->make_new<mining_laser>(current_radar_field);
                        l->r.position = r.position;
                        l->r.rotation = evector.angle();
                        ///speed of light is notionally a constant
                        l->velocity = evector.norm() * (float)(radar.speed_of_light_per_tick / radar.time_between_ticks_s) / 10.f;
                        l->phys_ignore.push_back(_pid);

                        l->parent_ship = this;
                        l->parent_component = c._pid;

                        l->power = c.get_activate_fixed(component_info::MINING).capacity * radar.time_between_ticks_s;
                    }

                    if(fixed.subtype == "tractor")
                    {
                        tractor_laser* l = parent->make_new<tractor_laser>(current_radar_field);
                        l->r.position = r.position;
                        l->r.rotation = evector.angle();
                        ///speed of light is notionally a constant
                        l->velocity = evector.norm() * (float)(radar.speed_of_light_per_tick / radar.time_between_ticks_s) / 10.f;
                        l->phys_ignore.push_back(_pid);

                        l->parent_ship = this;

                        l->power = c.get_activate_fixed(component_info::TRACTOR).capacity * radar.time_between_ticks_s;
                    }

                    alt_frequency_packet em;
                    em.make(20000, 1000);

                    radar.emit(em, r.position, *this);
                }

                ///take one component out of storage, eject it into space
                ///need to add it to the entity manager. Has a correct _pid so no worries there
                ///ok, need to harvest cpu files which are inside this parent ship
                ///so get root directory of ship before we remove
                ///then harvest files before we remove
                ///then strip the prefix of those files and add them back once we've spawnd the ship
                ///would be a lot easier if we stored the files actually on the ship instead of the current 'outside' model
                ///or actually if there was just a real directory system
                if(c.has_tag(tag_info::TAG_EJECTOR) && c.stored.size() > 0)
                {
                    std::vector<cpu_file> files;

                    for(component& tc : components)
                    {
                        if(tc.base_id != component_type::CPU)
                            continue;

                        ///c.stored[0] is because its the first stored item
                        std::string directory = c.stored[0].current_directory;

                        auto found = tc.cpu_core.extract_files_in_directory(directory);

                        files.insert(files.end(), found.begin(), found.end());
                    }

                    std::optional<ship> first = c.remove_first_stored_item();

                    if(first.has_value())
                    {
                        vec2f ship_vector = (vec2f){1, 0}.rot(r.rotation);

                        ship& to_produce = first.value();

                        ship* spawned = parent->take(to_produce);

                        spawned->r.position = r.position;
                        spawned->r.rotation = r.rotation;
                        //l->r.rotation = r.rotation + eangle;
                        //l->velocity = (vec2f){1, 0}.rot(r.rotation) * 50;
                        spawned->velocity = velocity + ship_vector.norm() * 10;
                        //l->velocity = velocity + (vec2f){1, 0}.rot(r.rotation + eangle) * 50;
                        spawned->phys_ignore.push_back(_pid);
                        //spawned->fired_by = id;

                        spawned->r.init_rectangular({1, 0.2});
                        spawned->network_owner = network_owner;
                        spawned->spawn_clock.restart();
                        spawned->spawned_by = _pid;
                        spawned->phys_drag = false;
                        spawned->current_radar_field = current_radar_field;

                        for(component& tc : spawned->components)
                        {
                            if(tc.base_id != component_type::CPU)
                                continue;

                            tc.cpu_core.files.insert(tc.cpu_core.files.end(), files.begin(), files.end());

                            tc.cpu_core.free_running = true;
                        }
                    }
                }

                /*if(c.has(component_info::SENSORS))
                {
                    alt_radar_field& radar = *current_radar_field;

                    alt_frequency_packet em;
                    em.frequency = 2000;
                    ///getting a bit complex to determine this value
                    em.intensity = 100000 * c.get_operating_efficiency();

                    radar.emit(em, r.position, *this);
                }*/

                ///damage surrounding targets
                ///should not be instantaneous but an expanding explosion ring, repurposable
                ///apply progressive damage to targets
                ///subtract damage from self before propagating to friends, or at least
                ///at minimum make it so it has to bust open the container, armour, and shields of us first
                if(c.has(component_info::SELF_DESTRUCT))
                {
                    float total_power = 0;

                    for(component& my_comp : components)
                    {
                        if(my_comp.base_id != component_type::CARGO_STORAGE)
                            continue;

                        for(ship& s : my_comp.stored)
                        {
                            if(s.is_ship)
                                continue;

                            for(component& store : s.components)
                            {
                                std::pair<material_dynamic_properties, material_fixed_properties> props = get_material_composite(store.composition);
                                material_dynamic_properties& dynamic = props.first;
                                material_fixed_properties& fixed = props.second;

                                if(fixed.specific_explosiveness == 0)
                                    continue;

                                double power = dynamic.volume * fixed.specific_explosiveness;

                                total_power += power;

                                store.cleanup = true;
                            }
                        }
                    }

                    std::cout << "POWER " << total_power << std::endl;

                    if(total_power < resource_status[component_info::HP])
                    {
                        take_damage(total_power, true);
                    }
                    else
                    {
                        take_damage(resource_status[component_info::HP], true);
                        total_power -= resource_status[component_info::HP];

                        double radius = sqrt(total_power);

                        if(parent == nullptr)
                            throw std::runtime_error("Broken");

                        aoe_damage* aoe = parent->make_new<aoe_damage>(current_radar_field);

                        aoe->max_radius = radius;
                        aoe->damage = total_power;
                        aoe->collided_with[_pid] = true;
                        aoe->r.position = r.position;
                        aoe->emitted_by = _pid;
                        aoe->velocity = velocity;
                    }
                }
            }
            else
            {
                c.last_activation_successful = false;
            }

            c.try_use = false;
            c.force_use = false;
        }

        for(does_dynamic& d : c.dyn_activate_requirements)
        {
            d.last_use_s += dt_s;
        }

        c.last_could_use = c.can_use(next_resource_status);
    }

    for(storage_pipe& p : pipes)
    {
        p.flow_rate = clamp(p.flow_rate, -p.max_flow_rate, p.max_flow_rate);

        auto c1_o = get_component_from_id(p.id_1);
        auto c2_o = get_component_from_id(p.id_2);

        if(c1_o && c2_o && !p.goes_to_space)
        {
            component& c1 = *c1_o.value();
            component& c2 = *c2_o.value();

            if(p.fluids)
                component::drain_material_from_to(c1, c2, p.flow_rate * dt_s);

            if(p.solids)
            {
                ///changed direction of pipe
                if(signum(p.flow_rate) != signum(p.transfer_work))
                    p.transfer_work = 0;

                ///get amount of work spent
                float extra = component::drain_ship_from_to(c1, c2, p.transfer_work);

                ///unspent work saved
                float diff = fabs(p.flow_rate * dt_s) - fabs(extra);

                p.transfer_work += diff * signum(p.flow_rate);

                if(p.flow_rate > 0)
                {
                    p.transfer_work = clamp(p.transfer_work, 0, c2.get_internal_volume());
                }

                if(p.flow_rate < 0)
                {
                    p.transfer_work = clamp(p.transfer_work, -c1.get_internal_volume(), 0);
                }
            }
        }

        if(c1_o && p.goes_to_space)
        {
            component& c1 = *c1_o.value();

            if(p.flow_rate > 0)
            {
                float real_drain = c1.drain(p.flow_rate * dt_s);

                ///so
                ///going to make an extremely crude assumption
                ///em signature out is proportional to temperature of emitted gas * emission

                ///theoretically something with a higher specific heat should emit for longer

                ///but like.. really i want a mechanic where emitting less hot stuff is worse than more cold stuff?
                float real_emissions = real_drain * c1.get_stored_temperature();


                alt_frequency_packet heat;
                heat.make(real_emissions * 100, HEAT_FREQ);

                alt_radar_field& radar = *current_radar_field;

                radar.emit(heat, r.position, *this);
            }
        }
    }

    std::vector<double> diff;
    diff.resize(component_info::COUNT);

    for(int i=0; i < (int)component_info::COUNT; i++)
    {
        diff[i] = next_resource_status[i] - resource_status[i];
    }

    std::vector<double> free_storage = sum<double>([](component& c)
    {
        std::vector<double> held = c.get_held();
        std::vector<double> capacity = c.get_capacity();

        std::vector<double> ret;

        if(held.size() != capacity.size())
            throw std::runtime_error("Bad sizes in sum free storage");

        for(int i=0; i < (int)held.size(); i++)
        {
            ret.push_back(capacity[i] - held[i]);
        }

        return ret;
    });

    ///so the problem with this is that we're applying the diff sequentially
    ///need to pass in the total required of a thing, and then only take a fraction of whats available
    ///will give averaging hp repair
    ///then combine the two together and we get a priority system

    for(component& c : components)
    {
        c.deplete_me(diff, free_storage, resource_status);
    }

    last_sat_percentage = all_sat;

    handle_heat(dt_s);
    handle_degredation(dt_s);

    for(component& c : components)
    {
        for(ship& s : c.stored)
        {
            if(s.is_build_holder)
                continue;

            for(int kk=0; kk < s.components.size(); kk++)
            {
                /*if(s.components[kk].get_my_volume() < 0.001 && s.components[kk].base_id == component_type::MATERIAL)
                {
                    s.components.erase(s.components.begin() + kk);
                    kk--;
                    continue;
                }*/

                component& to_remove = s.components[kk];

                bool remove = false;

                if(to_remove.base_id == component_type::MATERIAL)
                {
                    ///in the shitter, schedule to be removed
                    if(to_remove.get_my_volume() < 0.001 && !to_remove.is_build_holder)
                    {
                        if(!to_remove.bad_time)
                            to_remove.bad_time = fixed_clock();

                        ///1s
                        if(to_remove.bad_time->getElapsedTime().asMicroseconds() >= 1000 * 1000)
                        {
                            remove = true;
                        }
                    }
                    else
                    {
                        to_remove.bad_time = std::nullopt;
                    }
                }

                if(remove)
                {
                    s.components.erase(s.components.begin() + kk);
                    kk--;
                    continue;
                }
            }
        }
    }


    ///cleanup dead ships internally
    for(component& c : components)
    {
        std::vector<component> melted_extras;

        for(int i=0; i < (int)c.stored.size(); i++)
        {
            ship& s = c.stored[i];

            auto held = s.sum<double>([](component& c)
            {
                return c.get_held();
            });

            auto capacity = s.sum<double>([](component& c)
            {
                return c.get_capacity();
            });

            if(held[component_info::HP] <= 0 && capacity[component_info::HP] > 0)
            {
                /*c.stored.erase(c.stored.begin() + i);
                i--;
                continue;*/

                if(s.is_ship)
                {
                    std::vector<std::vector<material>> cost;
                    get_ship_cost(s, cost);

                    for(auto& i : cost)
                    {
                        component next = get_component_default(component_type::MATERIAL, 1);

                        auto [dyn, fixed] = get_material_composite(i);

                        next.composition = i;
                        next.my_temperature = c.my_temperature;
                        next.phase = next.my_temperature > fixed.melting_point ? 1 : 0;

                        melted_extras.push_back(next);
                    }
                }
                else ///??? heat damage not applied to materials, should be exempt from stuff
                {

                }

                c.stored.erase(c.stored.begin() + i);
                i--;
                continue;
            }

            if(s.construction_amount > 0 && s.is_build_holder && s.get_my_volume() <= 0.001 && !any_building(s._pid))
            {
                c.stored.erase(c.stored.begin() + i);
                i--;
                continue;
            }
        }

        for(auto& i : melted_extras)
        {
            c.store(i, true);
        }
    }

    data_track_elapsed_s += dt_s;

    double time_between_datapoints_s = 0.1;

    if(data_track_elapsed_s >= time_between_datapoints_s)
    {
        for(auto& type : tracked)
        {
            double held = resource_status[type];
            double sat = all_sat[type];

            //std::cout << "dtrack size " << (*data_track).size() << std::endl;

            data_track[type].add(sat, held);
        }

        data_track_elapsed_s -= time_between_datapoints_s;
    }
}

std::optional<component*> ship::get_component_from_id(uint64_t id)
{
    for(component& c : components)
    {
        if(c._pid == id)
            return &c;
    }

    return std::nullopt;
}

float uniform_get_stored_temperature(component& c)
{
    if(c.get_stored_volume() < 0.1 || !c.get_fixed_props().heat_sink)
        return c.get_my_temperature();
    else
        return c.get_stored_temperature();
}

float uniform_get_stored_heat(component& c)
{
    if(c.get_stored_volume() < 0.1 || !c.get_fixed_props().heat_sink)
        return c.get_my_contained_heat();
    else
        return c.get_stored_heat_capacity();
}

void uniform_add_stored_heat(component& c, float amount)
{
    if(c.get_stored_volume() < 0.1 || !c.get_fixed_props().heat_sink)
        c.add_heat_to_me(amount);
    else
        c.add_heat_to_stored(amount);
}

void uniform_remove_stored_heat(component& c, float amount)
{
    if(c.get_stored_volume() < 0.1 || !c.get_fixed_props().heat_sink)
        c.remove_heat_from_me(amount);
    else
        c.remove_heat_from_stored(amount);
}

float uniform_get_stored_volume(component& c)
{
    if(c.get_stored_volume() < 0.1 || !c.get_fixed_props().heat_sink)
        return c.get_my_volume();
    else
        return c.get_stored_volume();
}

float uniform_get_heat_x_volume(component& c)
{
    if(c.get_stored_volume() < 0.1 || !c.get_fixed_props().heat_sink)
        return c.get_my_heat_x_volume();
    else
        return c.get_stored_heat_x_volume();
}

void heat_transfer(float heat_coeff, float dt_s, component& c, component& hs, float mult)
{
    //if(hs.get_stored_volume() < 0.1)
    //    continue;

    if(c.get_my_volume() < 0.0001 || hs.get_my_volume() < 0.0001)
        return;

    //const component_fixed_properties& c_fixed = c.get_fixed_props();
    const component_fixed_properties& hs_fixed = hs.get_fixed_props();


    //assert(hs_fixed.heat_sink);

    //float hs_stored = hs.get_stored_temperature();

    float hs_stored = uniform_get_stored_temperature(hs);
    float my_temperature = c.get_my_temperature();

    float temperature_difference = (my_temperature - hs_stored);

    if(temperature_difference * mult <= 0 && mult != 0)
        return;

    //if(temperature_difference <= 0)
    //    return;

    ///ok
    ///assume every material has the same conductivity
    ///its also assuming incorrect things about mass which is why the temperature is jittering
    /*float heat_transfer_rate = temperature_difference * heat_coeff * dt_s;

    c.remove_heat_from_me(heat_transfer_rate);
    //hs.add_heat_to_stored(heat_transfer_rate);
    uniform_add_stored_heat(hs, heat_transfer_rate);*/

    float total_heat_x_vol = c.get_my_heat_x_volume() + uniform_get_heat_x_volume(hs);

    float c_h_x_v = c.get_my_heat_x_volume();
    float hs_h_x_v = uniform_get_heat_x_volume(hs);

    if(total_heat_x_vol < 0.00001)
        return;

    float final_temperature = (c_h_x_v * c.get_my_temperature() + hs_h_x_v * uniform_get_stored_temperature(hs)) / total_heat_x_vol;

    //printf("ttt %f %f %f hs %f real stored %f\n", final_temperature, c.get_my_temperature(), uniform_get_stored_temperature(hs), hs_stored, hs.get_stored_temperature());

    /*
    ///problem is we're calculating differences in temperature
    ///really just need to ensure that if we hit final_temperature, we clamp
    ///my temperature > final temperature
    ///this val < 0
    float component_to_final = final_temperature - my_temperature;
    ///hs_stored < final_temperature
    ///this val > 0
    float hs_to_final = final_temperature - hs_stored;

    float htr_1 = -component_to_final * heat_coeff * dt_s * hs.get_operating_efficiency();
    float htr_2 = hs_to_final * heat_coeff * dt_s * hs.get_operating_efficiency();*/

    //htr_1 = temperature_difference * heat_coeff * dt_s;
    //htr_2 = temperature_difference * heat_coeff * dt_s;

    ///so the problem here is that if the volume is low, we're extracting a lot of heat
    float heat_transfer_rate = temperature_difference * heat_coeff * dt_s;

    if(temperature_difference > 0)
    {
        c.remove_heat_from_me(heat_transfer_rate);
        uniform_add_stored_heat(hs, heat_transfer_rate);
    }
    else
    {
        c.add_heat_to_me(-heat_transfer_rate);
        uniform_remove_stored_heat(hs, -heat_transfer_rate);
    }

    ///temp went from c to hs
    if(temperature_difference > 0)
    {
        float stored_temp = uniform_get_stored_temperature(hs);

        if(stored_temp > final_temperature && stored_temp > c.get_my_temperature())
        {
            float diff = stored_temp - final_temperature;

            float heat = diff * hs_h_x_v;

            uniform_remove_stored_heat(hs, heat);
            c.my_temperature = final_temperature;
        }
    }
    else
    {
        float stored_temp = c.get_my_temperature();

        if(stored_temp > final_temperature && stored_temp > uniform_get_stored_temperature(hs))
        {
            float diff = final_temperature - uniform_get_stored_temperature(hs);

            float heat = diff * hs_h_x_v;

            uniform_add_stored_heat(hs, heat);
            c.my_temperature = final_temperature;
        }
    }
}

void apply_heat_damage(ship& s, double dt_s)
{
    if(!s.is_ship)
        return;

    ///applies heat damage
    for(component& c : s.components)
    {
        std::pair<material_dynamic_properties, material_fixed_properties> props = get_material_composite(c.composition);

        material_fixed_properties& fixed = props.second;

        float current_temperature = c.get_my_temperature();

        if(current_temperature >= fixed.melting_point)
        {
            float damage_max = fixed.melting_point * 2;
            float damage_min = fixed.melting_point;

            float damage_fraction = (current_temperature - damage_min) / (damage_max - damage_min);

            damage_fraction = clamp(damage_fraction, 0, 1);

            float real_damage = 1 * damage_fraction * dt_s;

            if(c.has(component_info::HP))
            {
                does_dynamic& d = c.get_dynamic(component_info::HP);
                const does_fixed& fix = c.get_fixed(component_info::HP);

                apply_to_does(-real_damage, d, fix);
            }
        }

        for(ship& ns : c.stored)
        {
            apply_heat_damage(ns, dt_s);
        }
    }
}

void ship::handle_heat(double dt_s)
{
    std::vector<double> all_produced = sum<double>([](component& c)
                                                   {
                                                       return c.get_produced();
                                                   });

    std::vector<double> all_needed = sum<double>([](component& c)
                                                   {
                                                       return c.get_needed();
                                                   });

    /*for(component& c : components)
    {
        std::cout << "s " << c.my_temperature << std::endl;
    }*/

    #if 0
    //radar.add_simple_collideable(this);

    double min_heat = 150;
    //double max_heat = 1000;


    double excess_power = all_needed[component_info::POWER];

    if(excess_power < 0)
        excess_power = 0;

    double power = all_produced[component_info::POWER] - excess_power;

    ///heat at max power instead?

    double power_to_heat = 200;
    double coolant_to_heat_drain = 200;

    double heat_intensity = min_heat + power_to_heat * power;

    double thrust_to_heat = 500;

    double thrust_produced = all_produced[component_info::THRUST];

    double heat_drained = all_produced[component_info::COOLANT] * coolant_to_heat_drain;

    heat_intensity += thrust_to_heat * thrust_produced;

    latent_heat += heat_intensity - heat_drained;

    if(latent_heat < 0)
        latent_heat = 0;

    float emitted = latent_heat * HEAT_EMISSION_FRAC;

    alt_frequency_packet heat;
    heat.frequency = HEAT_FREQ;
    heat.intensity = emitted;

    if(!model)
        radar.emit(heat, r.position, *this);
    else
        radar.emit_with_imaginary_packet(heat, r.position, *this, model);

    latent_heat -= emitted;
    #endif // 0

    //double produced_heat = 0;

    for(component& c : components)
    {
        const component_fixed_properties& fixed = c.get_fixed_props();


        //double heat = c.heat_produced_at_full_usage * std::min(c.last_sat, c.last_production_frac * c.activation_level);

        double heat = fixed.get_heat_produced_at_full_usage(c.current_scale) * c.get_operating_efficiency();

        /*if(c.long_name == "Power Generator")
        {
            std::cout << "HEAT " << heat << std::endl;
        }*/

        if(fixed.production_heat_scales)
        {
            if(fixed.primary_type == component_info::COUNT)
                throw std::runtime_error("Bad primary type");

            double produced_comp = all_produced[fixed.primary_type];
            double excess_comp = all_needed[fixed.primary_type];

            if(produced_comp > 0.001)
            {
                if(excess_comp < 0)
                    excess_comp = 0;

                double unused_frac = excess_comp / produced_comp;

                if(unused_frac < 0 || unused_frac > 1)
                {
                    printf("errr in unused frac? %lf\n", unused_frac);
                }

                heat = heat * (1 - unused_frac);
            }
        }

        c.add_heat_to_me(heat * dt_s);
        //produced_heat += heat;
    }

    //std::cout << "PHEAT " << produced_heat << std::endl;

    /*double coolant_to_heat_drain = 100;
    double heat_drained = all_produced[component_info::COOLANT] * coolant_to_heat_drain;

    produced_heat -= heat_drained;*/

    #if 0
    latent_heat += produced_heat * dt_s;

    if(latent_heat < 0)
        latent_heat = 0;

    ///radiated heat is fairly minimal
    float emitted = latent_heat * HEAT_EMISSION_FRAC;

    alt_frequency_packet heat;
    heat.frequency = HEAT_FREQ;
    heat.intensity = emitted * 10;

    alt_radar_field& radar = get_radar_field();

    if(!model)
        radar.emit(heat, r.position, *this);
    else
        radar.emit_with_imaginary_packet(heat, r.position, *this, model);

    latent_heat -= emitted;
    #endif // 0

    /*float produced_heat_ps = produced_heat * dt_s + latent_heat;

    for(component& c : components)
    {
        if(!c.heat_sink)
            continue;

        c.add_heat_to_stored(produced_heat_ps / num_hs);
    }*/

    std::vector<component*> heat_sinks;

    for(component& c : components)
    {
        const component_fixed_properties& fixed = c.get_fixed_props();

        if(!fixed.heat_sink)
            continue;

        heat_sinks.push_back(&c);
    }

    float my_vol = get_my_volume();

    ///long term use blueprint
    for(component& c : components)
    {
        const component_fixed_properties& fixed = c.get_fixed_props();

        float latent_fraction = (my_vol * latent_heat / components.size()) * 1/10.f;

        if(!fixed.heat_sink || c.get_stored_volume() < 0.1)
            c.add_heat_to_me(latent_fraction);
        else
            c.add_heat_to_stored(latent_fraction);
    }

    float heat_coeff = 1;

    ///equalise heat between storage and stored
    for(component& c : components)
    {
        if(c.stored.size() == 0 || c.get_stored_volume() < 0.1)
            continue;

        /*float my_temperature = c.get_my_temperature();

        float stored_temperature = c.get_stored_temperature();

        float temperature_difference = my_temperature - stored_temperature;

        float heat_transfer_rate = temperature_difference * heat_coeff * dt_s;

        if(c.base_id == component_type::REFINERY)
        {
            std::cout << "my temp " << my_temperature << " their " << stored_temperature << std::endl;
        }


        ///my_temperature > stored_temperature
        if(temperature_difference > 0)
        {
            c.remove_heat_from_me(heat_transfer_rate);
            c.add_heat_to_stored(heat_transfer_rate);
        }

        if(temperature_difference < 0)
        {
            c.remove_heat_from_stored(-heat_transfer_rate);
            c.add_heat_to_me(-heat_transfer_rate);
        }*/

        int store_num = 0;

        c.for_each_stored([&](component&)
                          {
                              store_num++;
                          });

        c.for_each_stored([&](component& store)
        {
            heat_transfer(heat_coeff / store_num, dt_s, c, store, 0);
        });
    }

    ///takes heat from a component and puts it into the heat sink
    for(component& c : components)
    {
        const component_fixed_properties& fixed = c.get_fixed_props();

        if(fixed.heat_sink)
            continue;

        for(component* hsp : heat_sinks)
        {
            component& hs = *hsp;

            heat_transfer(heat_coeff, dt_s, c, hs, 1);
        }
    }

    ///radiators
    ///transfers heat from heatsink to radiator, radiates into space
    double heat_to_radiate = 0;

    for(component& c : components)
    {
        if(!c.has(component_info::RADIATOR))
            continue;

        const does_fixed& d = c.get_fixed(component_info::RADIATOR);

        for(component* hsp : heat_sinks)
        {
            component& hs = *hsp;

            /*float hs_stored = uniform_get_stored_temperature(hs);

            ///ok so due to the above big block we already transfer heat from me to them, but need to do vice versa

            float temperature_difference = my_temperature - hs_stored;

            if(temperature_difference >= 0)
                continue;

            float total_heat_x_vol = c.get_my_heat_x_volume() + uniform_get_heat_x_volume(hs);

            if(total_heat_x_vol < 0.00001)
                continue;

            float final_temperature = (c.get_my_heat_x_volume() * c.get_my_temperature() + uniform_get_heat_x_volume(hs) * uniform_get_stored_temperature(hs)) / (total_heat_x_vol);

            ///my temperature < final temperature
            ///this val > 0
            float radiator_to_final = final_temperature - my_temperature;
            ///hs_stored > final_temperature
            ///this val < 0
            float hs_to_final = final_temperature - hs_stored;

            float htr_1 = radiator_to_final * heat_coeff * dt_s * c.get_operating_efficiency() / heat_sinks.size();
            float htr_2 = -hs_to_final * heat_coeff * dt_s * c.get_operating_efficiency() / heat_sinks.size();

            c.add_heat_to_me(htr_1);
            uniform_remove_stored_heat(hs, htr_2);*/

            heat_transfer(d.recharge * heat_coeff * c.get_operating_efficiency() / heat_sinks.size(), dt_s, c, hs, -1);
        }

        float heat_transfer_rate = c.get_my_temperature() * heat_coeff * dt_s * d.recharge * c.get_operating_efficiency();

        c.remove_heat_from_me(heat_transfer_rate);

        heat_to_radiate += heat_transfer_rate;
    }

    latent_heat = 0;

    apply_heat_damage(*this, dt_s);

    {
        alt_radar_field& radar = *current_radar_field;

        alt_frequency_packet heat;
        heat.make(permanent_heat + heat_to_radiate * 100, HEAT_FREQ);

        radar.emit(heat, r.position, *this);
    }

    //std::cout << "lheat " << latent_heat << std::endl;

    //dissipate();

    /*float emitted = latent_heat * 0.01;

    alt_radar_field& radar = get_radar_field();

    alt_frequency_packet heat;
    heat.frequency = HEAT_FREQ;
    heat.intensity = permanent_heat + emitted;

    radar.emit(heat, r.position, *this);

    latent_heat -= emitted;*/
}

std::vector<component> ship::handle_degredation(double dt_s)
{
    std::vector<component> to_ret;

    float phase_coeff = 1;

    if(!is_ship)
    {
        for(component& c : components)
        {
            ///???????
            if(c.base_id != component_type::MATERIAL)
                continue;

            auto [dyn, fixed] = get_material_composite(c.composition);

            float max_temp = fixed.melting_point;

            ///when things are solid and above melting point, break into individual components of material
            ///and store
            ///storage function should automatically handle concatinating liquids together
            if(c.my_temperature > max_temp && c.phase == 0)
            {
                float heat = fixed.specific_heat * (c.my_temperature - max_temp) * dyn.volume;

                c.my_temperature = max_temp;

                ///using specific heat here as a proxy to energy of.. make phase change
                float liquify_volume = heat / fixed.specific_heat;

                ///at a temperature difference of 1, it'll take 1 seconds to liquify one unit volume?
                liquify_volume = clamp(liquify_volume, 0, dyn.volume) * phase_coeff * dt_s;

                auto removed = c.remove_composition(liquify_volume);

                /*component next = get_component_default(component_type::MATERIAL, 1);

                next.composition = removed;
                next.my_temperature = c.my_temperature + 1;
                next.phase = 1;

                float total_to_add = 0;

                ///I hate floating point numbers
                for(material& m : next.composition)
                {
                    m.dynamic_desc.volume *= 0.99;
                    total_to_add += m.dynamic_desc.volume;
                }

                if(total_to_add <= 0.0001)
                    continue;

                to_ret.push_back(next);*/

                for(const material& m : removed)
                {
                    component next = get_component_default(component_type::MATERIAL, 1);

                    next.composition = {m};
                    next.my_temperature = c.my_temperature + 1;
                    next.phase = 1;

                    to_ret.push_back(next);
                }
            }

            ///ok so every liquid should be a separate component? Or do I just not use a
            ///temperature system and manually create alloys or something? Not a huge fan of that idea
            if(c.my_temperature < max_temp && c.phase == 1)
            {
                float heat = fixed.specific_heat * (max_temp - c.my_temperature) * dyn.volume;

                c.my_temperature = max_temp;

                float liquify_volume = heat / fixed.specific_heat;

                liquify_volume = clamp(liquify_volume, 0, dyn.volume) * phase_coeff * dt_s;

                auto removed = c.remove_composition(liquify_volume);

                component next = get_component_default(component_type::MATERIAL, 1);

                next.composition = removed;
                next.my_temperature = c.my_temperature - 1;
                next.phase = 0;

                next.long_name = c.long_name;
                next.short_name = c.short_name;

                float total_to_add = 0;

                ///I hate floating point numbers
                for(material& m : next.composition)
                {
                    total_to_add += m.dynamic_desc.volume;
                }

                if(total_to_add <= 0.0001)
                    continue;

                to_ret.push_back(next);
            }
        }
    }

    for(component& c : components)
    {
        std::vector<component> extra;

        for(ship& s : c.stored)
        {
            auto temp = s.handle_degredation(dt_s);

            for(auto& i : temp)
            {
                extra.push_back(i);
            }
        }

        for(auto& i : extra)
        {
            c.store(i, true);
        }
    }

    return to_ret;
}

void ship::add(const component& c)
{
    components.push_back(c);
}

void ship::set_thrusters_active(double active)
{
    thrusters_active = active;
}

/*std::string ship::show_components()
{
    std::vector<std::string> strings;

    strings.push_back("HP");

    for(component& c : components)
    {
        std::string str = to_string_with(c.hp, 1);

        strings.push_back(str);
    }

    std::string ret;

    for(auto& i : strings)
    {
        ret += format(i, strings) + "\n";
    }

    return ret;
}*/

void component::render_inline_stats()
{
    std::string rname = long_name;

    ImGui::Text(rname.c_str());

    /*for(does& d : info)
    {
        std::string dname = component_info::dname[d.type] + " " + to_string_with_variable_prec(d.recharge);

        ImGui::Text(dname.c_str());
    }*/

    std::vector<double> net;
    std::vector<double> stored;

    net.resize(component_info::COUNT);
    stored.resize(component_info::COUNT);

    const component_fixed_properties& fixed = get_fixed_props();

    for(const does_fixed& unscaled : fixed.d_info)
    {
        does_fixed d = scale(unscaled);

        net[d.type] += d.recharge + d.recharge_unconditional;
        stored[d.type] += d.capacity;
    }

    for(int i=0; i < (int)component_info::COUNT; i++)
    {
        if(net[i] == 0)
            continue;

        std::string dname = component_info::dname[i] + " " + to_string_with(net[i], 1, true);

        if(net[i] < 0)
            ImGui::TextColored(ImVec4(1, 0.2, 0.2, 1), dname.c_str());
        if(net[i] > 0)
            ImGui::TextColored(ImVec4(0.2, 1, 0.2, 1), dname.c_str());

        /*std::string dname = component_info::dname[i];

        ImGui::Text(dname.c_str());

        ImGui::SameLine();

        std::string net_v = to_string_with(net[i], 1, true);

        if(net[i] < 0)
            ImGui::TextColored(ImVec4(1, 0.2, 0.2, 1), net_v.c_str());
        if(net[i] > 0)
            ImGui::TextColored(ImVec4(0.2, 1, 0.2, 1), net_v.c_str());*/
    }
}

std::string component::phase_string()
{
    if(phase == 1)
        return "(l)";
    if(phase == 0)
        return "(s)";

    return "(Err)";
}

std::string component::get_render_long_name()
{
    if(base_id == component_type::MATERIAL)
    {
        if(composition.size() == 0)
            return "Empty";

        if(composition.size() == 1)
            return material_info::long_names[composition[0].type];
    }

    return long_name;
}

struct aggregate_ship_info
{
    double total_vol = 0;
    double total_temp = 0;
    int num = 0;
    bool processed = false;
};

void component::handle_drag_drop(size_t parent_id)
{
    if(ImGui::BeginDragDropTarget())
    {
        if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SHIPPY"))
        {
            if(!payload->Data)
            {
                throw std::runtime_error("Bad Payload");
            }

            drag_drop_data data;
            memcpy(&data, payload->Data, sizeof(data));

            if(data.type == drag_drop_info::UNIT)
            {
                pending_transfer tran;
                tran.pid_ship_from = data.id;
                tran.pid_ship_to = parent_id;
                tran.pid_component = _pid;

                client_pending_transfers().push_back(tran);
            }

            if(data.type == drag_drop_info::FRACTIONAL)
            {
                pending_transfer tran;
                tran.pid_ship_from = data.id;
                tran.pid_ship_to = parent_id;
                tran.pid_component = _pid;
                tran.is_fractiony = true;

                client_pending_transfers().push_back(tran);
            }
        }

        ImGui::EndDragDropTarget();
    }
}

void component::render_inline_ui(size_t parent_id, bool use_title, bool drag_drop)
{
    /*std::string total = "Storage: " + to_string_with(get_stored_volume()) + "/" + to_string_with_variable_prec(internal_volume);

    ImGui::Text(total.c_str());

    for(component& stored : stored)
    {
        std::string str = "    " + stored.long_name;

        str += " (" + to_string_with(stored.get_my_volume()) + ") " + to_string_with(stored.get_my_temperature()) + "K";

        ImGui::Text(str.c_str());
    }*/

    //ImGui::Text(("Container " + std::to_string(_pid)).c_str());

    ImGui::BeginGroup();

    if(use_title)
        ImGui::Text((long_name).c_str());

    if(get_internal_volume() > 0)
        foreign_access.render_ui(*this);

    std::map<std::string, aggregate_ship_info> aggregates;

    for(ship& s : stored)
    {
        if(!s.is_ship)
            continue;

        if(s.blueprint_name == "")
            continue;

        aggregate_ship_info& inf = aggregates[s.blueprint_name];

        inf.num++;
        inf.total_vol += s.get_my_volume();
        inf.total_temp += s.get_max_temperature();
    }

    for(ship& s : stored)
    {
        if(!s.is_ship)
        {
            for(component& store : s.components)
            {
                std::string name = store.long_name;

                float val = store.get_my_volume();
                float temperature = store.get_my_temperature();

                //ImGui::Text("Fluid:");

                std::string ext = store.phase_string();

                std::string ext_str = std::to_string(_pid) + "." + std::to_string(store._pid);

                {
                    ImGui::PushItemWidth(80);

                    ImGuiX::SliderFloat("##riup" + ext_str, &val, 0, get_internal_volume(), "%.1f");

                    ImGui::PopItemWidth();
                }

                ImGui::SameLine();

                {
                    ImGui::PushItemWidth(80);

                    ImGuiX::SliderFloat("##riug" + ext_str, &temperature, 0, 6000, "%.0fK " + ext);

                    ImGui::PopItemWidth();
                }

                ImGui::SameLine();

                //ImGui::Text(store.get_render_long_name().c_str());

                ImGui::Button((store.get_render_long_name() + "##" + std::to_string(store._pid)).c_str());

                if(ImGui::IsItemActive() && ImGui::BeginDragDropSource())
                {
                    drag_drop_data data;
                    data.id = s._pid;
                    data.type = drag_drop_info::FRACTIONAL;

                    ImGui::SetDragDropPayload("SHIPPY", &data, sizeof(data));

                    ImGui::EndDragDropSource();
                }
            }
        }
        else
        {
            auto agg_it = aggregates.find(s.blueprint_name);

            std::string name = "Ship";

            float val = s.get_my_volume();
            float temperature = s.get_max_temperature();

            int num = 1;

            if(agg_it != aggregates.end())
            {
                if(agg_it->second.processed)
                    continue;

                num = agg_it->second.num;
                assert(num > 0);

                val = agg_it->second.total_vol;
                temperature = agg_it->second.total_temp / num;
                agg_it->second.processed = true;
            }

            std::string ext_str = std::to_string(_pid) + "." + std::to_string(s._pid);

            {
                ImGui::PushItemWidth(80);

                ImGuiX::SliderFloat("##riup" + ext_str, &val, 0, get_internal_volume(), "%.1f");

                ImGui::PopItemWidth();
            }

            ImGui::SameLine();

            {
                ImGui::PushItemWidth(80);

                ImGuiX::SliderFloat("##riug" + ext_str, &temperature, 0, 6000, "%.0fK");

                ImGui::PopItemWidth();
            }

            ImGui::SameLine();

            ImGui::Button((s.blueprint_name + "##" + std::to_string(s._pid)).c_str());

            if(ImGui::IsItemActive() && ImGui::BeginDragDropSource())
            {
                drag_drop_data data;
                data.id = s._pid;
                data.type = drag_drop_info::UNIT;

                ImGui::SetDragDropPayload("SHIPPY", &data, sizeof(data));

                ImGui::EndDragDropSource();
            }

            if(num > 1)
            {
                ImGui::SameLine();

                ImGui::Text(("x" + std::to_string(num)).c_str());
            }
        }
    }

    ImGui::EndGroup();

    if(drag_drop)
    {
        handle_drag_drop(parent_id);
    }
}

void component::change_access_permissions(access_permissions::state st)
{
    if(st == access_permissions::STATE_ALL || st == access_permissions::STATE_NONE)
        foreign_access.access = st;
}

void component::manufacture_blueprint_id(size_t blue_id)
{
    unchecked_blueprints.push_back(blue_id);

    if(unchecked_blueprints.size() > 100)
        unchecked_blueprints.resize(100);
}

void component::manufacture_blueprint(const blueprint& blue, ship& parent)
{
    if(has_tag(tag_info::TAG_EFACTORY) && parent.my_room == nullptr)
        return;

    component* which = nullptr;

    if(!has_tag(tag_info::TAG_EFACTORY))
    {
        bool any_free_space = false;

        float max_space = 0;

        for(component& lc : parent.components)
        {
            if(lc.base_id != component_type::CARGO_STORAGE)
                continue;

            float space = lc.get_internal_volume() - lc.get_stored_volume();

            if(space > 0)
            {
                any_free_space = true;

                if(space > max_space)
                {
                    which = &lc;
                    max_space = space;
                }
            }
        }

        if(!any_free_space)
            return;

        if(!which)
            return;
    }


    ///should display a warning to the user
    /*bool any = false;

    for(component& lc : parent.components)
    {
        if(lc.base_id != component_type::CARGO_STORAGE)
            continue;

        if(blue_ship.get_my_volume() <= lc.get_internal_volume())
        {
            any = true;
            break;
        }
    }*/

    build_in_progress build;
    build.make(blue);

    ///clap of my arse etc
    ship dummy_ship;
    dummy_ship.is_build_holder = true;
    dummy_ship.is_ship = true;
    ///if i change this string, remember to change the stripping in design_editor.cpp
    dummy_ship.blueprint_name = "UNFINISHED_" + blue.name;
    dummy_ship.original_blueprint = std::make_shared<blueprint>(blue);

    if(!has_tag(tag_info::TAG_EFACTORY))
    {
        which->stored.push_back(dummy_ship);
        build.in_progress_pid = which->stored.back()._pid;
    }
    else
    {
        ///spawn me in real space
        ship* spawned = parent.parent->take(dummy_ship);

        spawned->r.position = parent.r.position + (vec2f){40, 0}.rot(parent.r.rotation);
        spawned->r.rotation = parent.r.rotation;
        spawned->velocity = parent.velocity;
        spawned->phys_ignore.push_back(_pid);

        spawned->r.init_rectangular({1, 0.2});
        spawned->network_owner = parent.network_owner;
        spawned->spawn_clock.restart();
        spawned->spawned_by = _pid;
        spawned->phys_drag = true;
        spawned->current_radar_field = parent.current_radar_field;
        build.in_progress_pid = spawned->_pid;
    }

    building = true;
    build_queue.push_back(std::make_shared<build_in_progress>(build));
}

void component::render_manufacturing_window(size_t parent_id, blueprint_manager& blueprint_manage, ship& parent, std::vector<ship>& nearby_unfinished)
{
    if(!factory_view_open)
        return;

    std::string name = long_name + "##" + std::to_string(_pid);

    ImGui::Begin(name.c_str(), &factory_view_open, ImGuiWindowFlags_AlwaysAutoResize);

    render_inline_ui(parent_id);

    ///construction queue

    ImGui::Text("In progress");

    ImGui::Indent();

    std::set<size_t> pids_going;

    //if(ImGui::TreeNodeEx("In Progress", ImGuiTreeNodeFlags_Leaf))
    {
        for(int i=0; i < (int)build_queue.size(); i++)
        {
            float work = get_build_work(build_queue[i]->result.to_ship());

            std::string str = build_queue[i]->result.name;

            ship* fship = dynamic_cast<ship*>(find_by_id(parent, build_queue[i]->in_progress_pid));

            if(fship == nullptr)
            {
                for(auto& fs : nearby_unfinished)
                {
                    if(build_queue[i]->in_progress_pid == fs._pid)
                    {
                        fship = &fs;
                    }
                }
            }

            if(fship != nullptr)
            {
                pids_going.insert(fship->_pid);

                if(fship->get_my_volume() > 0)
                {
                    str += " " + to_string_with(100 * fship->get_my_volume() / work) + "%%";
                }
            }

            ImGui::Text(str.c_str());

            ImGui::SameLine();

            if(ImGuiX::SimpleButton("(Cancel)"))
            {
                parent.cancel_building_rpc(this->_pid, build_queue[i]->in_progress_pid);
            }
        }

        //ImGui::TreePop();
    }

    ImGui::Unindent();

    ImGui::NewLine();

    ImGui::Text("Unfinished");

    ImGui::Indent();

    {
        std::vector<ship*> check_resumable;

        for(component& pc : parent.components)
        {
            for(ship& s : pc.stored)
            {
                check_resumable.push_back(&s);
            }
        }

        for(auto& i : nearby_unfinished)
        {
            check_resumable.push_back(&i);
        }

        for(ship* s : check_resumable)
        {
            if(s->is_ship && s->is_build_holder)
            {
                if(pids_going.find(s->_pid) == pids_going.end())
                {
                    float work = get_build_work(s->original_blueprint->to_ship());

                    std::string sbutt = "Resume##a" + std::to_string(s->_pid);

                    std::string str = s->blueprint_name;

                    str += " " + to_string_with(100 * s->construction_amount / work) + "%%";

                    ImGui::Text(str.c_str());

                    ImGui::SameLine();

                    if(ImGuiX::SimpleButton("(Resume)"))
                    {
                        parent.resume_building_rpc(this->_pid, s->_pid);
                    }
                }
            }
        }
    }

    ImGui::Unindent();

    ImGui::NewLine();

    ImGui::Text("Blueprints");

    std::vector<std::vector<material>> in_storage;

    for(component& lc : parent.components)
    {
        if(lc.base_id != component_type::CARGO_STORAGE)
            continue;

        lc.for_each_stored([&](component& c)
        {
            if(c.base_id == component_type::MATERIAL)
                in_storage.push_back(c.composition);
        });
    }

    material_deduplicate(in_storage);

    for(blueprint& blue : blueprint_manage.blueprints)
    {
        if(ImGui::TreeNode(blue.name.c_str()))
        {
            ship s = blue.to_ship();

            render_ship_cost(s, in_storage);

            std::vector<std::vector<material>> cost = blue.get_cost();

            std::string time_to_build = to_string_with_variable_prec(blue.get_build_time_s(get_fixed(component_info::MANUFACTURING).recharge));

            if(material_satisfies(cost, in_storage))
            {
                if(ImGui::Button("Build"))
                {
                    ///OOPS THIS NEEDS TO BE DONE ON THE SERVER
                    /*building = true;
                    build_queue.push_back(blue.to_ship());*/

                    manufacture_blueprint_id_rpc(blue._pid);

                    /*for_each_stored([&](component& c)
                    {
                        if(c.base_id == component_type::MATERIAL)
                            material_partial_deplete(c.composition, cost);
                    });*/
                }
            }
            else
            {
                //ImGui::Button("Insufficient Resources");

                if(ImGui::Button("Build (Insufficient Resources)"))
                {
                    manufacture_blueprint_id_rpc(blue._pid);
                }
            }

            ImGui::SameLine();

            ImGui::Text((time_to_build + " (s)").c_str());

            ///future james
            ///some sort of resources_satisfied
            ///if true colour button and make clickable

            ImGui::TreePop();
        }
    }

    ImGui::End();
}

/*void component::transfer_stored_from_to(size_t pid_ship_from, size_t pid_component_to)
{
    transfers.push_back({pid_ship_from, pid_component_to});

    while(transfers.size() > 100)
        transfers.erase(transfers.begin());
}

void component::transfer_stored_from_to_frac(size_t pid_ship_from, size_t pid_component_to, float frac)
{
    transfers.push_back({pid_ship_from, pid_component_to, frac, true});

    while(transfers.size() > 100)
        transfers.erase(transfers.begin());
}*/

void ship::show_resources(bool window)
{
    std::vector<std::string> strings;

    strings.push_back("Status");

    for(auto& i : component_info::dname)
    {
        strings.push_back(i);
    }

    /*std::vector<double> needed;
    needed.resize(component_info::COUNT);

    for(component& c : components)
    {
        auto them = c.get_needed();

        for(int i=0; i < (int)them.size(); i++)
        {
            needed[i] += them[i];
        }
    }*/

    std::vector<double> needed = sum<double>
                                ([](component& c)
                                 {
                                    return c.get_needed();
                                 });

    std::vector<std::string> status;
    status.push_back("Net");

    for(auto& i : needed)
    {
        status.push_back(to_string_with(i, 2, true));
    }

    std::vector<std::string> caps;
    caps.push_back("Max");

    std::vector<double> max_capacity = sum<double>
                                ([](component& c)
                                 {
                                    return c.get_capacity();
                                 });

    for(auto& i : max_capacity)
    {
        caps.push_back(to_string_with_variable_prec(i));
    }


    std::vector<std::string> cur;
    cur.push_back("Cur");

    /*for(auto& i : resource_status)
    {
        cur.push_back(to_string_with(i, 1));
    }*/

    std::vector<double> cur_held = sum<double>
                                    ([](component& c)
                                     {
                                         return c.get_held();
                                     });

    for(auto& i : cur_held)
    {
        cur.push_back(to_string_with(i, 1));
    }

    assert(strings.size() == status.size());
    assert(strings.size() == cur.size());
    assert(strings.size() == caps.size());

    std::string ret;

    for(int i=0; i < (int)strings.size(); i++)
    {
        ret += format(strings[i], strings) + ": " + format(status[i], status) + " | " + format(cur[i], cur) + " / " + format(caps[i], caps) + "\n";
    }

    ret += "Heat: " + to_string_with_variable_prec(heat_to_kelvin(latent_heat)) + "\n";

    if(window)
        ImGui::Begin((std::string("Tship##") + std::to_string(_pid)).c_str());

    ImGui::Text(ret.c_str());

    for(component& c : components)
    {
        if(!c.is_storage())
            continue;

        const component_fixed_properties& fixed = c.get_fixed_props();

        float max_stored = fixed.get_internal_volume(c.current_scale);
        float cur_stored = c.get_stored_volume();

        //ret += "Stored: " + to_string_with_variable_prec(cur_stored) + "/" + to_string_with_variable_prec(max_stored) + "\n";

        ImGui::Text((c.long_name + ": " + to_string_with_variable_prec(cur_stored) + "/" + to_string_with_variable_prec(max_stored)).c_str());

        if(ImGui::IsItemClicked(0))
        {
            c.detailed_view_open = !c.detailed_view_open;
        }
    }

    for(storage_pipe& p : pipes)
    {
        std::string str = "Pipe " + std::to_string(p.id_1) + "/" + std::to_string(p.id_2);

        ImGui::Text(str.c_str());

        if(ImGui::IsItemClicked(0))
        {
            p.is_open = !p.is_open;
        }
    }

    for(component& c : components)
    {
        if(!c.detailed_view_open)
            continue;

        ImGui::Begin((c.long_name + "##" + std::to_string(c._pid)).c_str(), &c.detailed_view_open);

        c.render_inline_ui(_pid);

        ImGui::End();
    }

    for(storage_pipe& p : pipes)
    {
        if(!p.is_open)
            continue;

        std::string wstring = "PPipe##" + std::to_string(p._pid);

        ImGui::Begin(wstring.c_str(), &p.is_open);

        std::string conn_str = "Connects pipe " + std::to_string(p.id_1) + " with " + std::to_string(p.id_2);

        ImGui::Text(conn_str.c_str());

        auto c1 = get_component_from_id(p.id_1);
        auto c2 = get_component_from_id(p.id_2);

        if(c1 && c2 && !p.goes_to_space)
        {
            c1.value()->render_inline_ui(_pid);
            c2.value()->render_inline_ui(_pid);

            ///the problem with this is that its not being communicated back to the server
            bool changed = ImGuiX::SliderFloat("", &p.flow_rate, -p.max_flow_rate, p.max_flow_rate);
            //ImGui::DragFloat("", &p.flow_rate, 0.01f, -p.max_flow_rate, p.max_flow_rate);

            if(changed)
                p.set_flow_rate_rpc(p.flow_rate);
        }

        if(c1 && p.goes_to_space)
        {
            c1.value()->render_inline_ui(_pid);

            ///have sun temperature here
            ImGui::Text("Space");

            bool changed = ImGuiX::SliderFloat("", &p.flow_rate, 0, p.max_flow_rate);

            if(changed)
                p.set_flow_rate_rpc(p.flow_rate);
        }

        ImGui::End();
    }

    if(window)
        ImGui::End();
}

ImVec4 im4_mix(ImVec4 one, ImVec4 two, float a)
{
     return ImVec4(mix(one.x, two.x, a),
                   mix(one.y, two.y, a),
                   mix(one.z, two.z, a),
                   mix(one.w, two.w, a));
}

void ship::show_power()
{
    std::string ppower = "Status##" + std::to_string(_pid);

    ImGui::Begin(ppower.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    std::vector<component*> ui_sorted;

    for(component& c : components)
    {
        ui_sorted.push_back(&c);
    }

    std::sort(ui_sorted.begin(), ui_sorted.end(), [](component* c1, component* c2)
    {
        const component_fixed_properties& fixed1 = c1->get_fixed_props();
        const component_fixed_properties& fixed2 = c2->get_fixed_props();

        return fixed1.activation_type > fixed2.activation_type;
    });

    for(component* pc : ui_sorted)
    {
        component& c = *pc;

        std::string name = c.long_name;

        float as_percentage = c.activation_level * 100;

        std::string temperature = to_string_with(c.get_my_temperature());

        std::pair<material_dynamic_properties, material_fixed_properties> props = get_material_composite(c.composition);

        material_fixed_properties& fixed = props.second;

        float current_temperature = c.get_my_temperature();

        bool changed = false;

        ImVec4 default_slider_col = ImGui::GetLinearStyleColorVec4(ImGuiCol_SliderGrab);
        ImVec4 red_col = ImVec4(0.7, 0.1, 0.1, 1);

        ImGui::BeginGroup();

        //ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        ///HP
        {
            float hp = c.get_hp_frac() * 100;

            ImVec4 ccol = im4_mix(default_slider_col, red_col, 1 - c.get_hp_frac());

            ImVec4 text_col = im4_mix(ImVec4(1,1,1,1), red_col, 1 - c.get_hp_frac());
            ImVec4 rtext_col = im4_mix(ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab), red_col, 1 - c.get_hp_frac());

            if((c.base_id == component_type::S_DRIVE || c.base_id == component_type::W_DRIVE) && c.activation_level > 0)
            {
                bool success = (c.base_id == component_type::S_DRIVE && has_s_power) || (c.base_id == component_type::W_DRIVE && has_w_power);

                text_col = success ? colours::pastel_green : colours::pastel_red;
            }

            ImGui::PushLinearStyleColor(ImGuiCol_Text, text_col);

            //ImGui::PushLinearStyleColor(ImGuiCol_SliderGrab, ccol);
            //text_col.w = 40. / 255.;

            rtext_col.w = 0.5;

            ImGui::PushItemWidth(80);

            std::string str = "##t" + std::to_string(c._pid);

            ImGuiX::ProgressBarPseudo(hp/100, ImVec2(80, 20), name, rtext_col);

            //ImGuiX::SliderFloat("##t" + std::to_string(c._pid), &hp, 0, 100, "%.0f%%");

            ImGui::PopItemWidth();

            if(ImGui::IsItemHovered())
            {
                std::string hp_str = "HP: " + to_string_with(c.get_hp_frac() * 100, 1) + "%%";

                ImGui::SetTooltip(hp_str.c_str());
            }

            ImGui::PopStyleColor(1);
        }

        ImGui::SameLine();

        ///temperature
        {
            float min_bad_temp = fixed.melting_point * 0.8;
            float max_bad_temp = fixed.melting_point;

            float overall_frac = current_temperature / max_bad_temp;

            overall_frac = clamp(overall_frac, 0, 1);

            float bad_fraction = (current_temperature - min_bad_temp) / (max_bad_temp - min_bad_temp);

            float good_fraction = clamp(bad_fraction, 0, 1);

            bad_fraction = clamp(1 - bad_fraction, 0, 1);

            ImVec4 ccol = im4_mix(default_slider_col, red_col, good_fraction);
            ImVec4 progress_col = im4_mix(default_slider_col, red_col, overall_frac);

            ImGui::PushLinearStyleColor(ImGuiCol_Text, ImVec4(1,bad_fraction,bad_fraction,1));
            ImGui::PushLinearStyleColor(ImGuiCol_SliderGrab, ccol);

            ImGui::PushItemWidth(80);

            std::string fmt_string = "%.0fK";

            if(current_temperature > fixed.melting_point)
                fmt_string = "!" + fmt_string + "!";
            if(current_temperature > fixed.melting_point * 1.2)
                fmt_string = "!" + fmt_string + "!";
            if(current_temperature > fixed.melting_point * 1.5)
                fmt_string = "!" + fmt_string + "!";

            //ImGuiX::ProgressBarPseudo(overall_frac, ImVec2(80, 3), "", progress_col);

            ImGuiX::SliderFloat("##b" + std::to_string(c._pid), &current_temperature, 0, fixed.melting_point, fmt_string);

            ImGui::PopItemWidth();

            ImGui::PopStyleColor(2);
        }

        //ImGui::PopStyleVar(1);

        ImGui::EndGroup();

        const component_fixed_properties& fixed_properties = c.get_fixed_props();

        ///render sliders
        {
            if(fixed_properties.activation_type == component_info::SLIDER_ACTIVATION)
            {
                ImGui::SameLine();

                ImGui::PushItemWidth(80);

                changed = ImGuiX::SliderFloat("##" + std::to_string(c._pid), &as_percentage, 0, 100, "%.0f");

                ImGui::PopItemWidth();
            }

            if(fixed_properties.activation_type == component_info::TOGGLE_ACTIVATION)
            {
                ImGui::SameLine();

                bool enabled = as_percentage == 100;

                changed = ImGui::Checkbox(("##" + std::to_string(c._pid)).c_str(), &enabled);

                as_percentage = ((int)enabled) * 100;
            }

            ///well
            if(fixed_properties.activation_type == component_info::NO_ACTIVATION)
            {

            }
        }

        //ImGui::SameLine();

        ///render name
        {
            if(c.has_tag(tag_info::TAG_FACTORY))
            {
                ImGui::SameLine();

                if(ImGuiX::SimpleButton("(Open)"))
                {
                    c.factory_view_open = !c.factory_view_open;
                }
            }
        }

        c.activation_level = as_percentage / 100.f;

        if(changed)
            rpc("set_activation_level", c, c.activation_level);
    }

    ImGui::NewLine();

    std::vector<std::string> formatted_names;

    for(storage_pipe& pipe : pipes)
    {
        auto c1 = get_component_from_id(pipe.id_1);

        if(!c1)
            continue;

        formatted_names.push_back(c1.value()->long_name);
    }

    for(storage_pipe& pipe : pipes)
    {
        auto c1 = get_component_from_id(pipe.id_1);
        auto c2 = get_component_from_id(pipe.id_2);
        float lbound = -pipe.max_flow_rate;

        if(pipe.goes_to_space)
        {
            lbound = 0;
        }

        ImGui::PushItemWidth(80);

        bool changed = ImGuiX::SliderFloat("##" + std::to_string(pipe._pid), &pipe.flow_rate, lbound, pipe.max_flow_rate);

        if(changed)
            pipe.set_flow_rate_rpc(pipe.flow_rate);

        ImGui::PopItemWidth();

        ImGui::SameLine();

        if(c1)
        {
            component& c = *c1.value();

            ImGui::Text((format(c.long_name, formatted_names) + " <-> ").c_str());

            //ImGui::SameLine();
        }

        if(c2 && !pipe.goes_to_space)
        {
            component& c = *c2.value();

            ImGui::SameLine();

            ImGui::Text(c.long_name.c_str());
        }
        else
        {
            ImGui::SameLine();

            ImGui::Text("Space");
        }
    }

    ImGui::NewLine();

    std::vector<std::string> all_names;

    for(component& c : components)
    {
        if(!c.is_storage())
            continue;

        all_names.push_back(c.long_name);
    }

    for(component& c : components)
    {
        if(!c.is_storage())
            continue;

        const component_fixed_properties& fixed = c.get_fixed_props();

        float max_stored = fixed.get_internal_volume(c.current_scale);
        float cur_stored = c.get_stored_volume();

        std::string full_frac = to_string_with_variable_prec(cur_stored) + "/" + to_string_with_variable_prec(max_stored);

        ImGui::BeginGroup();

        if(ImGui::TreeNodeEx((format(c.long_name, all_names) + " " + full_frac + "###TN" + std::to_string(c._pid)).c_str(), ImGuiTreeNodeFlags_None))
        {
            ImGui::Unindent();

            c.render_inline_ui(_pid, false, false);

            ImGui::Indent();

            ImGui::TreePop();
        }

        ImGui::EndGroup();

        c.handle_drag_drop(_pid);
    }

    for(component& c : components)
    {
        if(c.has(component_info::SELF_DESTRUCT))
        {
            if(ImGuiX::SimpleConfirmButton("(Destruct)"))
            {
                rpc("set_use", c);
            }
        }
    }

    ImGui::End();
}

bool ship::any_building(size_t ship_id)
{
    for(component& c : components)
    {
        if(c.base_id != component_type::FACTORY)
            continue;

        for(auto& i : c.build_queue)
        {
            if(i->in_progress_pid == ship_id)
                return true;
        }
    }

    return false;
}

float ship::get_my_volume()
{
    float vol = 0;

    for(const component& c : components)
    {
        vol += c.get_my_volume();
    }

    return vol;
}

std::vector<double> ship::get_capacity()
{
    return sum<double>
           ([](component& c)
           {
              return c.get_capacity();
           });
}

void ship::show_manufacturing_windows(blueprint_manager& blueprint_manage, std::vector<ship>& nearby_unfinished)
{
    for(component& c : components)
    {
        if(c.has_tag(tag_info::TAG_FACTORY))
        {
            c.render_manufacturing_window(_pid, blueprint_manage, *this, nearby_unfinished);
        }
    }
}

void ship::apply_force(vec2f dir)
{
    control_force += dir.rot(r.rotation);
}

void ship::apply_rotation_force(float force)
{
    control_angular_force += force;
}

float ship::get_max_angular_thrust()
{
    float mass = get_mass();

    if(room_type == space_type::REAL_SPACE)
        return get_net_resources(1, last_sat_percentage)[component_info::THRUST] * 2 / mass;

    return 0;
}

float ship::get_max_velocity_thrust()
{
    float mass = get_mass();

    if(room_type == space_type::REAL_SPACE)
        return get_net_resources(1, last_sat_percentage)[component_info::THRUST] * 2 * 20 / mass;

    return 0;
}

float ship::get_mass()
{
    float my_mass = 0;

    for(component& c : components)
    {
        auto [dyn, fixed] = get_material_composite(c.composition);

        my_mass += dyn.volume * fixed.density;
    }

    for(component& c : components)
    {
        for(ship& s : c.stored)
        {
            my_mass += s.get_mass();
        }
    }

    return my_mass;
}

float ship::get_max_temperature()
{
    float mtemp = 0;

    for(component& c : components)
    {
        mtemp = std::max(mtemp, c.get_my_temperature());
    }

    return mtemp;
}

void ship::tick_pre_phys(double dt_s)
{
    if(control_force.x() == 0 && control_force.y() == 0 && control_angular_force == 0)
        return;

    double angular_thrust = get_max_angular_thrust();
    double velocity_thrust = get_max_velocity_thrust();

    apply_inputs(dt_s, velocity_thrust, angular_thrust);
    //e.tick_phys(dt_s);
}

struct playspace_resetter
{
    ship& s;

    playspace_resetter(ship& in) : s(in){}
    ~playspace_resetter(){s.move_down = false; s.move_up = false; s.move_warp = false;}
};

void unblock_cpu_hardware(ship& s, hardware::type type)
{
    for(component& c : s.components)
    {
        if(!c.has_tag(tag_info::TAG_CPU))
            continue;

        c.cpu_core.blocking_status[(int)type] = 0;
    }
}

/*void on_enter_warp(ship& s, playspace_manager& play, playspace* left)
{

}

void on_leave_warp(ship& s, playspace_manager& play, playspace* arrive)
{

}

void on_enter_sspace(ship& s, playspace_manager& play, playspace* space)
{

}

void on_leave_sspace(ship& s, playspace_manager& play)
{

}

void on_enter_room(ship& s, playspace_manager& play, playspace* space, room* r)
{

}

void on_leave_room(ship& s, playspace_manager& play)
{

}*/

void ship_drop_to(ship& s, playspace_manager& play, playspace* space, room* r, bool disruptive = true)
{
    s.travelling_to_poi = false;

    if(r == nullptr)
    {
        s.velocity = {0,0};

        if(disruptive)
        {
            //float my_mass = s.get_mass();

            /*double health = s.get_capacity()[component_info::HP] / 2;
            s.take_damage(health, true, true);*/

            for(component& c : s.components)
            {
                if(c.base_id == component_type::S_DRIVE || c.base_id == component_type::W_DRIVE)
                {
                    does_dynamic& d = c.get_dynamic(component_info::HP);
                    const does_fixed& fix = c.get_fixed(component_info::HP);

                    float held = d.held;

                    apply_to_does(-d.held * 0.7, d, fix);

                    c.my_temperature += 500;
                }
            }
        }

        room* new_poi = space->make_room(s.r.position, 5, poi_type::DEAD_SPACE);

        play.enter_room(&s, new_poi);

        if(disruptive)
        {
            alt_frequency_packet heat;
            heat.make(20000, HEAT_FREQ);

            s.current_radar_field->emit(heat, s.r.position, s);
        }
    }

    //on_enter_room(s, play, space, r);
    unblock_cpu_hardware(s, hardware::S_DRIVE);
}

void deplete_w(ship& s)
{
    float amount = s.my_size;
    float cur = 0;

    for(component& c : s.components)
    {
        if(c.base_id == component_type::W_DRIVE)
        {
            c.try_use = true;
            c.force_use = true;
        }
    }
}

void handle_fsd_movement(double dt_s, playspace_manager& play, ship& s)
{
    if(s.room_type == space_type::REAL_SPACE)
    {
        play.exit_room(&s);
        //on_leave_room(s, play);
        return;
    }

    float drop_range = 10;

    vec2f position = s.destination_poi_position;
    vec2f my_pos = s.r.position;

    float dist = (position - my_pos).length();

    vec2f destination_vector = (position - my_pos).norm();

    ///yeah so uh this obviously isn't great
    float speed = std::max(s.get_net_resources(1, s.last_sat_percentage)[component_info::S_POWER] * 2 * 300 * 20 / s.get_mass(), 0.);

    vec2f translate = destination_vector * dt_s * speed;

    if(translate.length() > dist)
    {
        translate = translate.norm() * dist;
    }

    s.r.position += translate;
    s.r.rotation = translate.angle();

    if(dist <= drop_range)
    {
        unblock_cpu_hardware(s, hardware::S_DRIVE);

        std::optional<std::pair<playspace*, room*>> dest = play.get_room_from_id(s.destination_poi_pid);

        s.travelling_to_poi = false;

        s.velocity = {0,0};

        if(dest)
        {
            play.enter_room(&s, dest.value().second);

            //on_leave_sspace(s, play);
            //on_enter_room(s, play, dest.value().first, dest.value().second);
        }
        else
        {
            auto [nplay, r] = play.get_location_for(&s);

            room* new_poi = nplay->make_room(s.r.position, 5, poi_type::DEAD_SPACE);

            play.enter_room(&s, new_poi);

            //on_leave_sspace(s, nplay);
            //on_enter_room(s, play, nplay, new_poi);
        }
    }
}

void update_cpu_rules_and_hardware(ship& s, playspace_manager& play, playspace* space, room* r)
{
    s.consume_cpu_transfers(r);

    for(component& c : s.components)
    {
        if(!c.has_tag(tag_info::TAG_CPU))
            continue;

        if(c.get_operating_efficiency() < 0.2)
            continue;

        cpu_state& cpu = c.cpu_core;

        ///use ints
        /*if(cpu.ports[hardware::S_DRIVE].is_symbol())
        {
            std::string poi_name = cpu.ports[hardware::S_DRIVE].symbol;
        }*/

        if(cpu.ports[hardware::T_DRIVE].is_int() && cpu.ports[hardware::T_DRIVE].value > 0)
        {
            int success = 0;

            if(play.start_realspace_travel(s, cpu.my_move))
            {
                success = 1;
            }
            else
            {
                unblock_cpu_hardware(s, hardware::T_DRIVE);
            }

            cpu.context.register_states[(int)registers::TEST].set_int(success);
        }

        if(cpu.ports[hardware::S_DRIVE].is_int())
        {
            int id = cpu.ports[hardware::S_DRIVE].value;

            if(id != -1)
            {
                int success = 0;

                if(play.start_room_travel(s, id))
                {
                    success = 1;
                }
                else
                {
                    unblock_cpu_hardware(s, hardware::S_DRIVE);
                }

                cpu.context.register_states[(int)registers::TEST].set_int(success);
            }
        }

        if(cpu.ports[hardware::S_DRIVE].is_symbol())
        {
            std::string symb = cpu.ports[hardware::S_DRIVE].symbol;

            int success = 0;

            if(auto froom = play.get_room_from_symbol(space, symb); froom.has_value())
            {
                if(play.start_room_travel(s, froom.value()->_pid))
                {
                    success = 1;
                }
            }

            if(!success)
            {
                unblock_cpu_hardware(s, hardware::S_DRIVE);
            }

            cpu.context.register_states[(int)registers::TEST].set_int(success);
        }


        if(cpu.ports[hardware::W_DRIVE].is_int())
        {
            int id = cpu.ports[hardware::W_DRIVE].value;

            if(id != -1)
            {
                int success = 0;

                if(play.start_warp_travel(s, id))
                {
                    success = 1;
                }
                else
                {
                    unblock_cpu_hardware(s, hardware::W_DRIVE);
                }

                cpu.context.register_states[(int)registers::TEST].set_int(success);
            }
        }

        if(cpu.ports[hardware::W_DRIVE].is_label() || cpu.ports[hardware::W_DRIVE].is_symbol())
        {
            std::string str = cpu.ports[hardware::W_DRIVE].is_label() ? cpu.ports[hardware::W_DRIVE].label : cpu.ports[hardware::W_DRIVE].symbol;

            int success = 0;

            if(auto sp_opt = play.get_playspace_from_name(str); sp_opt.has_value())
            {
                if(play.start_warp_travel(s, sp_opt.value()->_pid))
                {
                    success = 1;
                }
            }

            if(!success)
            {
                unblock_cpu_hardware(s, hardware::W_DRIVE);
            }

            cpu.context.register_states[(int)registers::TEST].set_int(success);
        }

        cpu.ports[hardware::T_DRIVE].set_int(-1);
        cpu.ports[hardware::S_DRIVE].set_int(-1);
        cpu.ports[hardware::W_DRIVE].set_int(-1);

        cpu.waiting_for_hardware_feedback = false;
    }
}

float get_angular_control_force(float target, float my_angular_velocity, float my_angle, float max_angular_acceleration)
{
    float to_target = signed_angle_between_vectors((vec2f){1, 0}.rot(my_angle), (vec2f){1, 0}.rot(target));

    float sign_to_target = signum(to_target);

    if(fabs(my_angular_velocity) < 0.00001)
    {
        return sign_to_target * max_angular_acceleration;
    }

    if(max_angular_acceleration <= 0.000001)
        return sign_to_target * max_angular_acceleration;

    float distance_to_decel = -my_angular_velocity * my_angular_velocity / (-2 * max_angular_acceleration);

    if(distance_to_decel >= fabs(to_target) - M_PI/256 && signum(to_target) == signum(my_angular_velocity))
    {
        return -sign_to_target * max_angular_acceleration;
    }
    else
    {
        return sign_to_target * max_angular_acceleration;
    }
}

vec2f get_control_force(vec2f target, vec2f my_velocity, vec2f my_position, double max_acceleration, float lax_distance)
{
    vec2f to_target = (target - my_position).norm();

    //float linear_velocity = my_velocity.length();

    ///ok so steps
    ///take vector from me to them
    ///find velocity perpendicular to that direction
    ///cancel velocity perpendicular to that direction primarily
    ///then find out how much acceleration we have left
    ///basically: two states we need to distinguish
    ///either apply velocity in direction of target to go fast
    ///or apply velocity in opposite direction to go slow
    ///assuming available acceleration doesn't change, need to
    ///start decelerating when v^2 = u^2 + 2as says we need slightly less acceleration than we have to stop (but do even if we need more)

    vec2f control_force = {0,0};

    double remaining_control_force = max_acceleration;

    ///LEFT
    vec2f perpendicular_direction = perpendicular(to_target).norm();

    vec2f velocity_in_perp = projection(my_velocity, perpendicular_direction);

    if(velocity_in_perp.length() > 10)
    {
        double steal = remaining_control_force/2;

        remaining_control_force -= steal;

        control_force += steal * -velocity_in_perp.norm();
    }

    ///v2 = u2 + 2as
    ///v2 = 0
    ///-u2 = 2as
    ///-u2 / (2a) = stopping distance

    if(remaining_control_force <= 0.00001)
        return control_force;

    vec2f velocity_in_parallel = projection(my_velocity, to_target);

    float linear_velocity_in_parallel = velocity_in_parallel.length();

    if(linear_velocity_in_parallel < 0.00001)
    {
        control_force += to_target * remaining_control_force;
        return control_force;
    }

    float mult = 1;

    if(angle_between_vectors(to_target, velocity_in_parallel) > M_PI/2)
        mult = -1;

    double suvat_stop_distance = -(linear_velocity_in_parallel * linear_velocity_in_parallel) / (-2 * max_acceleration);

    float distance_to_target = (target - my_position).length();

    //std::cout << "SUVAT " << suvat_stop_distance << " DIST " << distance_to_target << std::endl;

    if(suvat_stop_distance >= distance_to_target - (10 + lax_distance) && suvat_stop_distance > 0 && mult > 0)
    {
        control_force += -to_target * remaining_control_force;
        return control_force;
    }
    else
    {
        control_force += to_target * remaining_control_force;
        return control_force;
    }
}

void ship_cpu_pathfinding(double dt_s, ship& s, playspace_manager& play, playspace* space, room* r)
{
    std::optional<entity*> target = std::nullopt;

    //vec2f dest = s.realspace_destination;

    ///uuh ok this is just wrong, not checking sensor data?
    /*if(s.realspace_pid_target)
    {
        target = r->entity_manage->fetch(s.realspace_pid_target);
    }*/

    vec2f known_target_dest = {s.move_args.x, s.move_args.y};
    float known_target_angle = 0;

    if(s.move_args.id != (size_t)-1)
    {
        std::optional<entity*> e = r->entity_manage->fetch(s.move_args.id);

        ///check that its in the same room as me
        if(e.has_value())
        {
            bool in_sensor_range = false;

            for(auto& i : s.last_sample.renderables)
            {
                if(i.uid == s.move_args.id)
                {
                    in_sensor_range = true;
                    break;
                }
            }

            if(in_sensor_range)
            {
                target = e;
            }
        }
    }

    if(target.has_value())
    {
        known_target_dest = target.value()->r.position;
        known_target_angle = target.value()->r.rotation;

        ///move to absolute target if we have one, update cache
        ///you know what
        ///maybe this shouldn't chase?
        if(s.move_args.type == instructions::TMOV && s.move_args.id != (size_t)-1)
        {
            /*s.move_args.x = known_target_dest.x();
            s.move_args.y = known_target_dest.y();*/
            //s.move_args.angle = known_target_angle;
        }

        ///nope
        if(s.move_args.type == instructions::AMOV)
        {

        }

        ///nothing on rmov, always moves to fixed coordinate
        ///also moves relative to myself but that's a one time kinda deal
        if(s.move_args.type == instructions::RMOV)
        {

        }

        ///???
        /*if(s.move_args.type == instructions::KEEP)
        {
            assert(s.move_args.id != (size_t)-1);

            float distance_away = s.move_args.radius;

            vec2f absolute_keep_position = (s.r.position - known_target_dest).norm() * distance_away;

            s.move_args.x = absolute_keep_position.x();
            s.move_args.y = absolute_keep_position.y();
        }*/

        ///turn to face absolute coordinate, does nothing
        if(s.move_args.type == instructions::ATRN)
        {

        }

        ///do nothing
        if(s.move_args.type == instructions::RTRN)
        {

        }
    }

    float target_angle = s.move_args.angle;

    bool is_turn = s.move_args.type == instructions::ATRN || s.move_args.type == instructions::RTRN || s.move_args.type == instructions::TTRN;

    if(is_turn)
    {
        s.set_thrusters_active(1);
        float max_turn_rate = s.get_max_angular_thrust();

        float signed_angle = signed_angle_between_vectors((vec2f){1, 0}.rot(s.r.rotation), (vec2f){1, 0}.rot(target_angle));

        /*float per_tick = max_turn_rate * dt_s;

        if(fabs(per_tick) > fabs(signed_angle))
        {
            per_tick = signum(signed_angle) * signed_angle;
        }

        float rotation_force = signum(per_tick) * dt_s;

        s.apply_rotation_force(rotation_force);*/

        float angular = get_angular_control_force(target_angle, s.angular_velocity, s.r.rotation, max_turn_rate) * dt_s;

        s.angular_velocity += angular;

        ///need to slow down again as well
        if(fabs(signed_angle) < M_PI/32 && fabs(s.angular_velocity) < M_PI/64)
        {
            unblock_cpu_hardware(s, hardware::T_DRIVE);
            s.travelling_in_realspace = false;
            return;
        }
    }

    bool is_move = s.move_args.type == instructions::TMOV || s.move_args.type == instructions::AMOV || s.move_args.type == instructions::RMOV || s.move_args.type == instructions::KEEP;

    if(is_move)
    {
        const vec2f move_to = {s.move_args.x, s.move_args.y};

        /*if(target)
        {
            dest = target.value()->r.position;
            s.realspace_destination = dest;
        }*/

        vec2f start_pos = s.r.position;

        vec2f to_dest = move_to - start_pos;


        float extra_lax_distance = 0;

        {
            entity dtest(temporary_owned{});
            dtest.r.init_rectangular({50, 50});
            dtest.r.position = move_to;
            dtest.r.rotation = 0;

            ///check for bad target
            for(auto& i : s.last_sample.renderables)
            {
                entity tent(temporary_owned{});
                tent.r = i.property.r;

                if(collides(dtest, tent))
                {
                    extra_lax_distance += i.property.r.approx_dim.length() * 2 * 1.5;
                }
            }
        }

        if(s.move_args.type == instructions::KEEP)
            extra_lax_distance = 0;

        ///collision avoidance here
        if(to_dest.length() < s.move_args.lax_distance + extra_lax_distance)
        {
            unblock_cpu_hardware(s, hardware::T_DRIVE);
            s.travelling_in_realspace = false;

            return;
        }

        /*vec2f additional_force = {0,0};

        if(to_dest.length() < 150)
        {
            vec2f my_vel = s.velocity;

            if(my_vel.length() > 20)
            {
                additional_force = -my_vel.norm();
            }
        }

        if(to_dest.length() < 70)
        {
            unblock_cpu_hardware(s, hardware::T_DRIVE);
            s.travelling_in_realspace = false;
            return;
        }*/

        float search_distance = 50;
        vec2f centre = s.r.position + to_dest.norm() * 10 + (to_dest.norm() * search_distance) / 2.f;

        vec2f avoid_force = {0,0};

        ///colliding with all entities!!! not just stuff in sensor range!
        if(std::optional<entity*> coll = r->entity_manage->collides_with_any(centre, (vec2f){search_distance/4, 10}, to_dest.angle()); coll.has_value() && (coll.value()->_pid != s.move_args.id || s.move_args.id == (size_t)-1))
        {
            vec2f my_travel_direction = s.velocity;
            vec2f my_position = s.r.position;

            vec2f their_travel_direction = coll.value()->velocity;
            vec2f their_position = coll.value()->r.position;

            ///counterclockwise
            vec2f perpendicular_direction = perpendicular((their_position - my_position).norm());

            float burn_dir = 1;

            int my_velocity_in_perp = angle_between_vectors(perpendicular_direction, my_travel_direction) < M_PI/2;
            int their_velocity_in_perp = angle_between_vectors(perpendicular_direction, their_travel_direction) < M_PI/2;

            vec2f my_perp_component = projection(my_travel_direction, perpendicular_direction.norm());
            vec2f their_perp_component = projection(their_travel_direction, perpendicular_direction.norm());

            ///so, if their velocity has a component which is the opposite direction to our velocity
            ///should accelerate in our velocity perpendicular direction
            if(my_velocity_in_perp != their_velocity_in_perp)
            {
                burn_dir = my_velocity_in_perp > 0 ? 1 : -1;
            }
            else
            {
                ///both in same direction

                if(their_perp_component.length() > my_perp_component.length())
                {
                    ///their velocity in the perpendicular > mine, should reverse velocity away
                    burn_dir = my_velocity_in_perp > 0 ? -1 : 1;
                }
                else
                {
                    ///my velocity in perp is > theirs, should more in the same direction
                    burn_dir = my_velocity_in_perp > 0 ? 1 : -1;
                }
            }

            avoid_force = perpendicular_direction.norm() * burn_dir;

            //vec2f relative_velocity = s.velocity - velocity;
        }

        float available_thrust = s.get_max_velocity_thrust();

        //vec2f final_force = (to_dest.norm() + additional_force.norm()).norm();

        vec2f target_force = get_control_force(move_to, s.velocity, s.r.position, available_thrust, s.move_args.lax_distance);

        vec2f final_force = (target_force.norm() * dt_s + avoid_force.norm() * available_thrust * dt_s).norm();

        //s.apply_force(final_force.rot(-s.r.rotation) * dt_s);

        s.velocity += final_force * available_thrust * dt_s;

        s.set_thrusters_active(1);

        vec2f crot = (vec2f){1, 0}.rot(s.r.rotation);
        s.r.rotation = (crot * 100 + final_force).angle();
    }
}

void ship::check_space_rules(double dt_s, playspace_manager& play, playspace* space, room* r)
{
    playspace_resetter dummy(*this);

    update_cpu_rules_and_hardware(*this, play, space, r);

    if(travelling_in_realspace)
    {
        ship_cpu_pathfinding(dt_s, *this, play, space, r);
    }

    last_room_type = room_type;

    if(r == nullptr)
    {
        travelling_in_realspace = false;
        current_room_pid = -1;
    }
    else
    {
        current_room_pid = r->_pid;
    }

    if(r == nullptr && !travelling_to_poi)
    {
        travelling_in_realspace = false;
        ship_drop_to(*this, play, space, nullptr, false);
        return;
    }

    if(!has_s_power && r == nullptr)
    {
        travelling_in_realspace = false;
        ///NEW ROOM
        ship_drop_to(*this, play, space, nullptr, true);
        return;
    }

    if(travelling_to_poi)
    {
        travelling_in_realspace = false;
        handle_fsd_movement(dt_s, play, *this);
        return;
    }

    if(move_warp && space && has_w_power && r != nullptr)
    {
        unblock_cpu_hardware(*this, hardware::W_DRIVE);
        unblock_cpu_hardware(*this, hardware::T_DRIVE);

        std::optional<playspace*> dest = play.get_playspace_from_id(warp_to_pid);

        if(!dest.has_value())
            return;

        if(!playspaces_connected(space, dest.value()))
            return;

        ///leaving this here for future correctness in case i allow warping from fsd space
        if(r != nullptr)
        {
            r->rem(this);
        }

        deplete_w(*this);
        space->rem(this);
        dest.value()->add(this);

        room* nr = dest.value()->make_room(this->r.position, 5, poi_type::DEAD_SPACE);
        play.enter_room(this, nr);
        travelling_in_realspace = false;
        velocity = {0,0};

        return;
    }

    ///could not warp
    if(move_warp)
    {
        unblock_cpu_hardware(*this, hardware::W_DRIVE);
        return;
    }

    ///already in a room
    /*if(move_down && r != nullptr)
        return;

    ///already in fsd space
    if(move_up && r == nullptr)
        return;

    if(move_up && has_s_power && r != nullptr)
    {
        play.exit_room(this);
        //play->enter_room(this, r);
    }

    if(move_down && r == nullptr)
    {
        std::optional<room*> found = play.get_nearby_room(this);

        if(found)
        {
            play.enter_room(this, found.value());
        }
    }*/
}

void ship::step_cpus(playspace_manager& play, playspace* space, room* r)
{
    for(component& c : components)
    {
        if(!c.has_tag(tag_info::TAG_CPU))
            continue;

        if(c.cpu_core.free_running || c.cpu_core.should_step)
            c.cpu_core.step(this, &play, space, r);
    }
}

double apply_to_does(double amount, does_dynamic& d, const does_fixed& fix)
{
    double prev = d.held;

    double next = prev + amount;

    if(next < 0)
        next = 0;

    if(next > fix.capacity)
        next = fix.capacity;

    d.held = next;

    return next - prev;
}

void ship::take_damage(double amount, bool internal_damage, bool base)
{
    int max_breakup = 5;
    int cur_breakup = 0;

    while(amount >= 10 && cur_breakup < max_breakup && base)
    {
        take_damage(amount/2, internal_damage, false);

        amount -= amount/2;

        cur_breakup++;
    }

    double remaining = -amount;

    if(!internal_damage)
    {
        for(component& c : components)
        {
            if(c.has(component_info::SHIELDS))
            {
                does_dynamic& d = c.get_dynamic(component_info::SHIELDS);
                const does_fixed& fix = c.get_fixed(component_info::SHIELDS);

                double diff = apply_to_does(remaining, d, fix);

                remaining -= diff;
            }
        }

        for(component& c : components)
        {
            if(c.has(component_info::ARMOUR))
            {
                does_dynamic& d = c.get_dynamic(component_info::ARMOUR);
                const does_fixed& fix = c.get_fixed(component_info::ARMOUR);

                double diff = apply_to_does(remaining, d, fix);

                remaining -= diff;
            }
        }
    }

    std::minstd_rand0 rng;
    rng.seed(get_random_value());

    std::uniform_int_distribution<int> dist(0, (int)components.size() - 1);

    int max_vals = 100;

    for(int i=0; i < max_vals && fabs(remaining) > 0.00001; i++)
    {
        int next = dist(rng);

        if(next >= (int)components.size())
        {
            printf("What? Next!\n");
            continue;
        }

        component& c = components[next];

        if(c.has(component_info::HP))
        {
            does_dynamic& d = c.get_dynamic(component_info::HP);
            const does_fixed& fix = c.get_fixed(component_info::HP);

            double diff = apply_to_does(remaining, d, fix);

            remaining -= diff;
        }
    }
}

void ship::add_pipe(const storage_pipe& p)
{
    pipes.push_back(p);
}

double ship::get_sensor_strength()
{
    std::vector<double> net = sum<double>
    ([](component& c)
    {
        return c.get_needed();
    });

    return clamp(net[component_info::SENSORS], 0, 1);
}

void ship::pre_collide(entity& other)
{
    bool is_missile = false;

    for(component& c : components)
    {
        if(c.has_tag(tag_info::TAG_MISSILE_BEHAVIOUR))
            is_missile = true;
    }

    if(is_missile)
    {
        for(component& c : components)
        {
            if(c.has(component_info::SELF_DESTRUCT) && c.get_hp_frac() > 0.2)
            {
                c.force_use = true;
            }
        }
    }
}

void ship::on_collide(entity_manager& em, entity& other)
{
    if(dynamic_cast<asteroid*>(&other) != nullptr || dynamic_cast<ship*>(&other))
    {
        const vec2f relative_velocity = velocity - other.velocity;

        vec2f normal_vector = r.position - other.r.position;

        if(angle_between_vectors(relative_velocity, normal_vector) <= M_PI/2)
            return;

        vec2f to_them = -normal_vector.norm();

        vec2f to_them_component = projection(relative_velocity, to_them);

        float my_mass = mass;
        float their_mass = other.mass;

        float my_velocity_share = 1 - (my_mass / (my_mass + their_mass));
        float their_velocity_share = 1 - (their_mass / (my_mass + their_mass));


        vec2f velocity_change = (relative_velocity - 1.5 * to_them_component);

        vec2f my_change = velocity_change * my_velocity_share;
        //vec2f their_change = -velocity_change * their_velocity_share;
        vec2f their_change = 0.5 * to_them_component * their_velocity_share;

        vec2f next_velocity = (velocity - relative_velocity) + my_change + (r.position - other.r.position).norm() * 0.1;;

        velocity = next_velocity;
        other.velocity += their_change;

        take_damage(my_change.length()/2);

        ship* oship = dynamic_cast<ship*>(&other);

        if(oship)
        {
            oship->take_damage(their_change.length()/2);
        }
    }
}

bool transfer_is_valid(ship& test_ship, ship& destination, pending_transfer& in)
{
    auto destination_component = destination.fetch_component_by_id(in.pid_component);

    if(!destination_component.has_value())
        return false;

    auto ship_to_steal = test_ship.fetch_ship_by_id(in.pid_ship_from);

    if(!ship_to_steal.has_value())
        return false;

    if(in.is_fractiony)
    {
        component& store_in = destination_component.value();
        ship* to_steal = ship_to_steal.value();

        if(to_steal->is_ship)
            return false;

        float free_space = store_in.get_internal_volume() - store_in.get_stored_volume();

        float requested_volume = to_steal->get_my_volume() * in.fraction;

        if(free_space < requested_volume)
        {
            if(requested_volume < 0.0001)
                return false;

            float takeable_frac = free_space / to_steal->get_my_volume();

            in.fraction = clamp(takeable_frac, 0, 1);
        }

        if(in.fraction < 0.00001)
            return false;

        ship scopy = *to_steal;

        std::optional<ship> psplit = scopy.split_materially(in.fraction);

        if(!psplit)
            return false;

        if(!destination_component.value().can_store(psplit.value()))
            return false;
    }
    else
    {
        if(!destination_component.value().can_store(*ship_to_steal.value()))
            return false;
    }

    return true;
}

void ship::consume_cpu_transfers(room* r)
{
    for(component& c : components)
    {
        if(c.base_id == component_type::CPU)
        {
            for(cpu_xfer& cxfer : c.cpu_core.xfers)
            {
                pending_transfer equiv;
                equiv.pid_component = cxfer.pid_component;
                equiv.pid_ship_from = cxfer.pid_ship_from;
                equiv.pid_ship_to = cxfer.pid_ship_to;
                equiv.fraction = cxfer.fraction;
                equiv.is_fractiony = cxfer.is_fractiony;

                bool success = consume_transfer(r, equiv, this->_pid);

                c.cpu_core.context.register_states[(int)registers::TEST].set_int(success);

                assert(cxfer.held_file >= 0 && cxfer.held_file < (int)c.cpu_core.files.size());

                c.cpu_core.files[cxfer.held_file].was_xferred = true;
            }

            c.cpu_core.tx_pending = false;
            c.cpu_core.xfers.clear();
        }
    }
}


bool has_ship_in_storage(size_t pid, std::vector<component>& components)
{
    for(component& c : components)
    {
        for(ship& s : c.stored)
        {
            if(s._pid == pid)
                return true;

            ///not our ship, recurse
            auto it = s.fetch_ship_by_id(pid);

            if(it)
                return true;
        }
    }

    return false;
}

///from is the thing to steal
std::optional<size_t> stored_in_nearby_ship(size_t from_pid, ship* to, room* r, size_t pid_ship_initiating)
{
    if(from_pid == to->_pid)
        return {from_pid};

    if(has_ship_in_storage(from_pid, to->components))
        return {to->_pid};

    std::vector<std::pair<ship, std::vector<component>>> found = r->get_nearby_accessible_ships(*to, pid_ship_initiating);

    for(auto& i : found)
    {
        if(has_ship_in_storage(from_pid, i.second))
            return {i.first._pid};
    }

    return std::nullopt;
}

bool consume_transfer(room* r, pending_transfer& xfer, size_t pid_ship_initiating)
{
    auto ship_to_opt = r->entity_manage->fetch(xfer.pid_ship_to);

    if(!ship_to_opt)
        return false;

    ship* to_as_ship = dynamic_cast<ship*>(ship_to_opt.value());

    if(!to_as_ship)
        return false;

    auto parent_ship_id = stored_in_nearby_ship(xfer.pid_ship_from, to_as_ship, r, pid_ship_initiating);

    if(!parent_ship_id.has_value())
        return false;

    auto parent_ship_opt = r->entity_manage->fetch(parent_ship_id.value());

    if(!parent_ship_opt.has_value())
        return false;

    ship* parent_ship_as_ship = dynamic_cast<ship*>(parent_ship_opt.value());

    assert(parent_ship_as_ship);

    if(!transfer_is_valid(*parent_ship_as_ship, *to_as_ship, xfer))
        return false;

    std::optional<ship> removed_ship;

    if(!xfer.is_fractiony)
    {
        removed_ship = parent_ship_as_ship->remove_ship_by_id(xfer.pid_ship_from);
    }
    else
    {
        auto fetched = parent_ship_as_ship->fetch_ship_by_id(xfer.pid_ship_from);

        if(fetched)
        {
            removed_ship = fetched.value()->split_materially(xfer.fraction);
        }
        else
        {
            removed_ship = std::nullopt;
        }
    }

    if(!removed_ship.has_value())
        return false;

    std::vector<cpu_file> files;

    for(component& tc : removed_ship.value().components)
    {
        if(tc.base_id != component_type::CPU)
            continue;

        std::string directory = removed_ship.value().current_directory;

        auto found = tc.cpu_core.extract_files_in_directory(directory);

        files.insert(files.end(), found.begin(), found.end());

        tc.cpu_core.had_tx_pending = true;
    }

    /*for(component& tc : removed_ship.value().components)
    {
        if(tc.base_id != component_type::CPU)
            continue;

        tc.cpu_core.files.insert(tc.cpu_core.files.end(), files.begin(), files.end());
    }*/

    ///kind of wrong
    ///ok so what we need to do is find THEIR *REAL* ship, which is to_as_ship
    ///find the component from it
    ///find the directory of that component
    ///then rename our file to directory + / + componentname

    to_as_ship->add_ship_to_component(removed_ship.value(), xfer.pid_component);

    for(component& tc : to_as_ship->components)
    {
        if(tc.base_id != component_type::CPU)
            continue;

        tc.cpu_core.files.insert(tc.cpu_core.files.end(), files.begin(), files.end());
        tc.cpu_core.had_tx_pending = true;

        break;
    }


    /* std::vector<cpu_file> files;

    for(component& tc : components)
    {
        if(tc.base_id != component_type::CPU)
            continue;

        std::string directory = c.stored[0].current_directory;

        auto found = tc.cpu_core.extract_files_in_directory(directory);

        files.insert(files.end(), found.begin(), found.end());
    }

    std::optional<ship> first = c.remove_first_stored_item();

    if(first.has_value())
    {
        vec2f ship_vector = (vec2f){1, 0}.rot(r.rotation);

        ship& to_produce = first.value();

        ship* spawned = parent->take(to_produce);

        spawned->r.position = r.position;
        spawned->r.rotation = r.rotation;
        //l->r.rotation = r.rotation + eangle;
        //l->velocity = (vec2f){1, 0}.rot(r.rotation) * 50;
        spawned->velocity = velocity + ship_vector.norm() * 10;
        //l->velocity = velocity + (vec2f){1, 0}.rot(r.rotation + eangle) * 50;
        spawned->phys_ignore.push_back(_pid);
        //spawned->fired_by = id;

        spawned->r.init_rectangular({1, 0.2});
        spawned->network_owner = network_owner;
        spawned->spawn_clock.restart();
        spawned->spawned_by = _pid;
        spawned->phys_drag = false;
        spawned->current_radar_field = current_radar_field;

    }*/

    return true;
}

std::optional<ship*> ship::fetch_ship_by_id(size_t pid)
{
    if(_pid == pid)
        return this;

    for(component& c : components)
    {
        for(ship& s : c.stored)
        {
            if(s._pid == pid)
                return &s;

            ///not our ship, recurse
            auto it = s.fetch_ship_by_id(pid);

            if(it)
                return it;
        }
    }

    return std::nullopt;
}

std::optional<component> ship::fetch_component_by_id(size_t pid)
{
    for(component& c : components)
    {
        if(c._pid == pid)
            return c;

        for(ship& s : c.stored)
        {
            auto it = s.fetch_component_by_id(pid);

            if(it)
                return it;
        }
    }

    return std::nullopt;
}

std::optional<ship> ship::remove_ship_by_id(size_t pid)
{
    for(component& c : components)
    {
        for(int i=0; i < (int)c.stored.size(); i++)
        {
            ship& s = c.stored[i];

            ///not our ship, recurse
            if(s._pid != pid)
            {
                auto it = s.remove_ship_by_id(pid);

                if(it)
                    return it;
            }
            ///found our ship, keep going
            else
            {
                ship cpy = s;

                c.stored.erase(c.stored.begin() + i);
                return cpy;
            }
        }
    }

    return std::nullopt;
}

void ship::add_ship_to_component(ship& s, size_t pid)
{
    for(component& c : components)
    {
        if(c._pid != pid)
        {
            for(ship& ns : c.stored)
            {
                ns.add_ship_to_component(s, pid);
            }
        }
        else
        {
            c.store(s);
            return;
        }
    }
}

std::optional<ship> ship::split_materially(float split_take_frac)
{
    split_take_frac = clamp(split_take_frac, 0, 1);

    if(is_ship)
        return std::nullopt;

    ship nnext;

    for(component& c : components)
    {
        component next = get_component_default(component_type::MATERIAL, 1);
        next.my_temperature = c.get_my_temperature();
        next.phase = c.phase;
        next.long_name = c.long_name;
        next.short_name = c.short_name;

        for(material& m : c.composition)
        {
            float my_frac = (1 - split_take_frac);
            float their_frac = split_take_frac;

            float my_mat_vol = m.dynamic_desc.volume * my_frac;
            float their_mat_vol = m.dynamic_desc.volume * their_frac;

            material them = m;
            them.dynamic_desc.volume = their_mat_vol;
            m.dynamic_desc.volume = my_mat_vol;

            next.composition.push_back(them);
        }

        nnext.add(next);
    }

    return nnext;
}

void ship::new_network_copy()
{
    data_track.resize(component_info::COUNT);
    _pid = get_next_persistent_id();

    for(auto& i : data_track)
    {
        i._pid = get_next_persistent_id();
    }

    for(storage_pipe& p : pipes)
    {
        p._pid = get_next_persistent_id();
    }

    for(component& c : components)
    {
        auto next_id = get_next_persistent_id();

        for(storage_pipe& p : pipes)
        {
            if(p.id_1 == c._pid)
            {
                p.id_1 = next_id;
            }

            if(p.id_2 == c._pid)
            {
                p.id_2 = next_id;
            }
        }

        c._pid = next_id;

        original_blueprint = std::make_shared<blueprint>(*original_blueprint);

        for(ship& s : c.stored)
        {
            s.new_network_copy();
        }
    }
}

#if 0
void ship::render(sf::RenderWindow& window)
{
    /*sf::RectangleShape shape;
    shape.setSize(sf::Vector2f(4, 10));
    shape.setOrigin(shape.getSize().x/2, shape.getSize().y/2);

    shape.setFillColor(sf::Color(0,0,0,255));
    shape.setOutlineColor(sf::Color(255,255,255,255));
    shape.setOutlineThickness(2);

    shape.setPosition(position.x(), position.y());
    shape.setRotation(r2d(rotation));

    win.draw(shape);*/

    e.render(window);
}
#endif // 0

void ship::fire(const std::vector<client_fire>& fired)
{
    /*for(component& c : components)
    {
        if(c.has(component_info::WEAPONS))
        {
            c.try_use = true;
        }
    }*/


    for(const client_fire& fire : fired)
    {
        int weapon_offset = 0;

        for(component& c : components)
        {
            if(c.has_tag(tag_info::TAG_WEAPON))
            {
                if(weapon_offset == (int)fire.weapon_offset)
                {
                    c.try_use = true;
                    c.use_angle = fire.fire_angle;
                }

                weapon_offset++;
            }
        }
    }
}

void ship::ping()
{
    for(component& c : components)
    {
        if(c.has(component_info::SENSORS))
        {
            c.try_use = true;
        }
    }
}

projectile::projectile()
{
    r.init_rectangular({1, 0.2});
}

void projectile::on_collide(entity_manager& em, entity& other)
{
    if(dynamic_cast<ship*>(&other))
    {
        dynamic_cast<ship*>(&other)->take_damage(11);
    }

    cleanup = true;
}

void projectile::tick(double dt_s)
{

}

asteroid::asteroid(std::shared_ptr<alt_radar_field> field) : current_radar_field(field)
{
    mass = 1000;
}

void asteroid::init(float min_rad, float max_rad)
{
    int n = 5;

    for(int i=0; i < n; i++)
    {
        r.vert_dist.push_back(randf_s(min_rad, max_rad));

        float angle = ((float)i / n) * 2 * M_PI;

        r.vert_angle.push_back(angle);

        r.vert_cols.push_back({1,1,1,1});
    }

    r.approx_rad = max_rad;
    r.approx_dim = {max_rad, max_rad};

    mass = 500 * pow(max_rad, 3);
}

void asteroid::tick(double dt_s)
{
    float emitted = latent_heat * HEAT_EMISSION_FRAC;

    alt_frequency_packet heat;
    heat.make(permanent_heat + emitted, HEAT_FREQ);

    current_radar_field->emit(heat, r.position, *this);

    latent_heat -= emitted;
}

struct transient_entity : entity
{
    bool immediate_destroy = false;
    uint32_t id_e = 0;
    uint32_t id_r = 0;
    sf::Clock clk;
    int echo_type = -1;

    uint32_t uid = -1;

    std::vector<std::pair<vec2f, sf::Clock>> last_props;

    vec2f dir = {0,0};

    bool fadeout = false;
    double fadeout_after_s = 0.2;
    double fadeout_time_s = 0.3;

    transient_entity()
    {
        r.init_rectangular({5, 5});
    }

    virtual void tick(double dt_s) override
    {
        double time_ms = clk.getElapsedTime().asMicroseconds() / 1000.;

        if(time_ms >= 1000)
        {
            cleanup = true;
        }

        if(immediate_destroy)
            cleanup = true;

        if(fadeout)
        {
            double current_time_s = (clk.getElapsedTime().asMicroseconds() / 1000.) / 1000.;

            if(current_time_s >= fadeout_after_s && current_time_s <= fadeout_after_s + fadeout_time_s)
            {
                double frac = (current_time_s - fadeout_after_s) / fadeout_time_s;

                frac = clamp(1 - frac, 0, 1);

                for(auto& i : r.vert_cols)
                {
                    i = {frac, frac, frac, 1};
                }
            }
        }
    }

    void add_last_prop(vec2f in)
    {
        last_props.push_back({in, sf::Clock()});
    }

    vec2f get_last_prop()
    {
        vec2f sum = {0,0};

        float max_intens = 0;

        for(auto it = last_props.begin(); it != last_props.end();)
        {
            if(it->second.getElapsedTime().asMicroseconds() / 1000. > 1000)
            {
                it = last_props.erase(it);
            }
            else
            {
                sum += it->first;

                max_intens = std::max(max_intens, it->first.length());

                it++;
            }
        }

        return sum.norm() * max_intens;

    }
};

void tick_radar_data(entity_manager& transients, alt_radar_sample& sample, entity* ship_proxy)
{
    if(!sample.fresh)
        return;

    sample.fresh = false;

    /*for(entity* e : transients.entities)
    {
        e->cleanup = true;
    }

    transients.cleanup();*/

    /*std::cout << "received " << sample.receive_dir.size() << std::endl;

    for(auto& e : sample.receive_dir)
    {
        std::cout << e.property << std::endl;
    }*/

    for(auto& e : sample.renderables)
    {
        uint32_t uid = e.uid;

        auto entities = transients.fetch<transient_entity>();

        transient_entity* next = nullptr;

        for(transient_entity* transient : entities)
        {
            if(uid == transient->uid && transient->echo_type == 3)
            {
                next = transient;
                break;
            }
        }

        if(next == nullptr)
            next = transients.make_new<transient_entity>();

        next->uid = uid;
        next->echo_type = 3;
        next->clk.restart();
        next->r = e.property.r;

        for(auto& i : next->r.vert_cols)
        {
            if(e.property.is_unknown && e.property.velocity.length() > 0)
            {
                i = {1,0,0,1};
            }
        }

        if(!e.property.is_unknown || e.property.velocity.length() == 0)
        {
            next->r.vert_cols = e.property.r.vert_cols;
        }
    }

    /*for(alt_object_property<uncertain_renderable>& e : sample.low_detail)
    {
        uint32_t uid = e.uid;

        auto entities = transients.fetch<transient_entity>();

        transient_entity* next = nullptr;

        bool has_high_detail = false;

        for(transient_entity* transient : entities)
        {
            if(uid == transient->uid && transient->echo_type == 4)
            {
                next = transient;
            }

            if(uid == transient->uid && transient->echo_type == 3)
            {
                has_high_detail = true;
            }
        }

        if(!has_high_detail)
        {
            if(next == nullptr)
                next = transients.make_new<transient_entity>();

            next->uid = uid;
            next->echo_type = 4;
            next->clk.restart();
            //next->r = e.property;

            next->r.init_rectangular({e.property.radius, e.property.radius});
            next->r.position = e.property.position;

            for(auto& i : next->r.vert_cols)
            {
                if(e.property.is_unknown && e.property.velocity.length() > 0)
                {
                    i = {1,0,0,1};
                }
                else
                {
                    i = {1,1,1,1};
                }
            }
        }

        if(has_high_detail && next)
        {
            next->cleanup = true;
        }
    }*/

    #if 0
    float radar_width = 1;

    for(alt_object_property<vec2f>& e : sample.echo_pos)
    {
        uint32_t id_e = e.id_e;
        uint32_t id_r = e.id_r;
        bool found = false;

        auto entities = transients.fetch<transient_entity>();

        transient_entity* next = nullptr;

        for(transient_entity* transient : entities)
        {
            if(id_e == transient->id_e && id_r == transient->id_r && transient->echo_type == 0)
            {
                next = transient;
                break;
            }
        }

        //if(!found)

        if(next == nullptr)
            next = transients.make_new<transient_entity>();

        next->id_e = id_e;
        next->id_r = id_r;
        next->r.position = e.property;
        next->echo_type = 0;
        next->clk.restart();

        next->r.init_rectangular({e.cross_section, 1});
        next->r.rotation = (next->r.position - ship_proxy->r.position).angle() + M_PI/2;
    }

    std::vector<alt_object_property<vec2f>> processed_echo_dir;

    for(auto& e : sample.echo_dir)
    {
        bool found = false;

        for(auto& o : processed_echo_dir)
        {
            if(e.id_e == o.id_e && e.id_r == o.id_r && e.frequency == o.frequency)
            {
                found = true;
                ///this is a curious property of the fact that they're unnormalised vectors
                e.property += o.property;
                break;
            }
        }

        if(!found)
        {
            processed_echo_dir.push_back(e);
        }
    }

    for(alt_object_property<vec2f>& e : processed_echo_dir)
    {
        uint32_t id_e = e.id_e;
        uint32_t id_r = e.id_r;
        bool found = false;

        transient_entity* next = nullptr;

        auto entities = transients.fetch<transient_entity>();

        for(transient_entity* transient : entities)
        {
            if(id_e == transient->id_e && id_r == transient->id_r && transient->echo_type == 1)
            {
                next = transient;
                break;
            }
        }

        bool has = next != nullptr;

        if(next == nullptr)
            next = transients.make_new<transient_entity>();

        next->add_last_prop(e.property);

        //float intensity = std::max(e.property.length(), next->last_prop.length());

        vec2f next_prop = next->get_last_prop();

        float intensity = next_prop.length();

        intensity = clamp(intensity*10, 1, 20);

        vec2f next_pos = sample.location + next_prop.norm() * (intensity * 2 + 10);
        //vec2f next_pos = sample.location + (e.property + next->last_prop).norm() * (intensity * 2 + 10);

        if(!has)
        {
            next->r.position = next_pos;
        }

        next->r.init_rectangular({intensity, radar_width});
        next->r.rotation = (next_prop).angle();
        next->clk.restart();
        next->id_e = id_e;
        next->id_r = id_r;
        next->echo_type = 1;
        //next->last_prop = e.property + next->last_prop;

        next->set_parent_entity(ship_proxy, next_pos);
    }

    for(alt_object_property<vec2f>& e : sample.receive_dir)
    {
        uint32_t id_e = e.id_e;
        uint32_t id_r = e.id_r;
        bool found = false;

        transient_entity* next = nullptr;

        auto entities = transients.fetch<transient_entity>();

        for(transient_entity* transient : entities)
        {
            if(id_e == transient->id_e && id_r == transient->id_r && transient->echo_type == 2)
            {
                next = transient;
                break;
            }
        }

        bool has = next != nullptr;

        if(next == nullptr)
        {
            next = transients.make_new<transient_entity>();
        }

        /*float intensity = sample.receive_dir[i].property.length();
        intensity = clamp(intensity*10, 1, 20);
        vec2f next_pos = sample.location + sample.receive_dir[i].property.norm() * (intensity + 10);*/

        next->add_last_prop(e.property);
        vec2f next_prop = next->get_last_prop();
        float intensity = next_prop.length();

        intensity = clamp(intensity*10, 1, 20);

        vec2f next_pos = sample.location + next_prop.norm() * (intensity * 2 + 10);

        if(!has)
        {
            next->r.position = next_pos;
        }

        next->r.init_rectangular({intensity, radar_width});
        next->r.rotation = next_prop.angle();

        next->clk.restart();
        next->id_e = id_e;
        next->id_r = id_r;
        next->echo_type = 2;

        next->set_parent_entity(ship_proxy, next_pos);

        for(auto& i : next->r.vert_cols)
        {
            i = {1, 0, 0, 1};
        }
    }
    #endif // 0

    /*client_model.sample.echo_pos.clear();
    client_model.sample.echo_dir.clear();
    client_model.sample.receive_dir.clear();*/
}

void render_radar_data(sf::RenderWindow& window, const alt_radar_sample& sample)
{
    ImGui::Begin("Radar Data");

    assert(sample.frequencies.size() == sample.intensities.size());

    int frequency_bands = 100;

    std::vector<float> frequencies;
    frequencies.resize(frequency_bands);

    double min_freq = MIN_FREQ;
    double max_freq = MAX_FREQ;

    for(int i=0; i < frequency_bands - 1; i++)
    {
        double lower_band = ((double)i / frequency_bands) * (max_freq - min_freq) + min_freq;
        double upper_band = ((double)(i + 1) / frequency_bands) * (max_freq - min_freq) + min_freq;

        for(int kk=0; kk < (int)sample.frequencies.size(); kk++)
        {
            float freq = sample.frequencies[kk];
            float intensity = sample.intensities[kk];

            if(freq >= lower_band && freq < upper_band)
            {
                frequencies[i] += intensity;
            }
        }
    }

    ImGui::PlotHistogram("RADAR DATA", &frequencies[0], frequencies.size(), 0, nullptr, 0, 100, ImVec2(0, 100));

    ImGui::End();
}
