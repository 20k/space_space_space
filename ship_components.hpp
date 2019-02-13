#ifndef SHIP_COMPONENTS_HPP_INCLUDED
#define SHIP_COMPONENTS_HPP_INCLUDED

#include <vector>
#include <string>
#include <map>
#include <vec/vec.hpp>

#include "entity.hpp"

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

struct does
{
    double capacity = 0;
    double held = 0;
    double recharge = 0;

    component_info::does_type type = component_info::COUNT;
};

struct component
{
    std::vector<does> info;
    std::vector<does> activate_requirements;

    std::string long_name;

    double last_sat = 1;

    double satisfied_percentage(double dt_s, const std::vector<double>& res);
    void apply(const std::vector<double>& efficiency, double dt_s, std::vector<double>& res);

    void add(component_info::does_type, double amount);
    void add(component_info::does_type, double amount, double cap);

    void add_on_use(component_info::does_type, double amount);

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

struct data_tracker
{
    std::vector<float> vsat;
    std::vector<float> vheld;
    int max_data = 500;

    void add(double sat, double held);
};

struct ship : entity
{
    size_t network_owner = 0;

    std::vector<component> components;

    std::map<component_info::does_type, data_tracker> data_track;

    ship();

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
    void take_damage(double amount);

    double data_track_elapsed_s = 0;
};

struct projectile : entity
{
    projectile();

    void on_collide(entity_manager& em, entity& other) override;
};

#endif // SHIP_COMPONENTS_HPP_INCLUDED
