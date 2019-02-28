#ifndef AGGREGATES_HPP_INCLUDED
#define AGGREGATES_HPP_INCLUDED

#include <vec/vec.hpp>
#include <vector>

template<typename T>
struct aggregate
{
    std::vector<T> data;
    vec2f pos;
    vec2f dim;

    vec2f calc_avg()
    {
        vec2f fmin = {FLT_MAX, FLT_MAX};
        vec2f fmax = {-FLT_MAX, -FLT_MAX};

        if(data.size() == 0)
            return {0,0};

        for(auto& i : data)
        {
            fmin = min(fmin, i.pos - i.dim);
            fmax = max(fmax, i.pos + i.dim);
        }

        return ((fmax + fmin) / 2.f);
    }

    vec2f calc_half_dim()
    {
        vec2f fmin = {FLT_MAX, FLT_MAX};
        vec2f fmax = {-FLT_MAX, -FLT_MAX};

        if(data.size() == 0)
            return {0,0};

        for(auto& i : data)
        {
            fmin = min(fmin, i.pos - i.dim);
            fmax = max(fmax, i.pos + i.dim);
        }

        return ((fmax - fmin) / 2.f);
    }
};

template<typename T>
struct all_aggregate
{
    std::vector<aggregate<T>> data;
};

template<typename T>
all_aggregate<T> collect_aggregates(const std::vector<T>& in, int num_groups)
{
    all_aggregate<T> ret;

    if(in.size() == 0)
        return ret;

    std::vector<int> used;
    used.resize(in.size());

    int num_per_group = ceilf((float)in.size() / num_groups);

    alt_aggregate_collideables next;
    next.collide.reserve(num_per_group+1);

    for(int ng=0; ng < num_groups; ng++)
    {
        int start_elem = 0;
        for(int ielem = 0; ielem < (int)in.size(); ielem++)
        {
            if(used[ielem])
                continue;

            used[ielem] = 1;
            start_elem = ielem;

            next.collide.push_back(in[ielem]);
            break;
        }

        vec2f tavg = next.calc_avg();

        std::vector<std::pair<float, int>> nearest_elems;
        nearest_elems.reserve(in.size());

        for(int ielem = start_elem; ielem < (int)in.size(); ielem++)
        {
            if(used[ielem])
                continue;

            ///MANHATTEN DISTANCE
            float next_man = (in[ielem].pos - tavg).sum_absolute();

            nearest_elems.push_back({next_man, ielem});
        }

        auto it = nearest_elems.begin();

        for(int max_count = 0; it != nearest_elems.end() && max_count < num_per_group; it++, max_count++)
        {

        }

        std::nth_element(nearest_elems.begin(), it, nearest_elems.end(),
          [](const auto& i1, const auto& i2)
          {
              return i1.first < i2.first;
          });

        for(int i=0; i < (int)nearest_elems.size() && i < num_per_group; i++)
        {
            next.collide.push_back(in[nearest_elems[i].second]);
            used[nearest_elems[i].second] = 1;
        }

        if(nearest_elems.size() == 0)
            break;

        next.pos = next.calc_avg();
        next.half_dim = next.calc_half_dim();

        ret.aggregate.push_back(next);
        next = alt_aggregate_collideables();
        next.collide.reserve(num_per_group+1);
    }

    for(auto& i : used)
    {
        assert(i);
    }

    return ret;
}

#endif // AGGREGATES_HPP_INCLUDED
