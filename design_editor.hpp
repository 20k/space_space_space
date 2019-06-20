#ifndef DESIGN_EDITOR_HPP_INCLUDED
#define DESIGN_EDITOR_HPP_INCLUDED

#include <vector>
#include "ship_components.hpp"
#include <iostream>

#define MAX_COMPONENT_SHIP_AMOUNT 15.f

namespace sf
{
    struct RenderWindow;
}

void render_component_simple(const component& c);
void render_component_compact(const component& c, int id, float size_mult, float real_size);

struct design_editor;
struct material;

///?
struct player_research : serialisable, owned
{
    std::vector<component> components;

    void render(design_editor& edit, vec2f upper_size);

    SERIALISE_SIGNATURE(player_research)
    {
        DO_SERIALISE(components);
    }

    void operator=(player_research&& other)
    {
        components = std::move(other.components);
    }

    player_research& operator=(const player_research& other) = default;

    void merge_into_me(player_research& other);
};

struct blueprint;

struct blueprint_node : serialisable
{
    //static inline int gid = 0;
    //int id = gid++;

    component my_comp;
    component original;
    std::string name;
    vec2f pos = {0,0};
    float size = 1;

    bool cleanup = false;

    void render(design_editor& edit, blueprint& parent);

    SERIALISE_SIGNATURE(blueprint_node)
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
    std::vector<std::string> tags;

    void add_component_at(const component& c, vec2f pos, float size);

    blueprint_render_state render(design_editor& edit, vec2f upper_size);

    ship to_ship() const;
    float get_build_time_s(float build_power);

    std::vector<std::vector<material>> get_cost() const;

    void add_tag(const std::string& tg);

    std::string unbaked_tag;

    SERIALISE_SIGNATURE(blueprint)
    {
        DO_SERIALISE(nodes);
        DO_SERIALISE(name);
        DO_SERIALISE(overall_size);
        DO_SERIALISE(tags);
    }
};

bool shares_blueprint(const ship& s1, const ship& s2);

float get_build_time_s(const ship& s, float build_power);
float get_build_work(const ship& s);

void clean_tag(std::string& in);
void clean_blueprint_name(std::string& in);

struct blueprint_manager : serialisable, owned
{
    std::vector<blueprint> blueprints;
    bool dirty = false;

    SERIALISE_SIGNATURE(blueprint_manager)
    {
        DO_SERIALISE(blueprints);
        DO_RPC(create_blueprint);
        DO_RPC(upload_blueprint);
    }

    std::optional<blueprint*> fetch(size_t id)
    {
        for(auto& i : blueprints)
        {
            if(i._pid == id)
                return &i;
        }

        return std::nullopt;
    }

    void create_blueprint()
    {
        dirty = true;

        printf("Create\n");

        blueprints.push_back(blueprint());
    }

    void upload_blueprint(blueprint print)
    {
        dirty = true;

        printf("UBlueprint\n");

        clean_blueprint_name(print.name);

        print.overall_size = clamp(print.overall_size, 0.01, 100);

        for(blueprint_node& n : print.nodes)
        {
            n.size = clamp(n.size, 0.25, 4);
        }

        for(auto& t : print.tags)
        {
            clean_tag(t);
        }

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
    uint32_t currently_selected = -1;

    player_research& research;
    blueprint_manager server_blueprint_manage;
    blueprint_manager blueprint_manage;

    design_editor(player_research& _r) : research(_r){}

    void tick(double dt_s);
    void render(sf::RenderWindow& win);

    bool open = false;

    std::optional<component*> fetch(uint32_t id);

    bool dragging = false;
    //uint32_t dragging_id = -1;
    component dragged;
    float dragging_size = 1;
};

void get_ship_cost(const ship& s, std::vector<std::vector<material>>& out);
void render_ship_cost(const ship& s, const std::vector<std::vector<material>>& mats);

#endif // DESIGN_EDITOR_HPP_INCLUDED
