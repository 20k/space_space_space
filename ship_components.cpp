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

double apply_to_does(double amount, does& d);

ship::ship()
{
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

    drag = true;

    data_track.resize(component_info::COUNT);
}

void component::add(component_info::does_type type, double amount)
{
    does d;
    d.type = type;
    d.recharge = amount;

    info.push_back(d);
}

void component::add(component_info::does_type type, double amount, double capacity)
{
    does d;
    d.type = type;
    d.recharge = amount;
    d.capacity = capacity;
    d.held = d.capacity;

    info.push_back(d);
}

void component::add_on_use(component_info::does_type type, double amount, double time_between_use_s)
{
    does d;
    d.type = type;
    d.capacity = amount;
    d.held = 0;
    d.time_between_use_s = time_between_use_s;

    activate_requirements.push_back(d);
}

void component::set_heat(double heat)
{
    heat_produced_at_full_usage = heat;
}

void component::set_heat_scales_by_production(bool status, component_info::does_type primary)
{
    production_heat_scales = status;
    primary_type = primary;
}

void component::set_no_drain_on_full_production()
{
    no_drain_on_full_production = true;
}

void component::set_complex_no_drain_on_full_production()
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

    for(does& d : info)
    {
        double mod = last_sat;

        if(d.recharge < 0)
            continue;

        float hacky_lpf = last_production_frac;

        if(complex_no_drain_on_full_production)
            hacky_lpf = 1;

        needed[d.type] += d.recharge * (hp / max_hp) * mod * activation_level * hacky_lpf;
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

    for(does& d : info)
    {
        if(d.recharge > 0)
            continue;

        needed[d.type] += fabs(d.recharge) * (hp / max_hp) * activation_level * last_production_frac;
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

    for(does& d : info)
    {
        double mod = last_sat;

        if(d.recharge < 0)
            continue;

        needed[d.type] += d.recharge * (hp / max_hp) * mod * last_production_frac * activation_level;
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

    for(does& d : info)
    {
        double mod = 1;

        if(d.recharge > 0)
        {
            mod = last_sat;
        }

        needed[d.type] += d.recharge * (hp / max_hp) * mod  * last_production_frac * activation_level;
    }

    return needed;
}

std::vector<double> component::get_capacity()
{
    std::vector<double> needed;
    needed.resize(component_info::COUNT);

    for(does& d : info)
    {
        needed[d.type] += d.capacity;
    }

    return needed;
}

std::vector<double> component::get_held()
{
    std::vector<double> needed;
    needed.resize(component_info::COUNT);

    for(does& d : info)
    {
        needed[d.type] += d.held;
    }

    return needed;
}

void component::deplete_me(const std::vector<double>& diff, const std::vector<double>& free_storage, const std::vector<double>& used_storage)
{
    for(does& d : info)
    {
        double my_held = d.held;
        double my_cap = d.capacity;

        if(diff[d.type] > 0)
        {
            double my_free = my_cap - my_held;

            if(my_free <= 0)
                continue;

            double free_of_type = free_storage[d.type];

            if(fabs(free_of_type) <= 0.00001)
                continue;

            double deplete_frac = my_free / free_of_type;

            double to_change = deplete_frac * diff[d.type];

            double next = clamp(my_held + to_change, 0, my_cap);

            d.held = next;
        }
        else
        {
            double my_used = d.held;

            if(my_used <= 0)
                continue;

            double used_of_type = used_storage[d.type];

            if(fabs(used_of_type) <= 0.00001)
                continue;

            double deplete_frac = my_used / used_of_type;

            double to_change = deplete_frac * diff[d.type];

            double next = clamp(my_held + to_change, 0, my_cap);

            d.held = next;
        }
    }
}

bool component::can_use(const std::vector<double>& res)
{
    for(does& d : activate_requirements)
    {
        if(d.capacity >= 0)
            continue;

        if(fabs(d.capacity) > res[d.type])
            return false;

        if(d.last_use_s < d.time_between_use_s)
            return false;
    }

    if(has(component_info::HP))
    {
        does& d = get(component_info::HP);

        assert(d.capacity > 0);

        if((d.held / d.capacity) < 0.2)
            return false;
    }

    return true;
}

void component::use(std::vector<double>& res)
{
    for(does& d : activate_requirements)
    {
        res[d.type] += d.capacity;

        d.last_use_s = 0;
    }

    try_use = false;
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

float component::get_stored_volume() const
{
    float vol = 0;

    for(const component& c : stored)
    {
        vol += c.get_my_volume();
    }

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

    for(component& c : stored)
    {
        temp += c.get_my_temperature();
    }

    return temp;
}

float component::get_stored_heat_capacity()
{
    float temp = 0;

    for(component& c : stored)
    {
        temp += c.get_my_contained_heat();
    }

    return temp;
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

    for(component& c : stored)
    {
        float total_heat = 0;

        for(material& m : c.composition)
        {
            total_heat += material_info::fetch(m.type).specific_heat * m.dynamic_desc.volume;
        }

        if(total_heat < 0.0001)
            continue;

        c.my_temperature += (heat / total_heat) / stored_num;
    }
}

void component::remove_heat_from_stored(float heat)
{
    float stored_num = stored.size();

    for(component& c : stored)
    {
        float total_heat = 0;

        for(material& m : c.composition)
        {
            total_heat += material_info::fetch(m.type).specific_heat * m.dynamic_desc.volume;
        }

        if(total_heat < 0.0001)
            continue;

        c.my_temperature -= (heat / total_heat) / stored_num;

        if(c.my_temperature < 0)
            c.my_temperature = 0;
    }
}

bool component::can_store(const component& c)
{
    if(internal_volume <= 0)
        return false;

    float storeable = internal_volume - get_stored_volume();

    return (storeable - c.get_my_volume()) >= 0;
}

void component::store(const component& c)
{
    if(!can_store(c))
        throw std::runtime_error("Cannot store component");

    stored.push_back(c);
}

bool component::is_storage()
{
    return internal_volume > 0;
}

float component::get_hp_frac()
{
    double hp_frac = 1;

    if(has(component_info::HP) && get(component_info::HP).capacity > 0.00001)
    {
        hp_frac = get(component_info::HP).held / get(component_info::HP).capacity;
    }

    return hp_frac;
}

float component::get_operating_efficiency()
{
     return std::min(last_sat, last_production_frac) * activation_level * get_hp_frac();
}

void component::add_composition(material_info::material_type type, double volume)
{
    material new_mat;
    new_mat.type = type;
    new_mat.dynamic_desc.volume = volume;

    //my_volume += volume;

    composition.push_back(new_mat);
}

std::vector<double> ship::get_net_resources(double dt_s, const std::vector<double>& all_sat)
{
    std::vector<double> produced_resources;
    produced_resources.resize(component_info::COUNT);

    for(component& c : components)
    {
        double hp = c.get_held()[component_info::HP];
        double max_hp = c.get_capacity()[component_info::HP];

        assert(max_hp > 0);

        double min_sat = c.get_sat(all_sat);

        for(does& d : c.info)
        {
            if(d.recharge > 0)
                produced_resources[d.type] += d.recharge * (hp / max_hp) * min_sat * dt_s * c.last_production_frac * c.activation_level;
            else
                produced_resources[d.type] += d.recharge * (hp / max_hp) * dt_s * c.last_production_frac * c.activation_level;
        }

        c.last_sat = min_sat;
    }

    return produced_resources;
}

double component::get_sat(const std::vector<double>& sat)
{
    double min_sat = 1;

    for(does& d : info)
    {
        if(d.recharge > 0)
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

    for(component& c : stored)
    {
        if(!c.flows)
            continue;

        float vstored = c.get_my_volume();

        total_drainable += vstored;
    }

    if(total_drainable <= 0.00001f)
        return 0;

     if(amount > total_drainable)
        amount = total_drainable;

    for(component& c : stored)
    {
        if(!c.flows)
            continue;

        float vstored = c.get_my_volume();

        if(vstored < 0.00001f)
            continue;

        float my_frac = vstored / total_drainable;

        float to_drain = my_frac * amount;

        if(to_drain < 0.00001f)
            continue;

        for(material& m : c.composition)
        {
            float material_drain_volume = (m.dynamic_desc.volume / vstored) * to_drain;

            material_drain_volume = clamp(material_drain_volume, 0, m.dynamic_desc.volume);

            m.dynamic_desc.volume -= material_drain_volume;
        }
    }

    return amount;
}

void component::drain_from_to(component& c1_in, component& c2_in, float amount)
{
    if(amount < 0)
        return drain_from_to(c2_in, c1_in, -amount);

    float total_drainable = 0;

    for(component& c : c1_in.stored)
    {
        if(!c.flows)
            continue;

        float stored = c.get_my_volume();

        total_drainable += stored;
    }

    if(total_drainable <= 0.00001f)
        return;

    if(amount > total_drainable)
        amount = total_drainable;

    float internal_storage = c2_in.internal_volume;

    float free_volume = internal_storage - c2_in.get_stored_volume();

    if(amount > free_volume)
    {
        amount = free_volume;
    }

    for(component& c : c1_in.stored)
    {
        if(!c.flows)
            continue;

        float stored = c.get_my_volume();

        if(stored < 0.00001f)
            continue;

        float my_frac = stored / total_drainable;

        float to_drain = my_frac * amount;

        if(to_drain < 0.00001f)
            continue;

        component* found = nullptr;

        ///asserts that there is only ever one flowable component
        for(component& other : c2_in.stored)
        {
            if(!other.flows)
                continue;

            found = &other;
        }

        if(found == nullptr)
        {
            component next;
            next.add(component_info::HP, 0, 1);
            next.long_name = "Fluid";
            next.flows = true;

            c2_in.stored.push_back(next);

            found = &c2_in.stored.back();
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
    }
}

std::vector<double> ship::get_sat_percentage()
{
    std::vector<double> all_needed;
    all_needed.resize(component_info::COUNT);

    std::vector<double> all_produced;
    all_produced.resize(component_info::COUNT);

    for(component& c : components)
    {
        double hp = c.get_held()[component_info::HP];
        double max_hp = c.get_capacity()[component_info::HP];

        assert(max_hp > 0);

        for(does& d : c.info)
        {
            if(d.recharge < 0)
                all_needed[d.type] += d.recharge * (hp / max_hp) * c.last_production_frac * c.activation_level;

            if(d.recharge > 0)
                all_produced[d.type] += d.recharge * (hp / max_hp) * c.get_sat(last_sat_percentage) * c.last_production_frac * c.activation_level;
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

struct torpedo : projectile
{
    sf::Clock clk;
    sf::Clock inactivity_time;

    float homing_frequency = HEAT_FREQ;
    bool pinged = false;

    //vec2f last_best_dir = {0,0};

    uint32_t fired_by = -1;

    virtual void pre_collide(entity& other) override
    {
        alt_radar_field& radar = get_radar_field();

        alt_frequency_packet em;
        em.frequency = HEAT_FREQ;
        em.intensity = 10000;

        radar.emit(em, r.position - velocity.norm() * 5, *this);
    }

    virtual void tick(double dt_s) override
    {
        projectile::tick(dt_s);

        if(clk.getElapsedTime().asMicroseconds() / 1000. >= 50 * 1000)
        {
            cleanup = true;
        }

        double activate_time = 3 * 1000;
        bool activated = (inactivity_time.getElapsedTime().asMicroseconds() / 1000.0) >= activate_time;

        alt_radar_field& radar = get_radar_field();

        alt_radar_sample sam = radar.sample_for(r.position, *this, *parent);

        vec2f best_dir = {0, 0};
        float best_intensity = 0;

        for(auto& i : sam.receive_dir)
        {
            if(i.frequency != homing_frequency)
                continue;

            if((i.id_e == id || i.id_e == fired_by || i.id_r == id || i.id_r == fired_by) && !activated)
                continue;

            if(i.property.length() > best_intensity)
            {
                best_dir = i.property;
                best_intensity = i.property.length();
            }
        }

        for(auto& i : sam.echo_dir)
        {
            if(i.frequency != homing_frequency)
                continue;

            if((i.id_e == id || i.id_e == fired_by || i.id_r == id || i.id_r == fired_by) && !activated)
                continue;

            if(i.property.length() > best_intensity)
            {
                best_dir = i.property;
                best_intensity = i.property.length();
            }
        }

        for(auto& i : sam.echo_pos)
        {
            if(i.frequency != homing_frequency)
                continue;

            if((i.id_e == id || i.id_e == fired_by || i.id_r == id || i.id_r == fired_by) && !activated)
                continue;

            best_dir = i.property - r.position;
            best_intensity = BEST_UNCERTAINTY;
        }

        /*if((inactivity_time.getElapsedTime().asMicroseconds() / 1000.) >= 1 * 1000 && !pinged)
        {
            alt_frequency_packet em;
            em.frequency = HEAT_FREQ;
            em.intensity = 100000;

            radar.emit(em, r.position, id);

            pinged = true;

            printf("ping\n");
        }

        if(best_dir.x() == 0 && best_dir.y() == 0)
        {
            best_dir = last_best_dir;
        }

        last_best_dir = best_dir;*/

        if(best_dir.x() == 0 && best_dir.y() == 0)
        {
            return;
        }

        float accuracy = best_intensity / BEST_UNCERTAINTY;

        accuracy = clamp(accuracy, 0, 1);

        //accuracy = 1;

        float max_angle_per_s = dt_s * M_PI / 8;

        vec2f my_dir = (vec2f){1, 0}.rot(r.rotation);
        //vec2f my_dir = velocity;
        vec2f target_angle = best_dir;

        float requested_angle = signed_angle_between_vectors(my_dir, target_angle);

        if(fabs(requested_angle) > max_angle_per_s)
        {
            requested_angle = signum(requested_angle) * max_angle_per_s;
        }

        float angle_change = requested_angle;

        r.rotation += angle_change * accuracy;

        //r.rotation = point_angle;

        if(activated)
        {
            vec2f desired_velocity = best_dir.norm() * 100;

            vec2f real_velocity = velocity;

            double max_speed_ps = 30;

            vec2f project = projection(desired_velocity - real_velocity, (vec2f){1, 0}.rot(r.rotation));

            if(angle_between_vectors(project, (vec2f){1, 0}.rot(r.rotation)) > M_PI/2)
            {
                max_speed_ps = 10;
            }

            if(project.length() > max_speed_ps)
            {
                project = project.norm() * max_speed_ps;
            }

            float horizontal_speed_ps = 20;

            vec2f reject = projection(desired_velocity - real_velocity, (vec2f){1, 0}.rot(r.rotation + M_PI/2));

            if(reject.length() > horizontal_speed_ps)
            {
                reject = reject.norm() * horizontal_speed_ps;
            }

            velocity += project * dt_s;
            velocity += reject * dt_s * accuracy;


            alt_frequency_packet em;
            em.frequency = 1500;
            em.intensity = 1000;

            radar.emit(em, r.position, *this);

            phys_ignore.clear();

            //float max_speed = 100;
            //float max_speed_ps = 30;

            /*velocity = velocity + (vec2f){max_speed_ps, 0}.rot(point_angle) * dt_s;

            if(velocity.length() < max_speed)
            {
                velocity = velocity.norm() * velocity.length() + (vec2f){1, 0}.rot(point_angle) * max_speed_ps * dt_s;
            }

            if(velocity.length() > max_speed)
            {
                velocity = velocity.norm() * max_speed;
            }*/

            //velocity = velocity + (vec2f){10, 0}.rot(angle_change) * dt_s;
        }
    }
};

struct laser : projectile
{
    sf::Clock clk;

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

        alt_radar_field& radar = get_radar_field();

        alt_frequency_packet em;
        em.frequency = 3000;
        em.intensity = 10000;

        radar.emit(em, r.position, *this);
    }
};

void ship::tick(double dt_s)
{
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

    for(component& c : components)
    {
        bool any_under = false;

        double max_theoretical_activation_fraction = 0;

        for(does& d : c.info)
        {
            if(d.recharge <= 0)
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

        if(!any_under && c.no_drain_on_full_production)
        {
            c.last_production_frac = 0.1;
        }
        else
        {
            c.last_production_frac = 1;
        }

        if(c.complex_no_drain_on_full_production)
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
    }

    std::vector<double> all_sat = get_sat_percentage();

    //std::cout << "SAT " << all_sat[component_info::LIFE_SUPPORT] << std::endl;

    std::vector<double> produced_resources = get_net_resources(dt_s, all_sat);

    std::vector<double> next_resource_status;
    next_resource_status.resize(component_info::COUNT);

    for(int i=0; i < (int)component_info::COUNT; i++)
    {
        next_resource_status[i] = resource_status[i] + produced_resources[i];
    }

    for(component& c : components)
    {
        if(c.try_use)
        {
            if(c.can_use(next_resource_status))
            {
                c.use(next_resource_status);

                if(c.has(component_info::WEAPONS))
                {
                    double eangle = c.use_angle;
                    double ship_rotation = r.rotation;

                    vec2f evector = (vec2f){1, 0}.rot(eangle);
                    vec2f ship_vector = (vec2f){1, 0}.rot(ship_rotation);

                    alt_radar_field& radar = get_radar_field();

                    if(fabs(angle_between_vectors(ship_vector, evector)) > c.max_use_angle)
                    {
                        float angle_signed = signed_angle_between_vectors(ship_vector, evector);

                        evector = ship_vector.rot(signum(angle_signed) * c.max_use_angle);
                    }

                    if(c.subtype == "missile")
                    {
                        torpedo* l = parent->make_new<torpedo>();
                        l->r.position = r.position;
                        l->r.rotation = evector.angle();
                        //l->r.rotation = r.rotation + eangle;
                        //l->velocity = (vec2f){1, 0}.rot(r.rotation) * 50;
                        l->velocity = velocity + evector.norm() * 50;
                        //l->velocity = velocity + (vec2f){1, 0}.rot(r.rotation + eangle) * 50;
                        l->phys_ignore.push_back(id);
                        l->fired_by = id;
                    }

                    if(c.subtype == "laser")
                    {
                        laser* l = parent->make_new<laser>();
                        l->r.position = r.position;
                        l->r.rotation = evector.angle();
                        ///speed of light is notionally a constant
                        l->velocity = evector.norm() * (float)(radar.speed_of_light_per_tick / radar.time_between_ticks_s);
                        l->phys_ignore.push_back(id);
                    }

                    alt_frequency_packet em;
                    em.frequency = 1000;
                    em.intensity = 20000;

                    radar.emit(em, r.position, *this);
                }

                if(c.has(component_info::SENSORS))
                {
                    alt_radar_field& radar = get_radar_field();

                    alt_frequency_packet em;
                    em.frequency = 2000;
                    ///getting a bit complex to determine this value
                    em.intensity = 100000 * c.get_operating_efficiency();

                    if(!model)
                        radar.emit(em, r.position, *this);
                    else
                        radar.emit_with_imaginary_packet(em, r.position, *this, model);
                }

                ///damage surrounding targets
                ///should not be instantaneous but an expanding explosion ring, repurposable
                ///apply progressive damage to targets
                ///subtract damage from self before propagating to friends, or at least
                ///at minimum make it so it has to bust open the container, armour, and shields of us first
                if(c.has(component_info::SELF_DESTRUCT))
                {
                    for(component& store : c.stored)
                    {
                        std::pair<material_dynamic_properties, material_fixed_properties> props = get_material_composite(store.composition);
                        material_dynamic_properties& dynamic = props.first;
                        material_fixed_properties& fixed = props.second;

                        double power = dynamic.volume * fixed.specific_explosiveness;

                        if(power < resource_status[component_info::HP])
                        {
                            take_damage(power, true);
                        }
                        else
                        {
                            take_damage(resource_status[component_info::HP], true);
                            power -= resource_status[component_info::HP];

                            double radius = sqrt(power);

                            if(parent == nullptr)
                                throw std::runtime_error("Broken");

                            aoe_damage* aoe = parent->make_new<aoe_damage>();

                            aoe->max_radius = radius;
                            aoe->damage = power;
                            aoe->collided_with[id] = true;
                            aoe->r.position = r.position;
                            aoe->emitted_by = id;
                            aoe->velocity = velocity;
                        }
                    }
                }
            }

            c.try_use = false;
        }

        for(does& d : c.activate_requirements)
        {
            d.last_use_s += dt_s;
        }
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

            component::drain_from_to(c1, c2, p.flow_rate * dt_s);
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
                heat.frequency = HEAT_FREQ;
                heat.intensity = real_emissions * 100;

                alt_radar_field& radar = get_radar_field();

                if(!model)
                    radar.emit(heat, r.position, *this);
                else
                    radar.emit_with_imaginary_packet(heat, r.position, *this, model);
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

void ship::handle_heat(double dt_s)
{
    std::vector<double> all_produced = sum<double>([](auto c)
                                                   {
                                                       return c.get_produced();
                                                   });

    std::vector<double> all_needed = sum<double>([](auto c)
                                                   {
                                                       return c.get_needed();
                                                   });

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
        //double heat = c.heat_produced_at_full_usage * std::min(c.last_sat, c.last_production_frac * c.activation_level);

        double heat = c.heat_produced_at_full_usage * c.get_operating_efficiency();

        /*if(c.long_name == "Power Generator")
        {
            std::cout << "HEAT " << heat << std::endl;
        }*/

        if(c.production_heat_scales)
        {
            if(c.primary_type == component_info::COUNT)
                throw std::runtime_error("Bad primary type");

            double produced_comp = all_produced[c.primary_type];
            double excess_comp = all_needed[c.primary_type];

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

    int num_hs = 0;

    for(component& c : components)
    {
        if(!c.heat_sink)
            continue;

        num_hs++;
    }

    if(num_hs == 0)
        return;

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
        if(!c.heat_sink)
            continue;

        heat_sinks.push_back(&c);
    }

    ///long term use blueprint
    for(component& c : components)
    {
        float latent_fraction = latent_heat / components.size();

        if(!c.heat_sink || c.get_stored_volume() < 0.1)
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

        float my_temperature = c.get_my_temperature();

        float stored_temperature = c.get_stored_temperature();

        float temperature_difference = my_temperature - stored_temperature;

        float heat_transfer_rate = temperature_difference * heat_coeff * dt_s;

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
        }
    }

    for(component& c : components)
    {
        if(c.heat_sink)
            continue;

        float my_temperature = c.get_my_temperature();

        for(component* hsp : heat_sinks)
        {
            component& hs = *hsp;

            if(hs.get_stored_volume() < 0.1)
                continue;

            assert(hs.heat_sink);

            float hs_stored = hs.get_stored_temperature();

            float temperature_difference = my_temperature - hs_stored;

            if(temperature_difference <= 0)
                continue;

            ///ok
            ///assume every material has the same conductivity
            float heat_transfer_rate = temperature_difference * heat_coeff * dt_s;

            c.remove_heat_from_me(heat_transfer_rate);
            hs.add_heat_to_stored(heat_transfer_rate);
        }
    }

    ///radiators

    double heat_to_radiate = 0;

    for(component& c : components)
    {
        if(!c.has(component_info::RADIATOR))
            continue;

        does& d = c.get(component_info::RADIATOR);

        float my_temperature = c.get_my_temperature();

        for(component* hsp : heat_sinks)
        {
            component& hs = *hsp;

            if(hs.get_stored_volume() < 0.1)
                continue;

            /*float hs_stored = hs.get_stored_temperature();

            ///so latent heat is added to us, which is environmental heat
            ///so we can emit ignoring environmental heat and the equation is fine

            float heat_transfer_rate = hs_stored * heat_coeff * dt_s * d.recharge * c.get_operating_efficiency() / heat_sinks.size();

            hs.remove_heat_from_stored(heat_transfer_rate);

            heat_to_radiate += heat_transfer_rate;*/

            float hs_stored = hs.get_stored_temperature();

            ///ok so due to the above big block we already transfer heat from me to them, but need to do vice versa

            float temperature_difference = my_temperature - hs_stored;

            if(temperature_difference >= 0)
                continue;

            float heat_transfer_rate = -temperature_difference * heat_coeff * dt_s * c.get_operating_efficiency() / heat_sinks.size();

            c.add_heat_to_me(heat_transfer_rate);
            hs.remove_heat_from_stored(heat_transfer_rate);
        }

        float heat_transfer_rate = c.get_my_temperature() * heat_coeff * dt_s * d.recharge * c.get_operating_efficiency();

        c.remove_heat_from_me(heat_transfer_rate);

        heat_to_radiate += heat_transfer_rate;
    }

    latent_heat = 0;

    for(component& c : components)
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
                does& d = c.get(component_info::HP);

                apply_to_does(-real_damage, d);
            }
        }
    }

    {
        alt_radar_field& radar = get_radar_field();

        alt_frequency_packet heat;
        heat.frequency = HEAT_FREQ;
        heat.intensity = permanent_heat + heat_to_radiate * 100;
        //heat.packet_wavefront_width *= ticks_between_emissions;

        radar.emit(heat, r.position, *this);
    }

    //std::cout << "lheat " << latent_heat << std::endl;

    //dissipate();

    /*float emitted = latent_heat * 0.01;

    alt_radar_field& radar = get_radar_field();

    alt_frequency_packet heat;
    heat.frequency = HEAT_FREQ;
    heat.intensity = permanent_heat + emitted;
    heat.packet_wavefront_width *= ticks_between_emissions;

    radar.emit(heat, r.position, *this);

    latent_heat -= emitted;*/
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

    for(does& d : info)
    {
        net[d.type] += d.recharge;
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

void component::render_inline_ui()
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

    ImGui::Text((long_name).c_str());

    for(component& store : stored)
    {
        std::string name = store.long_name;

        float val = store.get_my_volume();
        float temperature = store.get_my_temperature();

        //ImGui::Text("Fluid:");

        std::string ext = "(s)";

        if(store.flows)
            ext = "(g)";

        std::string ext_str = std::to_string(_pid) + "." + std::to_string(store._pid);

        {
            ImGui::PushItemWidth(80);

            ImGuiX::SliderFloat("##riup" + ext_str, &val, 0, internal_volume, "%.1f");

            ImGui::PopItemWidth();
        }

        ImGui::SameLine();

        {
            ImGui::PushItemWidth(80);

            ImGuiX::SliderFloat("##riug" + ext_str, &temperature, 0, 6000, "%.0fK " + ext);

            ImGui::PopItemWidth();
        }
    }
}

void ship::show_resources()
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

    ImGui::Begin((std::string("Tship##") + std::to_string(network_owner)).c_str());

    ImGui::Text(ret.c_str());

    for(component& c : components)
    {
        if(!c.is_storage())
            continue;

        float max_stored = c.internal_volume;
        float cur_stored = c.get_stored_volume();

        //ret += "Stored: " + to_string_with_variable_prec(cur_stored) + "/" + to_string_with_variable_prec(max_stored) + "\n";

        ImGui::Text(("Stored: " + to_string_with_variable_prec(cur_stored) + "/" + to_string_with_variable_prec(max_stored)).c_str());

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

        ImGui::Begin((std::string("Tcomp##") + std::to_string(c.id)).c_str(), &c.detailed_view_open);

        c.render_inline_ui();

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
            c1.value()->render_inline_ui();
            c2.value()->render_inline_ui();

            ///the problem with this is that its not being communicated back to the server
            bool changed = ImGuiX::SliderFloat("", &p.flow_rate, -p.max_flow_rate, p.max_flow_rate);
            //ImGui::DragFloat("", &p.flow_rate, 0.01f, -p.max_flow_rate, p.max_flow_rate);

            if(changed)
                rpc("set_flow_rate", p, p.set_flow_rate, p.flow_rate);
        }

        if(c1 && p.goes_to_space)
        {
            c1.value()->render_inline_ui();

            ///have sun temperature here
            ImGui::Text("Space");

            bool changed = ImGuiX::SliderFloat("", &p.flow_rate, 0, p.max_flow_rate);

            if(changed)
                rpc("set_flow_rate", p, p.set_flow_rate, p.flow_rate);
        }

        ImGui::End();
    }

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
    std::string ppower = "Power##" + std::to_string(network_owner);

    ImGui::Begin(ppower.c_str());

    std::vector<component*> ui_sorted;

    for(component& c : components)
    {
        ui_sorted.push_back(&c);
    }

    std::sort(ui_sorted.begin(), ui_sorted.end(), [](component* c1, component* c2)
    {
        return c1->activation_type > c2->activation_type;
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

        ImVec4 default_slider_col = ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab);
        ImVec4 red_col = ImVec4(1, 0, 0, 1);


        //if(current_temperature >= fixed.melting_point)

        #define HORIZONTAL
        #ifdef HORIZONTAL
        //ImGui::Text((temperature + "K").c_str());

        ///HP
        {
            float hp = c.get_hp_frac() * 100;

            ImVec4 ccol = im4_mix(default_slider_col, red_col, 1 - c.get_hp_frac());

            ImVec4 text_col = im4_mix(ImVec4(1,1,1,1), red_col, 1 - c.get_hp_frac());

            ImGui::PushStyleColor(ImGuiCol_Text, text_col);
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, ccol);

            ImGui::PushItemWidth(80);

            ImGuiX::SliderFloat("##t" + std::to_string(c._pid), &hp, 0, 100, "%.0f%%");

            ImGui::PopItemWidth();

            ImGui::PopStyleColor(2);
        }

        ImGui::SameLine();

        ///temperature
        {
            float min_bad_temp = fixed.melting_point * 0.8;
            float max_bad_temp = fixed.melting_point;

            float bad_fraction = (current_temperature - min_bad_temp) / (max_bad_temp - min_bad_temp);

            float good_fraction = clamp(bad_fraction, 0, 1);

            bad_fraction = clamp(1 - bad_fraction, 0, 1);

            ImVec4 ccol = im4_mix(default_slider_col, red_col, good_fraction);

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,bad_fraction,bad_fraction,1));
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, ccol);

            ImGui::PushItemWidth(80);

            std::string fmt_string = "%.0fK";

            if(current_temperature > fixed.melting_point)
                fmt_string = "!" + fmt_string + "!";
            if(current_temperature > fixed.melting_point * 1.2)
                fmt_string = "!" + fmt_string + "!";
            if(current_temperature > fixed.melting_point * 1.5)
                fmt_string = "!" + fmt_string + "!";

            ImGuiX::SliderFloat("##b" + std::to_string(c._pid), &current_temperature, 0, fixed.melting_point, fmt_string);

            ImGui::PopItemWidth();

            ImGui::PopStyleColor(2);
        }

        ImGui::SameLine();

        if(c.activation_type == component_info::SLIDER_ACTIVATION)
        {
            ImGui::PushItemWidth(80);

            changed = ImGuiX::SliderFloat("##" + std::to_string(c._pid), &as_percentage, 0, 100, "%.0f");

            ImGui::PopItemWidth();
        }

        if(c.activation_type == component_info::TOGGLE_ACTIVATION)
        {
            bool enabled = as_percentage == 100;

            changed = ImGui::Checkbox(("##" + std::to_string(c._pid)).c_str(), &enabled);

            as_percentage = ((int)enabled) * 100;
        }

        ///well
        if(c.activation_type == component_info::NO_ACTIVATION)
        {

        }

        ImGui::SameLine();

        ImGui::Text(name.c_str());
        #endif // HORIZONTAL

        //#define VERTICAL
        #ifdef VERTICAL

        bool changed = ImGuiX::VSliderFloat("##" + std::to_string(c._pid), &as_percentage, 0, 100, "%.0f");

        ImGui::SameLine();

        #endif // VERTICAL

        c.activation_level = as_percentage / 100.f;

        if(changed)
            rpc("set_activation_level", c, c.set_activation_level, c.activation_level);
    }

    ImGui::NewLine();

    #if 0
    std::map<size_t, bool> processed_pipes;
    std::set<size_t> storage_comps;

    //ImGui::BeginGroup();

    //ImGui::Text("");

    //ImGui::EndGroup();

    //ImGui::SameLine();

    /*ImGui::PushItemWidth(0.1);

    float favdf = 0;
    ImGuiX::SliderFloat("##poop", &favdf, 0, 0);

    ImGui::PopItemWidth();*/

    ImGui::Button("", ImVec2(0.1, 0.1));

    ImGui::SameLine();

    int pidx = 0;

    while(processed_pipes.size() != pipes.size())
    {
        storage_pipe& first = pipes[pidx];

        auto c1 = get_component_from_id(first.id_1);

        storage_comps.insert(first.id_1);

        if(c1)
        {
            component& c = *c1.value();

            //ImGui::BeginGroup();

            //c.render_inline_ui();

            ImGui::Text((c.long_name).c_str());

            //ImGui::EndGroup();
        }

        ImGui::PushItemWidth(80);

        ImGui::SameLine();

        if(c1)
        {
            float lbound = -first.max_flow_rate;

            if(first.goes_to_space)
            {
                lbound = 0;
            }

            bool changed = ImGuiX::SliderFloat("##" + std::to_string(first._pid), &first.flow_rate, lbound, first.max_flow_rate);

            if(changed)
                rpc("set_flow_rate", first, first.set_flow_rate, first.flow_rate);

            ///nice idea but too distracting
            /*if(ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();

                auto c2 = get_component_from_id(first.id_2);

                ImGui::BeginGroup();

                c1.value()->render_inline_ui();

                ImGui::SameLine();

                ImGui::Text("to");

                ImGui::EndGroup();

                ImGui::SameLine();

                if(c2 && !first.goes_to_space)
                {
                    ImGui::SameLine();

                    ImGui::BeginGroup();

                    c2.value()->render_inline_ui();

                    ImGui::EndGroup();
                }
                else
                {
                    ImGui::SameLine();

                    ImGui::Text("Space");
                }

                ImGui::EndTooltip();
            }*/
        }

        ImGui::PopItemWidth();

        //ImGui::Text(std::to_string(first.id_1).c_str());

        processed_pipes[first._pid] = true;

        bool any = false;
        size_t any_id = -1;

        for(int i=0; i < pipes.size() && !first.goes_to_space; i++)
        {
            if(pipes[i].id_1 == first.id_2)
            {
                pidx = i;
                any = true;
                any_id = i;
                break;
            }
        }

        if(any)
        {
            ImGui::SameLine();

            pidx = any_id;
            continue;
        }

        if(first.goes_to_space)
        {
            ImGui::SameLine();

            ImGui::Text("Space");

            ImGui::Button("", ImVec2(0.1, 0.1));
            ImGui::SameLine();

            continue;
        }

        for(int i=0; i < (int)pipes.size(); i++)
        {
            if(processed_pipes.find(pipes[i]._pid) == processed_pipes.end())
            {
                pidx = any_id;
                break;
            }
        }

        ImGui::Button("", ImVec2(0.1, 0.1));
        ImGui::SameLine();
    }

    if(pipes.size() > 0)
    {
        ImGui::Text("");
    }
    #endif // 0

    std::vector<std::string> formatted_names;

    for(storage_pipe& pipe : pipes)
    {
        auto c1 = get_component_from_id(pipe.id_1);

        if(!c1)
            continue;

        formatted_names.push_back(c1.value()->long_name);
    }

    std::set<size_t> storage_comps;

    //for(storage_pipe& pipe : pipes)

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
            rpc("set_flow_rate", pipe, pipe.set_flow_rate, pipe.flow_rate);

        ImGui::PopItemWidth();

        ImGui::SameLine();

        if(c1)
        {
            component& c = *c1.value();

            ImGui::Text((format(c.long_name, formatted_names) + " <-> ").c_str());

            //ImGui::SameLine();

            storage_comps.insert(pipe.id_1);
        }

        if(c2 && !pipe.goes_to_space)
        {
            component& c = *c2.value();

            ImGui::SameLine();

            ImGui::Text(c.long_name.c_str());

            storage_comps.insert(pipe.id_2);
        }
        else
        {
            ImGui::SameLine();

            ImGui::Text("Space");
        }
    }

    ImGui::NewLine();

    for(auto id : storage_comps)
    {
        auto c1 = get_component_from_id(id);

        if(!c1)
            continue;

        component& c = *c1.value();

        c.render_inline_ui();
    }

    ImGui::End();
}

