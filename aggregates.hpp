#ifndef AGGREGATES_HPP_INCLUDED
#define AGGREGATES_HPP_INCLUDED

#include <vec/vec.hpp>
#include <vector>
//#include <SFML/System/Clock.hpp>

template<typename T>
vec2f tget_pos(T& in)
{
    return in.get_pos();
}

template<typename T>
vec2f tget_dim(T& in)
{
    return in.get_dim();
}

template<typename T>
vec2f tget_pos(T* in)
{
    return in->get_pos();
}

template<typename T>
vec2f tget_dim(T* in)
{
    return in->get_dim();
}

template<typename T>
struct aggregate
{
    std::vector<T> data;
    vec2f pos;
    //vec2f dim;
    vec2f half_dim;

    vec2f get_pos() const
    {
        return pos;
    }

    vec2f get_dim() const
    {
        return half_dim * 2;
    }

    vec2f calc_avg()
    {
        vec2f fmin = {FLT_MAX, FLT_MAX};
        vec2f fmax = {-FLT_MAX, -FLT_MAX};

        if(data.size() == 0)
            return {0,0};

        for(auto& i : data)
        {
            fmin = min(fmin, tget_pos(i) - tget_dim(i)/2.f);
            fmax = max(fmax, tget_pos(i) + tget_dim(i)/2.f);
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
            fmin = min(fmin, tget_pos(i) - tget_dim(i)/2.f);
            fmax = max(fmax, tget_pos(i) + tget_dim(i)/2.f);
        }

        return ((fmax - fmin) / 2.f);
    }

    void complete()
    {
        pos = calc_avg();
        half_dim = calc_half_dim();
    }

    bool intersects(vec2f in_pos, float current_radius, float next_radius, vec2f start_dir, float cos_half_restrict_angle, float restrict_angle)
    {
        bool hit_doughnut = rect_intersects_doughnut(pos, half_dim, in_pos, current_radius, next_radius);

        ///so, if we hit the doughnut AND we fully lie within the unoccluded zone, hit

        if(hit_doughnut)
        {
            vec2f tl = pos - half_dim;
            vec2f tr = pos + (vec2f){half_dim.x(), -half_dim.y()};
            vec2f bl = pos + (vec2f){-half_dim.x(), half_dim.y()};
            vec2f br = pos + half_dim;

            vec2f rtl = tl - in_pos;
            vec2f rtr = tr - in_pos;
            vec2f rbl = bl - in_pos;
            vec2f rbr = br - in_pos;

            /*bool lies_within = angle_lies_between_vectors_cos(rtl.norm(), start_dir, cos_half_restrict_angle) &&
                               angle_lies_between_vectors_cos(rtr.norm(), start_dir, cos_half_restrict_angle) &&
                               angle_lies_between_vectors_cos(rbl.norm(), start_dir, cos_half_restrict_angle) &&
                               angle_lies_between_vectors_cos(rbr.norm(), start_dir, cos_half_restrict_angle);*/

            return !angle_lies_between_vectors_cos(rtl.norm(), start_dir, cos_half_restrict_angle) ||
                    !angle_lies_between_vectors_cos(rtr.norm(), start_dir, cos_half_restrict_angle) ||
                    !angle_lies_between_vectors_cos(rbl.norm(), start_dir, cos_half_restrict_angle) ||
                    !angle_lies_between_vectors_cos(rbr.norm(), start_dir, cos_half_restrict_angle);

            /*vec2f v1 = start_dir.rot(-restrict_angle);
            vec2f v2 = start_dir.rot(restrict_angle);

            float c_1 = dot(rtl.norm(), {1, 0});
            float c_2 = dot(rtr.norm(), {1, 0});
            float c_3 = dot(rbl.norm(), {1, 0});
            float c_4 = dot(rbr.norm(), {1, 0});

            bool lies_within = angle_lies_between_vectors_cos(v1, v2, c_1) &&
                                      angle_lies_between_vectors_cos(v1, v2, c_2) &&
                                      angle_lies_between_vectors_cos(v1, v2, c_3) &&
                                      angle_lies_between_vectors_cos(v1, v2, c_4);*/


            /*float nc_1 = dot(rtl.norm(), start_dir);
            float nc_2 = dot(rtr.norm(), start_dir);
            float nc_3 = dot(rbl.norm(), start_dir);
            float nc_4 = dot(rbr.norm(), start_dir);

            bool lies_within = nc_1 >= cos_restrict_angle &&
                               nc_2 >= cos_restrict_angle &&
                               nc_3 >= cos_restrict_angle &&
                               nc_4 >= cos_restrict_angle;*/

            /*bool lies_within = angle_between_vectors(rtl, start_dir) < restrict_angle/2 &&
                                angle_between_vectors(rtr, start_dir) < restrict_angle/2 &&
                                angle_between_vectors(rbl, start_dir) < restrict_angle/2 &&
                                angle_between_vectors(rbr, start_dir) < restrict_angle/2;

            //assert(lies_within == assume_lies_within);

            return !lies_within;*/

            //return !lies_within;
        }

        return hit_doughnut;
    }

