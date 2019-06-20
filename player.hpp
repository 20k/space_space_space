#ifndef PLAYER_HPP_INCLUDED
#define PLAYER_HPP_INCLUDED

#include "entity.hpp"
#include <optional>
#include <SFML/System/Clock.hpp>
#include "fixed_clock.hpp"
#include "design_editor.hpp"
#include "radar_field.hpp"
#include "playspace.hpp"
#include <networking/serialisable.hpp>

struct common_renderable;

///i think the reason why the code is a mess is this inversion of ownership
///this needs to own everything about a player, ships etc
struct player_model : serialisable, owned, free_function
{
    ///oh goodie raw pointers
    //entity* en = nullptr;
    uint32_t network_id = -1;

    std::map<uint32_t, common_renderable> renderables;

    entity* controlled_ship = nullptr;
    //player_research research;
    //blueprint_manager blueprint_manage;

    void cleanup(vec2f my_pos);

    void tick(double dt_s);
};

/*struct player_model_manager
{
    std::vector<player_model*> models;

    player_model* make_new(uint32_t network_id)
    {
        player_model* m = new player_model;
        m->network_id = network_id;

        models.push_back(m);

        return m;
    }

    std::optional<player_model*> fetch_by_network_id(uint32_t network_id)
    {
        for(player_model* m : models)
        {
            if(m->network_id == network_id)
                return m;
        }

        return std::nullopt;
    }

    void tick(double dt_s)
    {
        for(auto& i : models)
        {
            i->tick(dt_s);
        }
    }
};*/

#define DB_USER_ID 1

struct persistent_user_data : serialisable, db_storable<persistent_user_data>, free_function
{
    player_research research;
    blueprint_manager blueprint_manage;
};

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

template<typename T>
struct data_model : serialisable
{
    std::vector<T> ships;
    std::vector<client_renderable> renderables;
    std::vector<client_poi_data> labels;
    alt_radar_sample sample;
    uint32_t client_network_id = 0;
    player_model networked_model;
    persistent_user_data persistent_data;
    size_t controlled_ship_id = -1;
    std::vector<system_descriptor> connected_systems;
    vec2f room_position;

    SERIALISE_SIGNATURE(data_model<T>)
    {
        DO_SERIALISE(ships);
        DO_SERIALISE(renderables);
        DO_SERIALISE(labels);
        DO_SERIALISE(sample);
        DO_SERIALISE(client_network_id);
        DO_SERIALISE(networked_model);
        DO_SERIALISE(persistent_data);
        DO_SERIALISE(controlled_ship_id);
        DO_SERIALISE(connected_systems);
        DO_SERIALISE(room_position);
    }
};

template<typename T>
struct data_model_manager
{
    std::map<uint64_t, data_model<T>> data;
    std::map<uint64_t, data_model<T>> backup;

    data_model<T>& fetch_by_id(uint64_t id)
    {
        backup[id];

        return data[id];
    }
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

struct client_input : serialisable, free_function
{
    vec2f direction = {0,0};
    float rotation = 0;
    std::vector<client_fire> fired;
    bool ping = false;
    vec2f mouse_world_pos = {0,0};
    global_serialise_info rpcs;
    bool to_poi_space = false;
    bool to_fsd_space = false;
    warp_info warp;
    poi_travel_info travel;
    std::vector<client_room_object_data> room_objects;
};

#endif // PLAYER_HPP_INCLUDED