std::vector<double> ship::get_capacity()
{
    return sum<double>
           ([](component& c)
           {
              return c.get_capacity();
           });
}

void ship::advanced_ship_display()
{
    ImGui::Begin(("Advanced Ship" + std::to_string(network_owner)).c_str());

    #if 0
    for(auto& i : data_track)
    {
        if(i.vsat.size() == 0)
            continue;

        /*std::string name_1 = component_info::dname[i.first];
        std::string name_2 = component_info::dname[i.first];

        std::vector<std::string> names{name_1, name_2};

        std::string label = "hi";

        std::vector<ImColor> cols;

        cols.push_back(ImColor(255, 128, 128, 255));
        cols.push_back(ImColor(128, 128, 255, 255));

        ImGuiX::PlotMultiEx(ImGuiPlotType_Lines, label.c_str(), names, cols, {i.second.vsat, i.second.vheld}, 0, 1, ImVec2(0,0));*/

        //ImGui::PlotHistogram(name.c_str(), &i.second.data[0], i.second.data.size(), 0, nullptr, 0, 1);
        //ImGui::PlotLines(name_1.c_str(), &i.second.vsat[0], i.second.vsat.size(), 0, nullptr, 0, 1);

        //ImGui::PlotHistogram(name.c_str(), &i.second.data[0], i.second.data.size(), 0, nullptr, 0, get_capacity()[i.first]);

        //ImGui::PlotLines(name.c_str(), &i.second.data[0], i.second.data.size(), 0, nullptr, 0, get_capacity()[i.first]);
    }
    #endif // 0

    ImGui::Columns(2);

    ImGui::Text("Stored");

    //for(auto& i : data_track)

    //for(int kk=0; kk < (int)data_track.size(); kk++)
    for(auto& kk : tracked)
    {
        if(data_track[kk].vheld.size() == 0)
            continue;

        std::string name_1 = component_info::dname[kk];

        ImGui::PlotLines(name_1.c_str(), &(data_track[kk].vheld)[0], data_track[kk].vheld.size(), 0, nullptr, 0, get_capacity()[kk], ImVec2(0, 0));
    }

    ImGui::NextColumn();

    ImGui::Text("Satisfied");

    //for(auto& i : data_track)

    for(auto& kk : tracked)
    {
        if((data_track)[kk].vsat.size() == 0)
            continue;

        if(kk == component_info::CAPACITOR)
        {
            ImGui::NewLine();
            continue;
        }

        std::string name_1 = component_info::dname[kk];

        ImGui::PlotLines(name_1.c_str(), &(data_track[kk].vsat)[0], data_track[kk].vsat.size(), 0, nullptr, 0, 1, ImVec2(0, 0));
    }

    ImGui::EndColumns();

    for(component& c : components)
    {
        if(c.has(component_info::WEAPONS))
        {
            ImGui::Button("Fire");

            if(ImGui::IsItemClicked())
            {
                rpc("set_use", c, c.set_use);

                //c.try_use = true;
            }
        }

        if(c.has(component_info::SELF_DESTRUCT))
        {
            ImGui::Button("DESTRUCT");

            if(ImGui::IsItemClicked())
            {
                rpc("set_use", c, c.set_use);
            }
        }
    }

    ImGui::End();
}

