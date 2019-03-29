#include "aoe_damage.hpp"
#include "ship_components.hpp"
#include "radar_field.hpp"

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

    //r.init_rectangular({my_rad, my_rad});

    r.init_xagonal(my_rad, 6);
    r.transient = true;

    accumulated_time += dt_s;

    if(accumulated_time > 1)
        cleanup = true;

    if(!emitted)
    {
        alt_radar_field& radar = get_radar_field();

        alt_frequency_packet alt_pack;
        alt_pack.intensity = damage * 100;
        alt_pack.frequency = HEAT_FREQ;

        radar.ignore_map[alt_frequency_packet::gid][emitted_by].restart();

        radar.emit_raw(alt_pack, r.position, id, r);

        emitted = true;
    }
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
