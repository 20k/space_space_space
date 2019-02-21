#include "camera.hpp"

vec2f camera::world_to_screen(vec2f in)
{
    return ((in - position) * zoom).rot(rotation);
}

vec2f camera::screen_to_world(vec2f in)
{
    return (in.rot(-rotation) / zoom) + position;
}
