#ifndef SERIALISABLES_HPP_INCLUDED
#define SERIALISABLES_HPP_INCLUDED

#include <networking/serialisable_fwd.hpp>

struct component;
struct ship;
struct aoe_damage;

DECLARE_SERIALISE_FUNCTION(component);
DECLARE_SERIALISE_FUNCTION(ship);
DECLARE_SERIALISE_FUNCTION(aoe_damage);

#endif // SERIALISABLES_HPP_INCLUDED
