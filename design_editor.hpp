#ifndef DESIGN_EDITOR_HPP_INCLUDED
#define DESIGN_EDITOR_HPP_INCLUDED

#include <vector>
#include "ship_components.hpp"
#include <iostream>
#include "serialisables.hpp"
#include <networking/serialisable_fwd.hpp>

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
struct player_research : serialisable, owned, free_function
{
    std::vector<component> components;

    void render(design_editor& edit, vec2f upper_size);

    void operator=(player_research&& other)
    {
        components = std::move(other.components);
    }

    player_research& operator=(const player_research& other) = default;

    void merge_into_me(player_research& other);
};

struct blueprint;

struct blueprint_node : serialisable, free_function
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
};

struct blueprint_render_state
{
    vec2f pos;
    bool is_hovered = false;
};

namespace blueprint_info
{
    inline
    std::vector<float> sizes
    {
        0.25,
        0.5,
        0.75,
        1,
        2,
        3,
        5,
        8,
        10,
        12,
        15,
        20,
        25,
        30
    };

    inline
    std::vector<std::string> names
    {
        "Missile",
        "Torpedo",
        "Scout",
        "Fighter",
        "Corvette",
        "Frigate",
        "Destroyer",
        "Cruiser",
        "Battlecruiser",
        "Capital",
        "Supercapital",
        "Carrier",
        "Supercarrier",
        "Titan",
    };
}

struct blueprint : serialisable, owned, free_function
{
    std::vector<blueprint_node> nodes;
    std::string name;
    int size_offset = 0;
    std::vector<std::string> tags;

    void add_component_at(const component& c, vec2f pos, float size);

    blueprint_render_state render(design_editor& edit, vec2f upper_size);

    ship to_ship() const;
    float get_build_time_s(float build_power);

    std::vector<std::vector<material>> get_cost() const;

    void add_tag(const std::string& tg);

    std::string unbaked_tag;

    float get_overall_size() const
    {
        int clamped = clamp(size_offset, 0, (int)blueprint_info::sizes.size()-1);

        return blueprint_info::sizes[clamped];
    }
};

struct build_in_progress : serialisable, free_function
{
    blueprint result;
    size_t in_progress_pid = -1;

    void make(const blueprint& fin)
    {
        result = fin;
    }
};

bool shares_blueprint(const ship& s1, const ship& s2);

float get_build_time_s(const ship& s, float build_power);
float get_build_work(const ship& s);

void clean_tag(std::string& in);
void clean_blueprint_name(std::string& in);

struct blueprint_manager : serialisable, owned, free_function
{
    std::vector<blueprint> blueprints;
    bool dirty = false;

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

        print.size_offset = clamp(print.size_offset, 0, (int)blueprint_info::names.size()-1);

        //print.overall_size = clamp(print.overall_size, 0.01, 100);

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

    DECLARE_FRIENDLY_RPC(create_blueprint);
    DECLARE_FRIENDLY_RPC(upload_blueprint, blueprint);
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
