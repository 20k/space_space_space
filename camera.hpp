#ifndef CAMERA_HPP_INCLUDED
#define CAMERA_HPP_INCLUDED

#include <vec/vec.hpp>

struct camera
{
    vec2f position = {0,0};
    float zoom = 1;
    float rotation = 0;

    vec2f world_to_screen(vec2f in);
    vec2f screen_to_world(vec2f in);
};

#endif // CAMERA_HPP_INCLUDED
