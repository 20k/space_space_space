#ifndef SERIALISABLES_HPP_INCLUDED
#define SERIALISABLES_HPP_INCLUDED

#include <networking/serialisable_fwd.hpp>

struct component;
struct ship;
struct aoe_damage;
struct radar_object;
struct alt_radar_sample;
struct common_renderable;
struct build_in_progress;
struct player_research;
struct blueprint_node;
struct blueprint;
struct blueprint_manager;;

DECLARE_SERIALISE_FUNCTION(component);
DECLARE_SERIALISE_FUNCTION(ship);
DECLARE_SERIALISE_FUNCTION(aoe_damage);
DECLARE_SERIALISE_FUNCTION(radar_object);
DECLARE_SERIALISE_FUNCTION(alt_radar_sample);
DECLARE_SERIALISE_FUNCTION(common_renderable);
DECLARE_SERIALISE_FUNCTION(build_in_progress);
DECLARE_SERIALISE_FUNCTION(player_research);
DECLARE_SERIALISE_FUNCTION(blueprint_node);
DECLARE_SERIALISE_FUNCTION(blueprint);
DECLARE_SERIALISE_FUNCTION(blueprint_manager);

#endif // SERIALISABLES_HPP_INCLUDED
