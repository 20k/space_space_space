#ifndef ENTITY_HPP_INCLUDED
#define ENTITY_HPP_INCLUDED

#include <vec/vec.hpp>
#include <networking/serialisable.hpp>

namespace sf
{
    struct RenderWindow;
}

struct entity_manager;

struct client_renderable : serialisable
{
    vec2f position = {0,0};
    float rotation = 0;

    std::vector<float> vert_dist;
    std::vector<float> vert_angle;
    std::vector<vec3f> vert_cols;

    float approx_rad = 0;
    float scale = 2;

    vec2f approx_dim = {0,0};

    uint32_t network_owner = -1;

    void serialise(nlohmann::json& data, bool encode)
    {
        //if(!encode)

        DO_SERIALISE(position);
        DO_SERIALISE(rotation);

        DO_SERIALISE(vert_dist);
        DO_SERIALISE(vert_angle);
        DO_SERIALISE(vert_cols);
        //if(!encode)
        //    std::cout <<" VCOL " << vert_cols[0].x() << std::endl;

        DO_SERIALISE(approx_rad);
        DO_SERIALISE(scale);
        DO_SERIALISE(network_owner);
    }

    void init_rectangular(vec2f dim);
    void render(sf::RenderWindow& window);
};

struct entity : virtual serialisable
{
    size_t id = 0;
    bool cleanup = false;
    bool collides = true;

    std::vector<size_t> phys_ignore;

    client_renderable r;

    vec2f velocity = {0,0};
    vec2f control_force = {0,0};

    float angular_velocity = 0;
    float control_angular_force = 0;

    bool drag = false;

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

    virtual void on_collide(entity_manager& em, entity& other){}

    virtual ~entity(){}

    void serialise(nlohmann::json& data, bool encode) override
    {
        /*DO_SERIALISE(position);
        DO_SERIALISE(rotation);

        DO_SERIALISE(vert_dist);
        DO_SERIALISE(vert_angle);
        DO_SERIALISE(vert_cols);*/

        r.serialise(data, encode);
    }
};

bool collides(entity& e1, entity& e2);

struct entity_manager : serialisable
{
    std::vector<entity*> entities;
    std::vector<entity*> to_spawn;

    static inline size_t gid = 0;

    template<typename T, typename... U>
    T* make_new(U&&... u)
    {
        T* e = new T(std::forward<U>(u)...);

        to_spawn.push_back(e);
        e->parent = this;
        e->id = gid++;

        return e;
    }

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

        return ret;
    }

    void tick(double dt_s)
    {
        for(auto& i : to_spawn)
        {
            entities.push_back(i);
        }

        to_spawn.clear();

        auto last_entities = entities;

        for(entity* e : last_entities)
        {
            e->tick(dt_s);
        }

        for(entity* e : last_entities)
        {
            e->tick_pre_phys(dt_s);
        }

        for(entity* e : last_entities)
        {
            e->tick_phys(dt_s);
        }

        for(int i=0; i < (int)last_entities.size(); i++)
        {
            if(!last_entities[i]->collides)
                continue;

            vec2f p1 = last_entities[i]->r.position;
            float r1 = last_entities[i]->r.approx_rad;

            for(int j = 0; j < (int)last_entities.size(); j++)
            {
                if(last_entities[j]->collides)
                {
                    if(j <= i)
                        continue;
                }

                vec2f p2 = last_entities[j]->r.position;
                float r2 = last_entities[j]->r.approx_rad;

                if((p2 - p1).squared_length() > ((r1 * 1.5f + r2 * 1.5f) * (r1 * 1.5f + r2 * 1.5f)) * 4.f)
                    continue;

                if(collides(*last_entities[i], *last_entities[j]))
                {
                    last_entities[i]->on_collide(*this, *last_entities[j]);
                    last_entities[j]->on_collide(*this, *last_entities[i]);
                }
            }
        }

        for(entity* e : last_entities)
        {
            if(e->parent_entity)
            {
                e->r.position = e->parent_entity->r.position + e->parent_offset;
            }
        }

        for(auto& i : to_spawn)
        {
            entities.push_back(i);
        }

        to_spawn.clear();
    }

    void render(sf::RenderWindow& window)
    {
        for(entity* e : entities)
        {
            e->r.render(window);
        }
    }

    void cleanup()
    {
        for(auto& i : to_spawn)
        {
            entities.push_back(i);
        }

        to_spawn.clear();

        for(entity* e : entities)
        {
            if(e->parent_entity && e->parent_entity->cleanup)
            {
                e->cleanup = true;
                e->parent_entity = nullptr;
            }
        }

        for(int i=0; i < (int)entities.size(); i++)
        {
            if(entities[i]->cleanup)
            {
                entity* ptr = entities[i];

                entities.erase(entities.begin() + i);

                delete ptr;
                i--;
                continue;
            }
        }
    }

    virtual void serialise(nlohmann::json& data, bool encode) override
    {
        DO_SERIALISE(entities);
    }
};

struct client_entities : serialisable
{
    std::vector<client_renderable> entities;

    void render(sf::RenderWindow& win);

    virtual void serialise(nlohmann::json& data, bool encode) override
    {
        DO_SERIALISE(entities);
    }
};

struct client_input : serialisable
{
    vec2f direction = {0,0};
    float rotation = 0;
    bool fired = false;
    bool ping = false;

    virtual void serialise(nlohmann::json& data, bool encode) override
    {
        DO_SERIALISE(direction);
        DO_SERIALISE(rotation);
        DO_SERIALISE(fired);
        DO_SERIALISE(ping);
    }
};

#endif // ENTITY_HPP_INCLUDED
