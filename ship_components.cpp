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

ship::ship()
{
    last_sat_percentage.resize(component_info::COUNT);

    for(auto& i : last_sat_percentage)
        i = 1;

    vec2f dim = {1, 2};

    r.init_rectangular(dim);

    for(auto& i : r.vert_cols)
    {
        i = {0.5, 1, 0.5};
    }

    r.scale = 2;

    r.vert_cols[3] = {1, 0.2, 0.2};

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

void component::add_on_use(component_info::does_type type, double amount)
{
    does d;
    d.type = type;
    d.capacity = amount;
    d.held = 0;

    activate_requirements.push_back(d);
}

double component::satisfied_percentage(double dt_s, const std::vector<double>& res)
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
}

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

        needed[d.type] += d.recharge * (hp / max_hp) * mod;
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
    }

    return true;
}

void component::use(std::vector<double>& res)
{
    for(does& d : activate_requirements)
    {
        res[d.type] += d.capacity;
    }

    try_use = false;
}

std::vector<double> ship::get_produced(double dt_s, const std::vector<double>& all_sat)
{
    std::vector<double> produced_resources;
    produced_resources.resize(component_info::COUNT);

    for(component& c : components)
    {
        double hp = c.get_held()[component_info::HP];
        double max_hp = c.get_capacity()[component_info::HP];

        assert(max_hp > 0);



        /*if(c.has(component_info::CREW))
        {
            printf("Min sat %lf\n", min_sat);
        }*/

        double min_sat = c.get_sat(all_sat);

        for(does& d : c.info)
        {
            if(d.recharge > 0)
                produced_resources[d.type] += d.recharge * (hp / max_hp) * min_sat * dt_s;
            else
                produced_resources[d.type] += d.recharge * (hp / max_hp) * dt_s;
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
                all_needed[d.type] += d.recharge * (hp / max_hp);

            if(d.recharge > 0)
                all_produced[d.type] += d.recharge * (hp / max_hp) * c.get_sat(last_sat_percentage);
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

void ship::tick(double dt_s)
{
    std::vector<double> resource_status = sum<double>([](component& c)
                                                      {
                                                          return c.get_held();
                                                      });

    std::vector<double> all_sat = get_sat_percentage();

    //std::cout << "SAT " << all_sat[component_info::LIFE_SUPPORT] << std::endl;

    std::vector<double> produced_resources = get_produced(dt_s, all_sat);

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

                projectile* l = parent->make_new<projectile>();
                l->r.position = r.position;
                l->r.rotation = r.rotation;
                l->velocity = (vec2f){0, 1}.rot(r.rotation) * 100;
                //l->velocity = velocity + (vec2f){0, 1}.rot(rotation) * 100;
                l->phys_ignore.push_back(id);

                alt_radar_field& radar = get_radar_field();

                alt_frequency_packet em;
                em.frequency = 1000;
                em.intensity = 50000;

                radar.emit(em, r.position, id);
            }

            c.try_use = false;
        }
    }

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

void ship::add(const component& c)
{
    components.push_back(c);
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

std::string ship::show_resources()
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

    return ret;
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
    double thrust = get_produced(1, last_sat_percentage)[component_info::THRUST] * 2;

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
    rng.seed(std::random_device()());
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

void ship::fire()
{
    for(component& c : components)
    {
        if(c.has(component_info::WEAPONS))
        {
            c.try_use = true;
        }
    }
}

projectile::projectile()
{
    r.init_rectangular({0.2, 1});
}

void projectile::on_collide(entity_manager& em, entity& other)
{
    if(dynamic_cast<ship*>(&other))
    {
        dynamic_cast<ship*>(&other)->take_damage(11);
    }

    cleanup = true;
}

asteroid::asteroid()
{
    float min_rad = 2;
    float max_rad = 4;

    int n = 5;

    for(int i=0; i < n; i++)
    {
        r.vert_dist.push_back(randf_s(min_rad, max_rad));

        float angle = ((float)i / n) * 2 * M_PI;

        r.vert_angle.push_back(angle);

        r.vert_cols.push_back({1,1,1});
    }

    r.approx_rad = max_rad;
    r.approx_dim = {max_rad*2, max_rad*2};
}

void asteroid::tick(double dt_s)
{
    alt_radar_field& radar = get_radar_field();

    radar.add_simple_collideable(r.rotation, r.approx_dim, r.position, id);
}

struct transient_entity : entity
{
    bool immediate_destroy = false;
    uint32_t server_id = 0;
    sf::Clock clk;
    int echo_type = -1;

    transient_entity()
    {
        r.init_rectangular({5, 5});
    }

    virtual void tick(double dt_s) override
    {
        double time_ms = clk.getElapsedTime().asMicroseconds() / 1000.;

        if(time_ms >= 500)
        {
            cleanup = true;
        }

        if(immediate_destroy)
            cleanup = true;
    }
};

void tick_radar_data(entity_manager& entities, const alt_radar_sample& sample, entity* ship_proxy)
{
    for(int i=0; i < (int)sample.echo_pos.size(); i++)
    {
        uint32_t fid = sample.echo_pos[i].id;
        bool found = false;

        for(entity* e : entities.entities)
        {
            transient_entity* transient = dynamic_cast<transient_entity*>(e);

            if(transient == nullptr)
                continue;

            if(fid == transient->server_id && transient->echo_type == 0)
            {
                found = true;
                transient->clk.restart();
                transient->r.position = sample.echo_pos[i].property;
                transient->set_parent_entity(ship_proxy, transient->r.position);
            }
        }

        if(!found)
        {
            transient_entity* next = entities.make_new<transient_entity>();
            next->server_id = fid;
            next->r.position = sample.echo_pos[i].property;
            next->echo_type = 0;
            next->clk.restart();

            next->set_parent_entity(ship_proxy, next->r.position);
        }
    }

    for(int i=0; i < (int)sample.echo_dir.size(); i++)
    {
        uint32_t fid = sample.echo_dir[i].id;
        bool found = false;

        transient_entity* next = nullptr;

        for(entity* e : entities.entities)
        {
            transient_entity* transient = dynamic_cast<transient_entity*>(e);

            if(transient == nullptr)
                continue;

            if(fid == transient->server_id && transient->echo_type == 1)
            {
                next = transient;
                break;
            }
        }

        if(next == nullptr)
            next = entities.make_new<transient_entity>();

        float intensity = sample.echo_dir[i].property.length();

        intensity = clamp(intensity*10, 1, 50);

        next->r.position = sample.location + sample.echo_dir[i].property.norm() * (intensity + 10);
        next->r.init_rectangular({intensity, 1});
        next->r.rotation = sample.echo_dir[i].property.angle();
        next->clk.restart();
        next->server_id = fid;
        next->echo_type = 1;

        next->set_parent_entity(ship_proxy, next->r.position);
    }

    for(int i=0; i < (int)sample.receive_dir.size(); i++)
    {
        transient_entity* next = entities.make_new<transient_entity>();

        float intensity = sample.receive_dir[i].property.length();

        intensity = clamp(intensity*10, 1, 50);

        next->r.position = sample.location + sample.receive_dir[i].property.norm() * (intensity + 10);
        next->r.init_rectangular({intensity, 1});
        next->r.rotation = sample.receive_dir[i].property.angle();
        next->immediate_destroy = true;
        next->echo_type = 2;
        next->set_parent_entity(ship_proxy, next->r.position);

        for(auto& i : next->r.vert_cols)
        {
            i = {1, 0, 0};
        }
    }
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
