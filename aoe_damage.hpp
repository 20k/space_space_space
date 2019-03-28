#ifndef AOE_DAMAGE_HPP_INCLUDED
#define AOE_DAMAGE_HPP_INCLUDED

#include "entity.hpp"

struct aoe_damage : entity
{
    float radius = 0;
    float max_radius = 0;

    virtual void tick(double dt_s) override;
};

#endif // AOE_DAMAGE_HPP_INCLUDED
