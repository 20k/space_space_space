#ifndef ENTITY_HPP_INCLUDED
#define ENTITY_HPP_INCLUDED

#include <vec/vec.hpp>

namespace sf
{
    struct RenderWindow;
}

struct entity
{
    vec2f position = {0,0};
    vec2f velocity = {0,0};
    vec2f control_force = {0,0};

    float rotation = 0;
    float angular_velocity = 0;
    float control_angular_force = 0;

    std::vector<float> vert_dist;
    std::vector<float> vert_angle;
    std::vector<vec3f> vert_cols;

    void init_rectangular(vec2f dim);
    void render(sf::RenderWindow& window);

    float approx_rad = 0;

    bool drag = false;

    void tick_phys(double dt_s);
    void apply_inputs(double dt_s, double velocity_mult, double angular_mult);

    double angular_drag = M_PI/8;
    double velocity_drag = angular_drag * 20;

    float scale = 2;

    vec2f get_world_pos(int vertex_id);
    bool point_within(vec2f point);
};

#endif // ENTITY_HPP_INCLUDED
