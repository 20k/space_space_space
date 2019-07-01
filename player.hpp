#ifndef PLAYER_HPP_INCLUDED
#define PLAYER_HPP_INCLUDED

#include "entity.hpp"
#include <optional>
#include <SFML/System/Clock.hpp>
#include "fixed_clock.hpp"
#include "design_editor.hpp"
#include "radar_field.hpp"
#include "playspace.hpp"
#include <networking/serialisable_fwd.hpp>

struct common_renderable;

struct auth_data : serialisable, free_function
{
    bool default_init = false;
};

struct system_descriptor : serialisable, free_function
{
    std::string name;
    vec2f position;
    size_t sys_pid = 0;
};

struct client_entities : serialisable, free_function
{
    std::vector<client_renderable> entities;

    void render(camera& cam, sf::RenderWindow& win);
    void render_layer(camera& cam, sf::RenderWindow& win, int layer);
};

struct client_fire : serialisable, free_function
{
    uint32_t weapon_offset = 0;
    float fire_angle = 0;
};

struct warp_info : serialisable, free_function
{
    size_t sys_pid = 0;
    bool should_warp = false;
};

struct poi_travel_info : serialisable, free_function
{
    size_t poi_pid = 0;
    bool should_travel = false;
};

struct client_room_object_data : serialisable, free_function
{
    std::string name;
    vec2f position;
    size_t pid = 0;
    std::string type;
};

struct pending_transfer;

struct client_input : serialisable, free_function
{
    vec2f direction = {0,0};
    float rotation = 0;
    std::vector<client_fire> fired;
    bool ping = false;
    vec2f mouse_world_pos = {0,0};
    //global_serialise_info rpcs;
    bool to_poi_space = false;
    bool to_fsd_space = false;
    warp_info warp;
    poi_travel_info travel;
    std::vector<client_room_object_data> room_objects;
    std::vector<pending_transfer> transfers;
    bool try_dock = false;
    size_t dock_to_ship = -1;
};

struct nearby_ship_info : serialisable, free_function
{
    std::string name;
    size_t ship_id = -1;
    std::vector<component> components;
};

#endif // PLAYER_HPP_INCLUDED
