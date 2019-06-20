#ifndef SERIALISABLES_HPP_INCLUDED
#define SERIALISABLES_HPP_INCLUDED

#include <networking/serialisable_fwd.hpp>

struct component;
struct ship;

DECLARE_SERIALISE_FUNCTION(component);
DECLARE_SERIALISE_FUNCTION(ship);

#endif // SERIALISABLES_HPP_INCLUDED
