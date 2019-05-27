#ifndef CAMERA_HPP_INCLUDED
#define CAMERA_HPP_INCLUDED

#include <vec/vec.hpp>

struct camera
{
    vec2f position = {0,0};
    vec2f screen_size = {400, 400};
    float zoom = 1;
    float rotation = 0;

    camera(vec2f _screen_size);

    vec2f world_to_screen(vec2f in);
    vec2f world_to_screen(vec2f in, float z);
    vec2f screen_to_world(vec2f in);

    bool within_screen(vec2f screen_position);

    void add_linear_zoom(float linear);
    float get_linear_zoom();
};

#endif // CAMERA_HPP_INCLUDED
