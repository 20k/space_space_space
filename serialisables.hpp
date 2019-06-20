#ifndef SERIALISABLES_HPP_INCLUDED
#define SERIALISABLES_HPP_INCLUDED

#include <networking/serialisable_fwd.hpp>

struct component;
struct ship;
struct aoe_damage;
struct radar_object;
struct alt_radar_sample;
struct common_renderable

DECLARE_SERIALISE_FUNCTION(component);
DECLARE_SERIALISE_FUNCTION(ship);
DECLARE_SERIALISE_FUNCTION(aoe_damage);
DECLARE_SERIALISE_FUNCTION(radar_object);
DECLARE_SERIALISE_FUNCTION(alt_radar_sample);
DECLARE_SERIALISE_FUNCTION(common_renderable);

#endif // SERIALISABLES_HPP_INCLUDED
