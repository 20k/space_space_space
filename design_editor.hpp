#ifndef DESIGN_EDITOR_HPP_INCLUDED
#define DESIGN_EDITOR_HPP_INCLUDED

#include <vector>
#include "ship_components.hpp"

namespace sf
{
    struct RenderWindow;
}

void render_component_simple(const component& c);
void render_component_compact(const component& c, int id, float size);

struct design_editor;

///?
struct player_research : serialisable, owned
{
    std::vector<component> components;

    void render(design_editor& edit, vec2f upper_size);

    SERIALISE_SIGNATURE()
    {
        DO_SERIALISE(components);
    }
};

struct blueprint_node
{
    static inline int gid = 0;
    int id = gid++;

    component my_comp;
    std::string name;
    vec2f pos = {0,0};
    bool cleanup = false;
    float size = 1;

    void render(design_editor& edit);
};

struct blueprint_render_state
{
    vec2f pos;
    bool is_hovered = false;
};

struct blueprint
{
    std::vector<blueprint_node> nodes;

    blueprint_render_state render(design_editor& edit, vec2f upper_size);

    ship to_ship();
};

struct design_editor
{
    player_research research;

    blueprint cur;

    void tick(double dt_s);
    void render(sf::RenderWindow& win);

    bool open = false;

    std::optional<component*> fetch(uint32_t id);

    bool dragging = false;
    uint32_t dragging_id = -1;
    float dragging_size = 1;
};

#endif // DESIGN_EDITOR_HPP_INCLUDED