    bool intersects(const aggregate<T>& agg)
    {
        return rect_intersect(pos - half_dim, pos + half_dim, agg.pos - agg.half_dim, agg.pos + agg.half_dim);
    }
};

template<typename T>
using all_aggregates = aggregate<aggregate<T>>;

template<typename T>
all_aggregates<T> collect_aggregates(const std::vector<T>& in, int num_groups)
{
    //sf::Clock agg_1;

    aggregate<aggregate<T>> ret;

    if(in.size() == 0)
        return ret;

    if(num_groups <= 0)
        num_groups = 1;

    int num_per_group = ceilf((float)in.size() / num_groups);

    #if 1
    int idx = 0;

    for(; idx < (int)in.size() && idx < num_groups; idx++)
    {
        ret.data.emplace_back();
    }

    for(int i=0; i < idx; i++)
    {
        ret.data[i].data.reserve(num_per_group);
        ret.data[i].data.push_back(in[i]);
    }

    int real_groups = idx;

    if(real_groups == 0)
        return ret;

    for(; idx < (int)in.size(); idx++)
    {
        vec2f my_pos = tget_pos(in[idx]);

        float min_dist = FLT_MAX;
        int nearest_group = -1;

        for(int ng=0; ng < real_groups; ng++)
        {
            //if(ret.data[ng].data.size() >= num_per_group)
            //    continue;

            vec2f their_start = tget_pos(ret.data[ng].data[0]);

            float man_dist = (my_pos - their_start).sum_absolute();

            if(man_dist < min_dist)
            {
                min_dist = man_dist;
                nearest_group = ng;
            }
        }

        ret.data[nearest_group].data.push_back(in[idx]);
    }

    for(auto& i : ret.data)
    {
        i.complete();
    }

    #else
    std::vector<int> used;
    used.resize(in.size());

    aggregate<T> next;
    next.data.reserve(num_per_group+1);

    for(int ng=0; ng < num_groups; ng++)
    {
        int start_elem = 0;
        for(int ielem = 0; ielem < (int)in.size(); ielem++)
        {
            if(used[ielem])
                continue;

            used[ielem] = 1;
            start_elem = ielem;

            next.data.push_back(in[ielem]);
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
            float next_man = (tget_pos(in[ielem]) - tavg).sum_absolute();

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
            next.data.push_back(in[nearest_elems[i].second]);
            used[nearest_elems[i].second] = 1;
        }

        if(next.data.size() == 0)
            break;

        next.pos = next.calc_avg();
        next.half_dim = next.calc_half_dim();

        ret.data.push_back(next);
        next = decltype(next)();
        next.data.reserve(num_per_group+1);
    }

    for(auto& i : used)
    {
        assert(i);
    }
    #endif // 0

    ret.complete();

    //double tim = agg_1.getElapsedTime().asMicroseconds() / 1000.;

    //std::cout << "agg_1 " << tim << std::endl;

    return ret;
}

#endif // AGGREGATES_HPP_INCLUDED
