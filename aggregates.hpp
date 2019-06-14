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

inline
float left_side_from_origin(const vec<2, float>& l2, const vec<2, float>& lp)
{
    return l2.v[0] * lp.v[1] - l2.v[1] * lp.v[0];
}

template<typename T>
struct aggregate
{
    std::vector<T> data;
    vec2f pos;
    //vec2f dim;
    vec2f half_dim;

    vec2f tl, tr, bl, br;

    vec2f get_pos() const
    {
        return pos;
    }

    vec2f get_dim() const
    {
        return half_dim * 2;
    }

    void calc_pos_half_dim()
    {
        vec2f fmin = {FLT_MAX, FLT_MAX};
        vec2f fmax = {-FLT_MAX, -FLT_MAX};

        if(data.size() == 0)
            return;

        for(auto& i : data)
        {
            vec2f lpos = tget_pos(i);
            vec2f ldim = tget_dim(i);

            fmin = min(fmin, lpos - ldim/2.f);
            fmax = max(fmax, lpos + ldim/2.f);
        }

        pos = (fmax + fmin) / 2.f;
        half_dim = (fmax - fmin)/2.f;
    }

    void complete()
    {
        calc_pos_half_dim();

        tl = pos - half_dim;
        tr = pos + (vec2f){half_dim.x(), -half_dim.y()};
        bl = pos + (vec2f){-half_dim.x(), half_dim.y()};
        br = pos + half_dim;
    }

    void complete_with_padding(float pad)
    {
        calc_pos_half_dim();
        half_dim += (vec2f){pad/2, pad/2};

        tl = pos - half_dim;
        tr = pos + (vec2f){half_dim.x(), -half_dim.y()};
        bl = pos + (vec2f){-half_dim.x(), half_dim.y()};
        br = pos + half_dim;
    }

    void recalculate_bounds()
    {
        tl = pos - half_dim;
        tr = pos + (vec2f){half_dim.x(), -half_dim.y()};
        bl = pos + (vec2f){-half_dim.x(), half_dim.y()};
        br = pos + half_dim;
    }

    bool intersects(vec2f in_pos, float current_radius, float next_radius, vec2f start_dir, float restrict_angle, vec2f left_restrict, vec2f right_restrict)
    {
        ///so, if we hit the doughnut AND we fully lie within the unoccluded zone, hit

        ///see rect_intersects_doughnut
        vec2f rtl = tl - in_pos;
        vec2f rtr = tr - in_pos;
        vec2f rbl = bl - in_pos;
        vec2f rbr = br - in_pos;

        float rad_sq = current_radius * current_radius;

        if(rtl.squared_length() < rad_sq &&
           rtr.squared_length() < rad_sq &&
           rbl.squared_length() < rad_sq &&
           rbr.squared_length() < rad_sq)
            return false;

        vec2f clpos = clamp(in_pos, tl, br);
        vec2f dist = in_pos - clpos;

        if(dist.squared_length() >= next_radius * next_radius)
            return false;

        if(restrict_angle >= M_PI/4)
            return true;

        //printf("restrict %f\n", restrict_angle);

        //std::cout << "cres " << cos(restrict_angle) << std::endl;

        //std::cout << "dt " << dot(rtl.norm(), start_dir) << std::endl;

        /*std::cout << "in? " << angle_lies_between_vectors_cos(rtl.norm(), start_dir, cos(restrict_angle)) << " " <<
                               angle_lies_between_vectors_cos(rtr.norm(), start_dir, cos(restrict_angle)) << " " <<
                               angle_lies_between_vectors_cos(rbl.norm(), start_dir, cos(restrict_angle)) << " " <<
                               angle_lies_between_vectors_cos(rbr.norm(), start_dir, cos(restrict_angle)) << "\n";*/

        //return true;

        /*bool all_in = angle_lies_between_vectors_cos(rtl.norm(), start_dir, cos(restrict_angle)) &&
                      angle_lies_between_vectors_cos(rtr.norm(), start_dir, cos(restrict_angle)) &&
                      angle_lies_between_vectors_cos(rbl.norm(), start_dir, cos(restrict_angle)) &&
                      angle_lies_between_vectors_cos(rbr.norm(), start_dir, cos(restrict_angle));*/

        ///oh ok, so the problem with this is that it might overlap
        /*bool all_out = angle_between_vectors(rtl.norm(), start_dir) > restrict_angle &&
                        angle_between_vectors(rtr.norm(), start_dir) > restrict_angle &&
                        angle_between_vectors(rbl.norm(), start_dir) > restrict_angle &&
                        angle_between_vectors(rbr.norm(), start_dir) > restrict_angle;

        ///check if we lie in the occluded zone
        return !all_out;*/

        //return true;

        if(left_side_from_origin(left_restrict, rtl) <= 0 &&
           left_side_from_origin(left_restrict, rtr) <= 0 &&
           left_side_from_origin(left_restrict, rbl) <= 0 &&
           left_side_from_origin(left_restrict, rbr) <= 0)
            return false;

        if(left_side_from_origin(right_restrict, rtl) > 0 &&
           left_side_from_origin(right_restrict, rtr) > 0 &&
           left_side_from_origin(right_restrict, rbl) > 0 &&
           left_side_from_origin(right_restrict, rbr) > 0)
            return false;

        return true;
    }

    bool intersects(const aggregate<T>& agg)
    {
        return rect_intersect(tl, br, agg.tl, agg.br);
    }

    bool intersects_with_bound(const aggregate<T>& agg, float bound)
    {
        vec2f nhalf_dim = half_dim + (vec2f){bound, bound};

        vec2f ntl = pos - nhalf_dim;
        vec2f nbr = pos + nhalf_dim;

        return rect_intersect(ntl, nbr, agg.tl, agg.br);
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
