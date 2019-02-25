#ifndef PLAYER_HPP_INCLUDED
#define PLAYER_HPP_INCLUDED

#include "entity.hpp"
#include <optional>
#include <SFML/System/Clock.hpp>
#include "fixed_clock.hpp"

struct uncertain
{
    fixed_clock elapsed;
    fixed_clock received_signal;

    bool begin_cleanup = false;
    bool is_unknown = false;

    bool bad(vec2f player_pos, vec2f my_pos)
    {
        return elapsed.getElapsedTime().asMilliseconds() > 500 && begin_cleanup;

        //return elapsed.getElapsedTime().asSeconds() > 1 && (player_pos - my_pos).length() < 10;
    }

    void unknown(bool state)
    {
        is_unknown = state;
    }

    void no_signal()
    {
        if(received_signal.getElapsedTime().asMicroseconds() / 1000. < 1000)
            return;

        if(begin_cleanup)
            return;

        begin_cleanup = true;
        elapsed.restart();
    }

    void got_signal()
    {
        //is_unknown = false;

        if(!begin_cleanup)
            return;

        begin_cleanup = false;
        received_signal.restart();
    }
};

struct common_renderable : serialisable, uncertain
{
    ///0 = low detail, 1 = high
    int type = 0;

    client_renderable r;
    vec2f velocity = {0,0};

    virtual void serialise(nlohmann::json& data, bool encode) override
    {
        DO_SERIALISE(type);
        DO_SERIALISE(r);
        DO_SERIALISE(velocity);
        DO_SERIALISE(is_unknown);
    }
};

struct player_model : serialisable
{
    ///oh goodie raw pointers
    //entity* en = nullptr;
    uint32_t network_id = -1;

    std::map<uint32_t, common_renderable> renderables;

    void cleanup(vec2f my_pos);

    void tick(double dt_s);
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

    void tick(double dt_s)
    {
        for(auto& i : models)
        {
            i->tick(dt_s);
        }
    }
};

#endif // PLAYER_HPP_INCLUDED
