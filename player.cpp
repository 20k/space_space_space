#include "player.hpp"

void client_entities::render(camera& cam, sf::RenderWindow& win)
{
    for(client_renderable& i : entities)
    {
        i.render(cam, win);
    }
}

void client_entities::render_layer(camera& cam, sf::RenderWindow& win, int layer)
{
    for(client_renderable& i : entities)
    {
        if(i.render_layer != layer && i.render_layer != -1)
            continue;

        i.render(cam, win);
    }
}


void player_model::cleanup(vec2f my_pos)
{
    for(auto it = renderables.begin(); it != renderables.end();)
    {
        if(it->second.bad(my_pos, it->second.r.position))
            it = renderables.erase(it);
        else
            it++;
    }
}

void player_model::tick(double dt_s)
{
    for(auto& i : renderables)
    {
        if(i.second.is_unknown)
            i.second.r.position += i.second.velocity * dt_s;
    }
}
