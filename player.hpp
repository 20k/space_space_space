#ifndef PLAYER_HPP_INCLUDED
#define PLAYER_HPP_INCLUDED

#include "entity.hpp"
#include <optional>
#include <SFML/System/Clock.hpp>
#include "fixed_clock.hpp"
#include "design_editor.hpp"
#include "radar_field.hpp"

struct common_renderable;

///i think the reason why the code is a mess is this inversion of ownership
///this needs to own everything about a player, ships etc
struct player_model : serialisable, owned
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

    SERIALISE_SIGNATURE()
    {
        //DO_SERIALISE(research);
        //DO_SERIALISE(blueprint_manage);
    }
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

struct persistent_user_data : serialisable, db_storable<persistent_user_data>
{
    player_research research;
    blueprint_manager blueprint_manage;
    bool default_init = false;

    SERIALISE_SIGNATURE()
    {
        DO_SERIALISE(research);
        DO_SERIALISE(blueprint_manage);
        DO_SERIALISE(default_init);
    }
};

struct auth_data : serialisable
{
    bool default_init = false;

    SERIALISE_SIGNATURE()
    {
        DO_SERIALISE(default_init);
    }
};

template<typename T>
struct data_model : serialisable
{
    std::vector<T> ships;
    std::vector<client_renderable> renderables;
    alt_radar_sample sample;
    uint32_t client_network_id = 0;
    player_model networked_model;
    persistent_user_data persistent_data;

    SERIALISE_SIGNATURE()
    {
        DO_SERIALISE(ships);
        DO_SERIALISE(renderables);
        DO_SERIALISE(sample);
        DO_SERIALISE(client_network_id);
        DO_SERIALISE(networked_model);
        DO_SERIALISE(persistent_data);
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

struct client_entities : serialisable
{
    std::vector<client_renderable> entities;

    void render(camera& cam, sf::RenderWindow& win);

    SERIALISE_SIGNATURE()
    {
        DO_SERIALISE(entities);
    }
};

struct client_fire : serialisable
{
    uint32_t weapon_offset = 0;
    float fire_angle = 0;

    SERIALISE_SIGNATURE()
    {
        DO_SERIALISE(weapon_offset);
        DO_SERIALISE(fire_angle);
    }
};

struct client_input : serialisable
{
    vec2f direction = {0,0};
    float rotation = 0;
    std::vector<client_fire> fired;
    bool ping = false;
    vec2f mouse_world_pos = {0,0};
    global_serialise_info rpcs;

    SERIALISE_SIGNATURE()
    {
        DO_SERIALISE(direction);
        DO_SERIALISE(rotation);
        DO_SERIALISE(fired);
        DO_SERIALISE(ping);
        DO_SERIALISE(mouse_world_pos);
        DO_SERIALISE(rpcs);
    }
};

#endif // PLAYER_HPP_INCLUDED
