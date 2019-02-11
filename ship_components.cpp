#include "ship_components.hpp"
#include <algorithm>
#include <assert.h>
#include <math.h>
#include "format.hpp"
#include <iostream>

ship::ship()
{
    resource_status.resize(component_info::COUNT);
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

    info.push_back(d);
}

double component::satisfied_percentage(double dt_s, const std::vector<double>& res)
{
    assert(res.size() == component_info::COUNT);

    std::vector<double> needed;
    needed.resize(component_info::COUNT);

    for(does& d : info)
    {
        if(d.recharge >= 0)
            continue;

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
    }

    return satisfied;
}

void component::apply(double efficiency, double dt_s, std::vector<double>& res)
{
    assert(res.size() == component_info::COUNT);

    for(does& d : info)
    {
        res[d.type] += d.recharge * efficiency * dt_s * (hp / max_hp);
    }
}

std::vector<double> component::get_needed()
{
    std::vector<double> needed;
    needed.resize(component_info::COUNT);

    for(does& d : info)
    {
        needed[d.type] += d.recharge * (hp / max_hp);
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

void ship::tick(double dt_s)
{
    for(component& c : components)
    {
        double sat_percent = c.satisfied_percentage(dt_s, resource_status);

        c.apply(sat_percent, dt_s, resource_status);
    }
}

void ship::add(const component& c)
{
    components.push_back(c);
}

std::string ship::show_components()
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
}

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
    caps.push_back("Capacity");

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

    for(auto& i : resource_status)
    {
        cur.push_back(to_string_with(i, 1));
    }

    std::string ret;

    for(int i=0; i < (int)strings.size(); i++)
    {
        ret += format(strings[i], strings) + ": " + format(status[i], status) + " | " + format(cur[i], cur) + "/" + format(caps[i], caps) + "\n";
    }

    return ret;
}
