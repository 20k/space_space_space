#ifndef SHIP_COMPONENTS_HPP_INCLUDED
#define SHIP_COMPONENTS_HPP_INCLUDED

#include <vector>
#include <string>

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
        POWER,
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
        "Power",
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

    std::string long_name;

    double satisfied_percentage(double dt_s, const std::vector<double>& res);
    void apply(double efficiency, double dt_s, std::vector<double>& res);

    void add(component_info::does_type, double amount);
    void add(component_info::does_type, double amount, double cap);

    std::vector<double> get_needed();
    std::vector<double> get_capacity();
    std::vector<double> get_held();

    void deplete_me(std::vector<double>& diff);
};

struct ship
{
    std::vector<component> components;

    ship();

    void tick(double dt_s);

    void add(const component& c);

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
};

#endif // SHIP_COMPONENTS_HPP_INCLUDED