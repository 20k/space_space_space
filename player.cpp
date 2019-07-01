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
