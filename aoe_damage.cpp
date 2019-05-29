#include "aoe_damage.hpp"
#include "ship_components.hpp"
#include "radar_field.hpp"

aoe_damage::aoe_damage(std::shared_ptr<alt_radar_field> _field)
{
    collides = true;
    is_collided_with = false;
    field = _field;

    //r.init_rectangular({10, 10});
}

void aoe_damage::tick(double dt_s)
{
    double my_rad = accumulated_time * max_radius;

    my_rad = clamp(my_rad, 0, max_radius);
    radius = my_rad;

    //r.init_rectangular({my_rad, my_rad});

    r.init_xagonal(my_rad, 6);
    r.transient = true;

    accumulated_time += dt_s;

    if(accumulated_time > 1)
        cleanup = true;

    if(!emitted)
    {
        alt_radar_field& radar = *field;

        alt_frequency_packet alt_pack;
        alt_pack.intensity = damage * 100;
        alt_pack.frequency = HEAT_FREQ;

        std::optional<entity*> en = parent->fetch(emitted_by);

        if(en && dynamic_cast<heatable_entity*>(en.value()))
        {
            heatable_entity* hen = (heatable_entity*)en.value();
            radar.ignore(alt_frequency_packet::gid, *hen);
        }

        radar.emit_raw(alt_pack, r.position, _pid, r);

        emitted = true;
    }
}

void aoe_damage::on_collide(entity_manager& em, entity& other)
{
    if(collided_with[other._pid])
        return;

    collided_with[other._pid] = true;

    ship* s = dynamic_cast<ship*>(&other);

    if(s == nullptr)
        return;

    float rad = radius;

    if(rad < 1)
        rad = 1;

    float final_damage = mix(damage, 0, radius / max_radius);

    s->take_damage(final_damage);
}
