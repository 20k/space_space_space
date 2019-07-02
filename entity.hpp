#ifndef ENTITY_HPP_INCLUDED
#define ENTITY_HPP_INCLUDED

#include <vec/vec.hpp>
#include <networking/serialisable_fwd.hpp>
#include "aggregates.hpp"

namespace sf
{
    struct RenderWindow;
}

struct entity_manager;
struct camera;

struct client_renderable : serialisable
{
    vec2f position = {0,0};
    float rotation = 0;

    float z_level = 1;
    int render_layer = -1;

    std::vector<float> vert_dist;
    std::vector<float> vert_angle;
    std::vector<vec4f> vert_cols;

    float approx_rad = 0;
    float scale = 2;

    ///turns out this is half dim
    vec2f approx_dim = {0,0};

    uint32_t network_owner = -1;
    bool transient = false;

    SERIALISE_SIGNATURE(client_renderable);

    void init_xagonal(float rad, int n);
    void init_rectangular(vec2f dim);
    void render(camera& cam, sf::RenderWindow& window);

    //client_renderable carve(float start_angle, float half_angle);

    client_renderable split(float world_angle);
    client_renderable merge_into_me(client_renderable& r1);

    void insert(float dist, float angle, vec4f col);
};

struct entity : serialisable, owned
{
    bool cleanup = false;
    int cleanup_rounds = 0;
    bool collides = true;
    bool is_collided_with = true;
    bool aggregate_unconditionally = false;
    int ticks_between_collisions = 1;

    float mass = 1;

    std::vector<size_t> phys_ignore;

    client_renderable r;
    vec2f last_position;

    vec2f velocity = {0,0};
    vec2f control_force = {0,0};
    vec2f force = {0,0};

    float angular_velocity = 0;
    float control_angular_force = 0;
    float angular_force = 0;

    bool phys_drag = false;

    ///this is a performance compromise
    ///TODO: store typeid
    bool is_heat = false;

    virtual void tick(double dt_s) {};
    void tick_phys(double dt_s);
    virtual void tick_pre_phys(double dt_s){}
    void apply_inputs(double dt_s, double velocity_mult, double angular_mult);

    double angular_drag = M_PI/8;
    double velocity_drag = angular_drag * 20;

    vec2f get_world_pos(int vertex_id);
    bool point_within(vec2f point);

    entity_manager* get_parent();
    entity_manager* parent = nullptr;

    void set_parent_entity(entity* en, vec2f absolute_position);
    entity* parent_entity = nullptr;
    vec2f parent_offset = {0,0};

    ///for aggregates
    vec2f get_pos();
    vec2f get_dim();

    entity(){}
    entity(temporary_owned tmp) : owned(tmp){}

    virtual void pre_collide(entity& other){}
    virtual void on_collide(entity_manager& em, entity& other){}

    float get_cross_section(float angle);

    virtual ~entity(){}

    SERIALISE_SIGNATURE(entity);
};

bool collides(entity& e1, entity& e2);

struct entity_manager : serialisable
{
    std::vector<entity*> entities;
    std::vector<entity*> to_spawn;

    static inline size_t gid = 0;
    uint32_t iteration = 0;

    bool use_aggregates = true;
    bool aggregates_dirty = true;
    int last_aggregated = 0;
    bool any_moving = false;

    all_aggregates<aggregate<entity*>> collision;

    template<typename T, typename... U>
    T* make_new(U&&... u)
    {
        T* e = new T(std::forward<U>(u)...);

        to_spawn.push_back(e);
        e->parent = this;

        return e;
    }

    template<typename T>
    T* take(const T& in)
    {
        T* e = new T(in);

        to_spawn.push_back(e);
        e->parent = this;

        return e;
    }

    void forget(entity* in);
    void steal(entity* in);

    template<typename T>
    std::vector<T*> fetch()
    {
        std::vector<T*> ret;

        for(auto& i : entities)
        {
            if(dynamic_cast<T*>(i))
            {
                ret.push_back((T*)i);
            }
        }

        for(auto& i : to_spawn)
        {
            if(dynamic_cast<T*>(i))
            {
                ret.push_back((T*)i);
            }
        }

        return ret;
    }

    std::optional<entity*> fetch(uint32_t id)
    {
        for(entity* e : entities)
        {
            if(e->_pid == id)
                return e;
        }

        return std::nullopt;
    }

    bool contains(entity* e);

    void tick(double dt_s, bool reaggregate = true);

    void render(camera& cam, sf::RenderWindow& window);
    void render_layer(camera& cam, sf::RenderWindow& window, int layer);

    std::optional<entity*> collides_with_any(vec2f centre, vec2f dim, float angle);

    void force_spawn();

    ///ok, need to modify this so we only update collision meshes intermittently
    ///so: need to pick one coarse, go through it and its subobjects and redistribute between
    ///other meshes, then recalculate any affected meshes
    ///only fully reaggregate on a spawn for the moment?
    void handle_aggregates();

    void partial_reaggregate(bool move_entities);

    void debug_aggregates(camera& cam, sf::RenderWindow& window);

    void cleanup();

    SERIALISE_SIGNATURE(entity_manager);
};

#endif // ENTITY_HPP_INCLUDED
