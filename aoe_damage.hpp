#ifndef AOE_DAMAGE_HPP_INCLUDED
#define AOE_DAMAGE_HPP_INCLUDED

#include "entity.hpp"

struct aoe_damage : entity
{
    float radius = 0;
    float max_radius = 0;
    float damage = 0;

    uint32_t emitted_by = -1;

    bool emitted = false;

    double accumulated_time = 0;

    std::map<uint64_t, bool> collided_with;

    aoe_damage();

    virtual void tick(double dt_s) override;
    virtual void on_collide(entity_manager& em, entity& other) override;

    SERIALISE_SIGNATURE()
    {
        DO_SERIALISE(radius);
        DO_SERIALISE(max_radius);
        DO_SERIALISE(damage);
        DO_SERIALISE(accumulated_time);
    }
};

#endif // AOE_DAMAGE_HPP_INCLUDED
