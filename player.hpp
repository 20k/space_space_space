#ifndef PLAYER_HPP_INCLUDED
#define PLAYER_HPP_INCLUDED

#include "entity.hpp"
#include <optional>

struct uncertain
{
    sf::Clock elapsed;

    bool bad(vec2f player_pos, vec2f my_pos)
    {
        return elapsed.getElapsedTime().asSeconds() > 1 && (player_pos - my_pos).length() < 10;
    }
};

struct uncertain_renderable : serialisable, uncertain
{
    vec2f position = {0,0};
    float radius = 0;

    virtual void serialise(nlohmann::json& data, bool encode)
    {
        DO_SERIALISE(position);
        DO_SERIALISE(radius);
    }
};

struct detailed_renderable : client_renderable, uncertain
{
    detailed_renderable(){}
    detailed_renderable(const client_renderable& in) : client_renderable(in){}
};

struct player_model : serialisable
{
    ///oh goodie raw pointers
    //entity* en = nullptr;
    uint32_t network_id = -1;

    std::map<uint32_t, detailed_renderable> accumulated_renderables;
    std::map<uint32_t, uncertain_renderable> uncertain_renderables;

    void cleanup(vec2f my_pos)
    {
        for(auto it = uncertain_renderables.begin(); it != uncertain_renderables.end();)
        {
            if(it->second.bad(my_pos, it->second.position))
                it = uncertain_renderables.erase(it);
            else
                it++;
        }

        for(auto it = accumulated_renderables.begin(); it != accumulated_renderables.end();)
        {
            if(it->second.bad(my_pos, it->second.position))
                it = accumulated_renderables.erase(it);
            else
                it++;
        }
    }
};

struct player_model_manager
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
};

#endif // PLAYER_HPP_INCLUDED