void ship::apply_force(vec2f dir)
{
    control_force += dir.rot(r.rotation);
}

void ship::apply_rotation_force(float force)
{
    control_angular_force += force;
}

void ship::tick_pre_phys(double dt_s)
{
    double thrust = get_net_resources(1, last_sat_percentage)[component_info::THRUST] * 2;

    //std::cout << "thrust " << thrust << std::endl;

    ///so to convert from angular to velocity multiply by this
    double velocity_thrust_ratio = 20;

    apply_inputs(dt_s, thrust * velocity_thrust_ratio, thrust);
    //e.tick_phys(dt_s);
}

double apply_to_does(double amount, does& d)
{
    double prev = d.held;

    double next = prev + amount;

    if(next < 0)
        next = 0;

    if(next > d.capacity)
        next = d.capacity;

    d.held = next;

    return next - prev;
}

void ship::take_damage(double amount, bool internal_damage)
{
    double remaining = -amount;

    if(!internal_damage)
    {
        for(component& c : components)
        {
            if(c.has(component_info::SHIELDS))
            {
                does& d = c.get(component_info::SHIELDS);

                double diff = apply_to_does(remaining, d);

                remaining -= diff;
            }
        }

        for(component& c : components)
        {
            if(c.has(component_info::ARMOUR))
            {
                does& d = c.get(component_info::ARMOUR);

                double diff = apply_to_does(remaining, d);

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
            does& d = c.get(component_info::HP);

            double diff = apply_to_does(remaining, d);

            remaining -= diff;
        }
    }
}

void ship::add_pipe(const storage_pipe& p)
{
    pipes.push_back(p);
}

double ship::get_radar_strength()
{
    std::vector<double> net = sum<double>
    ([](component& c)
    {
        return c.get_needed();
    });

    return clamp(net[component_info::SENSORS], 0, 1);
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

        if(dynamic_cast<ship*>(&other))
        {
            dynamic_cast<ship*>(&other)->take_damage(their_change.length()/2);
        }
    }
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
            if(c.has(component_info::WEAPONS))
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

asteroid::asteroid()
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
}

void asteroid::tick(double dt_s)
{
    dissipate();
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

    for(alt_object_property<common_renderable>& e : sample.renderables)
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

void client_entities::render(camera& cam, sf::RenderWindow& win)
{
    for(client_renderable& i : entities)
    {
        i.render(cam, win);
    }
}
