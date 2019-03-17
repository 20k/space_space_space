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

        needed[d.type] += d.recharge * (hp / max_hp) * mod * last_production_frac;
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

        needed[d.type] += d.recharge * (hp / max_hp) * mod  * last_production_frac;
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

void component::deplete_me(std::vector<double>& diff)
{
    for(does& d : info)
    {
        double my_held = d.held;
        double my_cap = d.capacity;

        double change = diff[d.type];

        double next = my_held + change;

        if(next < 0)
            next = 0;

        if(next > my_cap)
            next = my_cap;

        double real = next - my_held;

        diff[d.type] -= real;

        d.held = next;
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
    return my_volume;
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

float component::get_stored_temperature()
{
    return my_temperature;
}

void component::add_heat_to_stored(float temperature)
{
    for(component& c : stored)
    {
        float total_heat = 0;

        for(material& m : c.composition)
        {
            total_heat += material_info::fetch(m.type).specific_heat * m.dynamic_desc.volume;
        }

        if(total_heat < 0.0001)
            continue;

        for(component& c : stored)
        {
            c.my_temperature += temperature / total_heat;
        }
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

void component::add_composition(material_info::material_type type, double volume)
{
    material new_mat;
    new_mat.type = type;
    new_mat.dynamic_desc.volume = volume;

    my_volume += volume;

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
                produced_resources[d.type] += d.recharge * (hp / max_hp) * min_sat * dt_s * c.last_production_frac;
            else
                produced_resources[d.type] += d.recharge * (hp / max_hp) * dt_s * c.last_production_frac;
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
                all_needed[d.type] += d.recharge * (hp / max_hp) * c.last_production_frac;

            if(d.recharge > 0)
                all_produced[d.type] += d.recharge * (hp / max_hp) * c.get_sat(last_sat_percentage) * c.last_production_frac;
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

        alt_radar_sample sam = radar.sample_for(r.position, id, *parent);

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

            float max_speed = 100;
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

    for(component& c : components)
    {
        bool any_under = false;

        for(does& d : c.info)
        {
            if(d.recharge <= 0)
                continue;

            if(resource_status[d.type] < max_resource_status[d.type])
            {
                any_under = true;
                break;
            }
        }

        if(!any_under && c.no_drain_on_full_production)
        {
            c.last_production_frac = 0.1;
        }
        else
        {
            c.last_production_frac = 1;
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
                    em.intensity = 100000;

                    if(!model)
                        radar.emit(em, r.position, *this);
                    else
                        radar.emit_with_imaginary_packet(em, r.position, *this, model);
                }
            }

            c.try_use = false;
        }

        for(does& d : c.activate_requirements)
        {
            d.last_use_s += dt_s;
        }
    }

    handle_heat(dt_s);

    std::vector<double> diff;
    diff.resize(component_info::COUNT);

    for(int i=0; i < (int)component_info::COUNT; i++)
    {
        diff[i] = next_resource_status[i] - resource_status[i];
    }

    for(component& c : components)
    {
        c.deplete_me(diff);
    }

    last_sat_percentage = all_sat;



    data_track_elapsed_s += dt_s;

    double time_between_datapoints_s = 0.1;

    if(data_track_elapsed_s >= time_between_datapoints_s)
    {
        for(auto& type : tracked)
        {
            double held = resource_status[type];
            double sat = all_sat[type];

            //std::cout << "dtrack size " << (*data_track).size() << std::endl;

            data_track[type]->add(sat, held);
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

    double min_heat = 50;
    double produced_heat = 0;

    for(component& c : components)
    {
        double heat = c.heat_produced_at_full_usage * std::min(c.last_sat, c.last_production_frac);

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

        produced_heat += heat;
    }

    double coolant_to_heat_drain = 100;
    double heat_drained = all_produced[component_info::COOLANT] * coolant_to_heat_drain;

    produced_heat -= heat_drained;

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

void component::render_inline_ui()
{
    std::string total = "Storage: " + to_string_with_variable_prec(get_stored_volume()) + "/" + to_string_with_variable_prec(internal_volume);

    ImGui::Text(total.c_str());

    for(component& stored : stored)
    {
        std::string str = stored.long_name;

        str += " (" + to_string_with_variable_prec(stored.my_volume) + ") " + to_string_with_variable_prec(stored.get_stored_temperature()) + "K";

        ImGui::Text(str.c_str());
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

        std::string conn_str = "Connects " + std::to_string(p.id_1) + " with " + std::to_string(p.id_2);

        ImGui::Text(conn_str.c_str());

        auto c1 = get_component_from_id(p.id_1);
        auto c2 = get_component_from_id(p.id_2);

        if(c1 && c2)
        {
            c1.value()->render_inline_ui();
            c2.value()->render_inline_ui();
        }

        ImGui::End();
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
        if(data_track[kk]->vheld.size() == 0)
            continue;

        std::string name_1 = component_info::dname[kk];

        ImGui::PlotLines(name_1.c_str(), &(data_track[kk]->vheld)[0], data_track[kk]->vheld.size(), 0, nullptr, 0, get_capacity()[kk], ImVec2(0, 0));
    }

    ImGui::NextColumn();

    ImGui::Text("Satisfied");

    //for(auto& i : data_track)

    for(auto& kk : tracked)
    {
        if((data_track)[kk]->vsat.size() == 0)
            continue;

        if(kk == component_info::CAPACITOR)
        {
            ImGui::NewLine();
            continue;
        }

        std::string name_1 = component_info::dname[kk];

        ImGui::PlotLines(name_1.c_str(), &(data_track[kk]->vsat)[0], data_track[kk]->vsat.size(), 0, nullptr, 0, 1, ImVec2(0, 0));
    }

    ImGui::EndColumns();

    for(component& c : components)
    {
        if(c.has(component_info::WEAPONS))
        {
            ImGui::Button("Fire");

            if(ImGui::IsItemClicked())
            {
                c.try_use = true;
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

void ship::take_damage(double amount)
{
    double remaining = -amount;

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

        for(int kk=0; kk < sample.frequencies.size(); kk++)
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
