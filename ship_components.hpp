#ifndef SHIP_COMPONENTS_HPP_INCLUDED
#define SHIP_COMPONENTS_HPP_INCLUDED

#include <vector>
#include <string>
#include <map>
#include <vec/vec.hpp>
#include <optional>

#include "entity.hpp"
#include "networking/networking.hpp"
#include "radar_field.hpp"
#include "material_info.hpp"

namespace sf
{
    struct RenderWindow;
}

namespace component_info
{
    enum does_type
    {
        HP,
        THRUST,
        WARP,
        SHIELDS,
        WEAPONS,
        SENSORS,
        COMMS,
        ARMOUR,
        LIFE_SUPPORT,
        RADIATOR,
        CREW,
        POWER,
        CAPACITOR, ///weapons
        MISSILE_STORE,
        ORE,
        MATERIAL,
        SELF_DESTRUCT,
        COUNT
    };

    static inline std::vector<std::string> dname
    {
        "HP",
        "Thrust",
        "Warp",
        "Shield",
        "Weapons",
        "Sensors",
        "Comms",
        "Armour",
        "Oxygen",
        "Radiator",
        "Crew",
        "Power",
        "Capacitor",
        "Missiles",
        "Ore",
        "Material",
        "Self Destruct",
    };

    enum activation_type
    {
        NO_ACTIVATION,
        TOGGLE_ACTIVATION,
        SLIDER_ACTIVATION,
    };
}

namespace tag_info
{
    enum tag_type
    {
        TAG_NONE,
        TAG_EJECTOR,
    };
}

struct does : serialisable
{
    double capacity = 0;
    double held = 0;
    double recharge = 0;

    double time_between_use_s = 0;
    double last_use_s = 0;

    component_info::does_type type = component_info::COUNT;

    SERIALISE_SIGNATURE()
    {
        DO_SERIALISE(capacity);
        DO_SERIALISE(held);
        DO_SERIALISE(recharge);
        DO_SERIALISE(time_between_use_s);
        DO_SERIALISE(last_use_s);
        DO_SERIALISE(type);
    }
};

struct tag : serialisable
{
    tag_info::tag_type type = tag_info::TAG_NONE;

    SERIALISE_SIGNATURE()
    {
        DO_SERIALISE(type);
    }
};

struct storage_pipe : serialisable, owned
{
    size_t id_1 = 0;
    size_t id_2 = 0;

    ///+ve = id_1 -> id_2
    ///-ve = id_2 -> id_1
    float flow_rate = 0;
    float max_flow_rate = 0;

    bool goes_to_space = false;

    ///ui
    bool is_open = false;

    ///uh ok rpcs
    ///how doth implement

    SERIALISE_SIGNATURE()
    {
        DO_SERIALISE(id_1);
        DO_SERIALISE(id_2);

        DO_SERIALISE(flow_rate);
        DO_SERIALISE(max_flow_rate);
        DO_SERIALISE(goes_to_space);

        DO_RPC(set_flow_rate);
    }

    void set_flow_rate(float in)
    {
        if(isnan(in) || isinf(in))
            return;

        flow_rate = in;

        flow_rate = clamp(flow_rate, -max_flow_rate, max_flow_rate);
    }
};

struct ship;


template<typename C, typename T>
void for_each_ship_hackery(const C& c, T t);

template<typename C, typename T>
void for_each_ship_hackery(C& c, T t);

struct component : virtual serialisable, owned
{
    std::vector<does> info;
    std::vector<does> activate_requirements;
    std::vector<tag> tags;

    bool no_drain_on_full_production = false;
    bool complex_no_drain_on_full_production = false;

    static inline uint32_t gid = 0;
    uint32_t id = gid++;

    std::string long_name;
    std::string short_name;

    ///?????????????????
    ///this will go to the house
    std::string subtype;

    double last_sat = 1;
    bool flows = false;
    bool heat_sink = false;

    ///how active i need to be
    ///only applies to no_drain_on_full_production
    ///has a minimum value to prevent accidental feedback loops
    double last_production_frac = 1;
    ///user requested activation level
    float activation_level = 1;
    component_info::activation_type activation_type = component_info::NO_ACTIVATION;

    ///does heat scale depending on how much of the output is used?
    ///aka power gen
    bool production_heat_scales = false;
    component_info::does_type primary_type = component_info::COUNT;

