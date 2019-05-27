#include "camera.hpp"
#include <math.h>

float proj(float zoom)
{
    float val = pow(sqrt(2), zoom);

    return clamp(val, 1/256.f, 256.f);
}

float unproj(float zoom)
{
    return log(zoom) / log(sqrt(2));
}

camera::camera(vec2f _screen_size)
{
    screen_size = _screen_size;
    zoom = 1;
    //zoom = proj(1);
}

vec2f camera::world_to_screen(vec2f in)
{
    return ((in - position) * zoom).rot(rotation) + screen_size/2.f;
}

vec2f camera::world_to_screen(vec2f in, float z)
{
    return ((in - position) * zoom / z).rot(rotation) + screen_size/2.f;
}

vec2f camera::screen_to_world(vec2f in)
{
    return ((in - screen_size/2.f).rot(-rotation) / zoom) + position;
}

void camera::add_linear_zoom(float linear)
{
    float linear_zoom = unproj(zoom);

    linear_zoom += linear;

    zoom = proj(linear_zoom);
}

float camera::get_linear_zoom()
{
    return unproj(zoom);
}

bool camera::within_screen(vec2f point)
{
    if(point.x() < 0 || point.y() < 0 || point.x() >= screen_size.x() || point.y() >= screen_size.y())
        return false;

    return true;
}
