#include "ship_components.hpp"
#include <algorithm>
#include <assert.h>
#include <math.h>

ship::ship()
{
    resource_status.resize(component_info::COUNT);
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

        needed[d.type] += fabs(d.recharge) * dt_s * hp;
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
        res[d.type] += d.recharge * efficiency * dt_s * hp;
    }
}


void ship::tick(double dt_s)
{
    for(component& c : components)
    {
        double sat_percent = c.satisfied_percentage(dt_s, resource_status);

        c.apply(sat_percent, dt_s, resource_status);
    }
}