    SERIALISE_SIGNATURE()
    {
        DO_SERIALISE(info);
        DO_SERIALISE(activate_requirements);
        DO_SERIALISE(tags);
        DO_SERIALISE(long_name);
        DO_SERIALISE(short_name);
        DO_SERIALISE(last_sat);
        DO_SERIALISE(flows);
        DO_SERIALISE(heat_sink);
        DO_SERIALISE(no_drain_on_full_production);
        DO_SERIALISE(complex_no_drain_on_full_production);
        DO_SERIALISE(last_production_frac);
        DO_SERIALISE(max_use_angle);
        DO_SERIALISE(subtype);
        DO_SERIALISE(production_heat_scales);
        //DO_SERIALISE(my_volume);
        DO_SERIALISE(internal_volume);
        DO_SERIALISE(current_scale);
        DO_SERIALISE(stored);
        DO_SERIALISE(primary_type);
        DO_SERIALISE(id);
        DO_SERIALISE(composition);
        DO_SERIALISE(my_temperature);
        DO_SERIALISE(activation_level);
        DO_SERIALISE(activation_type);
        DO_RPC(set_activation_level);
        DO_RPC(set_use);
    }

    double satisfied_percentage(double dt_s, const std::vector<double>& res);
    void apply(const std::vector<double>& efficiency, double dt_s, std::vector<double>& res);

    void add(component_info::does_type, double amount);
    void add(component_info::does_type, double amount, double cap);
    void add(tag_info::tag_type);

    void add_on_use(component_info::does_type, double amount, double time_between_use_s);

    void set_heat(double heat);
    void set_heat_scales_by_production(bool status, component_info::does_type primary);

    void set_no_drain_on_full_production();
    void set_complex_no_drain_on_full_production();

    bool has(component_info::does_type type)
    {
        for(auto& i : info)
        {
            if(i.type == type)
                return true;
        }

        return false;
    }

    bool has_tag(tag_info::tag_type tag)
    {
        for(auto& i : tags)
        {
            if(i.type == tag)
                return true;
        }

        return false;
    }

    does& get(component_info::does_type type)
    {
        if(!has(type))
            throw std::runtime_error("rip get");

        for(auto& d : info)
        {
            if(d.type == type)
                return d;
        }

        throw std::runtime_error("rg2");
    }

    ///ignores anything < 0
    std::vector<double> get_theoretical_produced();
    std::vector<double> get_theoretical_consumed();
    std::vector<double> get_produced();
    ///both > 0 && < 0
    std::vector<double> get_needed();
    std::vector<double> get_capacity();
    std::vector<double> get_held();

    double get_sat(const std::vector<double>& sat);

    void deplete_me(const std::vector<double>& diff, const std::vector<double>& free_storage, const std::vector<double>& used_storage);

    bool can_use(const std::vector<double>& res);
    void use(std::vector<double>& res);

    bool try_use = false;
    double use_angle = 0;
    double max_use_angle = 0;

    double heat_produced_at_full_usage = 0;

    ///my volume is how large we are as a thing
    ///internal volume is how much can be stored within me
    ///internal volume should probably be less than my_volume
    //float my_volume = 0;

    ///no longer have a concept of my_volume, purely based off composition
    float internal_volume = 0;
    float current_scale = 1;

    std::vector<ship> stored;
    std::vector<material> composition;

    float my_temperature = 0;

    //std::vector<component> drain_amount_from_storage(float amount);

    float drain(float amount);
    static void drain_from_to(component& c1, component& c2, float amount);

    float get_my_volume() const;
    float get_stored_volume() const;

    float get_my_temperature();
    float get_my_contained_heat();
    float get_stored_temperature();
    float get_stored_heat_capacity();

    void add_heat_to_me(float heat);
    void remove_heat_from_me(float heat);
    void add_heat_to_stored(float heat);
    void remove_heat_from_stored(float heat);

    bool can_store(const component& c);
    bool can_store(const ship& s);
    void store(const component& c);
    void store(const ship& s);
    bool is_storage();
    bool should_flow();

    float get_hp_frac();
    float get_operating_efficiency();

    void scale(float size);

    ///update this to handle fractions, heat, and compounding existing components
    void add_composition(material_info::material_type type, double volume);

    void render_inline_stats();
    void render_inline_ui();

    ///do not network
    ///needs some adjustments to the network, need to fix ownership n stuff
    bool detailed_view_open = false;

    void set_activation_level(double level)
    {
        if(isinf(level) || isnan(level))
            return;

        if(activation_type == component_info::NO_ACTIVATION)
        {
            activation_level = 1;
            return;
        }

        if(activation_type == component_info::TOGGLE_ACTIVATION)
        {
            if(level < 0.5)
                level = 0;
            if(level >= 0.5)
                level = 1;
        }

        activation_level = clamp(level, 0., 1.);
    }

    void set_use()
    {
        try_use = true;
    }

    template<typename T>
    void for_each_stored(T t)
    {
        return for_each_ship_hackery(*this, t);
    }

