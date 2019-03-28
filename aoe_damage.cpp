#include "aoe_damage.hpp"
#include "ship_components.hpp"

aoe_damage::aoe_damage()
{
    collides = true;
    is_collided_with = false;

    //r.init_rectangular({10, 10});
}

void aoe_damage::tick(double dt_s)
{
    double my_rad = accumulated_time * max_radius;

    my_rad = clamp(my_rad, 0, max_radius);

    r.init_rectangular({my_rad, my_rad});

    accumulated_time += dt_s;

    if(accumulated_time > 1)
        cleanup = true;
}

void aoe_damage::on_collide(entity_manager& em, entity& other)
{
    if(collided_with[other.id])
        return;

    collided_with[other.id] = true;

    ship* s = dynamic_cast<ship*>(&other);

    if(s == nullptr)
        return;

    s->take_damage(damage);
}
