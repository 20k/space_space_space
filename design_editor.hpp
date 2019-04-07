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

struct blueprint_node : serialisable
{
    static inline int gid = 0;
    int id = gid++;

    component my_comp;
    component original;
    std::string name;
    vec2f pos = {0,0};
    float size = 1;

    bool cleanup = false;

    void render(design_editor& edit);

    SERIALISE_SIGNATURE()
    {
        DO_SERIALISE(original._pid);
        DO_SERIALISE(name);
        DO_SERIALISE(pos);
        DO_SERIALISE(size);
    }
};

struct blueprint_render_state
{
    vec2f pos;
    bool is_hovered = false;
};

struct blueprint : serialisable, owned
{
    std::vector<blueprint_node> nodes;
    std::string name;

    blueprint_render_state render(design_editor& edit, vec2f upper_size);

    ship to_ship();

    SERIALISE_SIGNATURE()
    {
        DO_SERIALISE(nodes);
        DO_SERIALISE(name);
    }
};

struct blueprint_manager : serialisable, owned
{
    std::vector<blueprint> blueprints;
    int currently_selected = -1;

    SERIALISE_SIGNATURE()
    {
        DO_SERIALISE(blueprints);
        DO_SERIALISE(currently_selected);
        DO_RPC(create_blueprint);
        DO_RPC(upload_blueprint);
    }

    void create_blueprint()
    {
        blueprints.push_back(blueprint());
        currently_selected = blueprints.size() - 1;
    }

    void upload_blueprint(blueprint print)
    {
        for(blueprint& p : blueprints)
        {
            if(p._pid == print._pid)
            {
                p = print;
                return;
            }
        }
    }

    FRIENDLY_RPC_NAME(create_blueprint);
    FRIENDLY_RPC_NAME(upload_blueprint);
};

struct design_editor
{
    bool rpcd_default_blueprint = false;

    player_research research;
    blueprint_manager server_blueprint_manage;
    blueprint_manager blueprint_manage;

    void tick(double dt_s);
    void render(sf::RenderWindow& win);

    bool open = false;

    std::optional<component*> fetch(uint32_t id);

    bool dragging = false;
    uint32_t dragging_id = -1;
    float dragging_size = 1;
};

#endif // DESIGN_EDITOR_HPP_INCLUDED
