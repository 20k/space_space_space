#ifndef DESIGN_EDITOR_HPP_INCLUDED
#define DESIGN_EDITOR_HPP_INCLUDED

#include <vector>
#include "ship_components.hpp"
#include <iostream>

namespace sf
{
    struct RenderWindow;
}

void render_component_simple(const component& c);
void render_component_compact(const component& c, int id, float size_mult, float real_size);

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

struct blueprint;

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

    void render(design_editor& edit, blueprint& parent);

    SERIALISE_SIGNATURE()
    {
        DO_SERIALISE(original);
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
    float overall_size = 1;

    void add_component_at(const component& c, vec2f pos, float size);

    blueprint_render_state render(design_editor& edit, vec2f upper_size);

    ship to_ship();

    SERIALISE_SIGNATURE()
    {
        DO_SERIALISE(nodes);
        DO_SERIALISE(name);
        DO_SERIALISE(overall_size);
    }
};

struct blueprint_manager : serialisable, owned
{
    std::vector<blueprint> blueprints;

    SERIALISE_SIGNATURE()
    {
        DO_SERIALISE(blueprints);
        DO_RPC(create_blueprint);
        DO_RPC(upload_blueprint);
    }

    void create_blueprint()
    {
        printf("Create\n");

        blueprints.push_back(blueprint());
    }

    void upload_blueprint(blueprint print)
    {
        printf("UBlueprint\n");

        print.overall_size = clamp(print.overall_size, 0.1, 100);

        for(blueprint_node& n : print.nodes)
        {
            n.size = clamp(n.size, 0.25, 4);
        }

        for(blueprint& p : blueprints)
        {
            if(p._pid == print._pid)
            {
                p = print;

                save("temp.blueprint");
                return;
            }
        }
    }

    void save(const std::string& file)
    {
        save_to_file(file, ::serialise(*this));
    }

    void load(const std::string& file)
    {
        nlohmann::json data = load_from_file(file);

        deserialise(data, *this);
    }

    FRIENDLY_RPC_NAME(create_blueprint);
    FRIENDLY_RPC_NAME(upload_blueprint);
};

struct design_editor
{
    bool rpcd_default_blueprint = false;
    uint32_t currently_selected = -1;

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