    template<typename T>
    void for_each_stored(T t) const
    {
        return for_each_ship_hackery(*this, t);
    }
};

struct data_tracker : serialisable, owned
{
    std::vector<float> vsat;
    std::vector<float> vheld;
    int max_data = 500;

    void add(double sat, double held);

    SERIALISE_SIGNATURE()
    {
        DO_SERIALISE(vsat);
        DO_SERIALISE(vheld);
        DO_SERIALISE(max_data);

        if(ctx.serialisation)
        {
            while((int)vsat.size() > max_data)
            {
                vsat.erase(vsat.begin());
            }

            while((int)vheld.size() > max_data)
            {
                vheld.erase(vheld.begin());
            }
        }
    }
};

struct alt_radar_field;
struct client_fire;
struct player_model;

struct ship : heatable_entity, owned
{
    size_t network_owner = 0;

    std::vector<component> components;
    std::vector<storage_pipe> pipes;

    std::vector<component_info::does_type> tracked
    {
        component_info::WARP,
        component_info::SHIELDS,
        component_info::ARMOUR,
        component_info::HP,
        component_info::POWER,
        component_info::CAPACITOR
    };

    //std::map<component_info::does_type, data_tracker> data_track;

    ///so we want a diff wrapper type
    //std::vector<data_tracker> data_track;

    player_model* model = nullptr;

    std::vector<data_tracker> data_track;

    ship();

    std::optional<component*> get_component_from_id(uint64_t id);

    void handle_heat(double dt_s);
    void tick(double dt_s) override;
    void tick_pre_phys(double dt_s) override;
    //void render(sf::RenderWindow& win);

    void add(const component& c);

    void set_thrusters_active(double frac);

    std::vector<double> get_sat_percentage();
    std::vector<double> get_net_resources(double dt_s, const std::vector<double>& sat_in);

    std::vector<double> get_capacity();

    std::vector<double> last_sat_percentage;

    //std::string show_components();
    void show_resources(bool window = true);
    void show_power();

    template<typename T, typename U>
    std::vector<T> sum(U in)
    {
        std::vector<T> ret;

        for(component& c : components)
        {
            auto vec = in(c);

            if(ret.size() < vec.size())
                ret.resize(vec.size());

            for(int i=0; i < (int)vec.size(); i++)
            {
                ret[i] += vec[i];
            }
        }

        if(ret.size() != component_info::COUNT)
        {
            ret.resize(component_info::COUNT);
        }

        return ret;
    }

    void advanced_ship_display();

    void apply_force(vec2f dir);
    void apply_rotation_force(float force);

    void fire(const std::vector<client_fire>& fired);
    void ping();
    void take_damage(double amount, bool internal_damage = false);

    double data_track_elapsed_s = 0;

    void add_pipe(const storage_pipe& p);

    double get_radar_strength();

    SERIALISE_SIGNATURE()
    {
        DO_SERIALISE(data_track);
        DO_SERIALISE(network_owner);
        DO_SERIALISE(components);
        DO_SERIALISE(last_sat_percentage);
        DO_SERIALISE(latent_heat);
        DO_SERIALISE(pipes);
    }

    virtual void on_collide(entity_manager& em, entity& other) override;

    void new_network_copy();

private:
    double thrusters_active = 0;
};

template<typename T>
void for_each_component(ship& s, T& t)
{
    for(component& c : s.components)
    {
        t(c);
    }
}

template<typename C, typename T>
void for_each_ship_hackery(C& c, T t)
{
    for(ship& ns : c.stored)
    {
        for(component& st : ns.components)
        {
            t(st);
        }
    }
}

template<typename C, typename T>
void for_each_ship_hackery(const C& c, T t)
{
    for(const ship& ns : c.stored)
    {
        for(const component& st : ns.components)
        {
            t(st);
        }
    }
}

template<typename T>
void for_each_stored(component& c, T t)
{
    for(ship& ns : c.stored)
    {
        for(component& st : ns.components)
        {
            t(st);
        }
    }
}

template<typename T>
void for_each_stored(const component& c, T t)
{
    for(const ship& ns : c.stored)
    {
        for(const component& st : ns.components)
        {
            t(st);
        }
    }
}

struct projectile : heatable_entity
{
    projectile();

    void on_collide(entity_manager& em, entity& other) override;
    void tick(double dt_s) override;
};

struct asteroid : heatable_entity
{
    asteroid();
    void init(float min_rad, float max_rad);

    virtual void tick(double dt_s) override;
};

void tick_radar_data(entity_manager& entities, alt_radar_sample& sample, entity* ship_proxy);
void render_radar_data(sf::RenderWindow& window, const alt_radar_sample& sample);

#endif // SHIP_COMPONENTS_HPP_INCLUDED
