#ifndef SHIP_COMPONENTS_HPP_INCLUDED
#define SHIP_COMPONENTS_HPP_INCLUDED

#include <vector>
#include <string>
#include <map>
#include <vec/vec.hpp>

#include "entity.hpp"
#include "networking/networking.hpp"
#include <memory>
#include "radar_field.hpp"

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
        COOLANT,
        CREW,
        POWER,
        CAPACITOR, ///weapons
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
        "Coolant",
        "Crew",
        "Power",
        "Capacitor",
    };
}

struct does : serialisable
{
    double capacity = 0;
    double held = 0;
    double recharge = 0;

    component_info::does_type type = component_info::COUNT;

    virtual void serialise(nlohmann::json& data, bool encode) override
    {
        DO_SERIALISE(capacity);
        DO_SERIALISE(held);
        DO_SERIALISE(recharge);
        DO_SERIALISE(type);
    }
};

struct component : serialisable
{
    std::vector<does> info;
    std::vector<does> activate_requirements;
    bool no_drain_on_full_production = false;

    std::string long_name;

    double last_sat = 1;

    ///how active i need to be
    ///only applies to no_drain_on_full_production
    ///has a minimum value to prevent accidental feedback loops
    double last_production_frac = 1;

    virtual void serialise(nlohmann::json& data, bool encode) override
    {
        DO_SERIALISE(info);
        DO_SERIALISE(activate_requirements);
        DO_SERIALISE(long_name);
        DO_SERIALISE(last_sat);
        DO_SERIALISE(no_drain_on_full_production);
        DO_SERIALISE(last_production_frac);
    }

    double satisfied_percentage(double dt_s, const std::vector<double>& res);
    void apply(const std::vector<double>& efficiency, double dt_s, std::vector<double>& res);

    void add(component_info::does_type, double amount);
    void add(component_info::does_type, double amount, double cap);

    void add_on_use(component_info::does_type, double amount);

    void set_no_drain_on_full_production();

    bool has(component_info::does_type type)
    {
        for(auto& i : info)
        {
            if(i.type == type)
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

    std::vector<double> get_needed();
    std::vector<double> get_capacity();
    std::vector<double> get_held();

    double get_sat(const std::vector<double>& sat);

    void deplete_me(std::vector<double>& diff);

    bool can_use(const std::vector<double>& res);
    void use(std::vector<double>& res);

    bool try_use = false;
};

struct data_tracker : serialisable
{
    delta_container<std::vector<float>> vsat;
    delta_container<std::vector<float>> vheld;
    int max_data = 500;

    void add(double sat, double held);

    virtual void serialise(nlohmann::json& data, bool encode) override
    {
        DO_SERIALISE(vsat);
        DO_SERIALISE(vheld);
        DO_SERIALISE(max_data);

        while((int)vsat.size() > max_data)
        {
            vsat.erase(vsat.begin());
        }

        while((int)vheld.size() > max_data)
        {
            vheld.erase(vheld.begin());
        }
    }
};

struct alt_radar_field;

struct ship : virtual entity, virtual serialisable
{
    size_t network_owner = 0;

    std::vector<component> components;

    std::vector<component_info::does_type> tracked
    {
        component_info::WARP,
        component_info::SHIELDS,
        component_info::ARMOUR,
        component_info::HP,
        component_info::COOLANT,
        component_info::POWER,
        component_info::CAPACITOR
    };

    //std::map<component_info::does_type, data_tracker> data_track;

    ///so we want a diff wrapper type
    //std::vector<data_tracker> data_track;

    std::vector<persistent<data_tracker>> data_track;

    ship();

    void handle_heat(double dt_s);
    void tick(double dt_s) override;
    void tick_pre_phys(double dt_s) override;
    //void render(sf::RenderWindow& win);

    void add(const component& c);

    std::vector<double> get_sat_percentage();
    std::vector<double> get_produced(double dt_s, const std::vector<double>& sat_in);

    std::vector<double> get_capacity();

    std::vector<double> last_sat_percentage;

    //std::string show_components();
    std::string show_resources();

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

        return ret;
    }

    void advanced_ship_display();

    void apply_force(vec2f dir);
    void apply_rotation_force(float force);

    void fire();
    void ping();
    void take_damage(double amount);

    double data_track_elapsed_s = 0;

    virtual void serialise(nlohmann::json& data, bool encode) override
    {
        DO_SERIALISE(data_track);
        DO_SERIALISE(network_owner);
        DO_SERIALISE(components);
        DO_SERIALISE(last_sat_percentage);
    }
};

struct projectile : entity
{
    projectile();

    void on_collide(entity_manager& em, entity& other) override;
};

template<typename T>
struct data_model : serialisable
{
    std::vector<T> ships;
    std::vector<client_renderable> renderables;
    alt_radar_sample sample;
    uint32_t client_network_id = 0;

    virtual void serialise(nlohmann::json& data, bool encode) override
    {
        DO_SERIALISE(ships);
        DO_SERIALISE(renderables);
        DO_SERIALISE(sample);
        DO_SERIALISE(client_network_id);
    }
};

struct asteroid : entity
{
    asteroid();

    virtual void tick(double dt_s) override;
};

void tick_radar_data(entity_manager& entities, alt_radar_sample& sample, entity* ship_proxy);
void render_radar_data(sf::RenderWindow& window, const alt_radar_sample& sample);

#endif // SHIP_COMPONENTS_HPP_INCLUDED
