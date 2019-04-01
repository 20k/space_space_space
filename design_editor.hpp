#ifndef DESIGN_EDITOR_HPP_INCLUDED
#define DESIGN_EDITOR_HPP_INCLUDED

#include <vector>
#include "ship_components.hpp"

namespace sf
{
    struct RenderWindow;
}

///?
struct player_research
{
    std::vector<component> components;

    void render(vec2f upper_size);
};

struct blueprint_node
{
    component my_comp;
    std::string name;
    vec2f pos = {0,0};

    void render();
};

struct blueprint
{
    std::vector<blueprint_node> nodes;
};

struct design_editor
{
    player_research research;

    void tick(double dt_s);
    void render(sf::RenderWindow& win);

    bool open = false;
};

#endif // DESIGN_EDITOR_HPP_INCLUDED
