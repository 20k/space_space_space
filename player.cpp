#include "player.hpp"

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
