#ifndef ENTITY_HPP_INCLUDED
#define ENTITY_HPP_INCLUDED

#include <vec/vec.hpp>

namespace sf
{
    struct RenderWindow;
}

struct entity_manager;

struct entity
{
    size_t id = 0;
    bool cleanup = false;
    bool collides = true;

    std::vector<size_t> phys_ignore;

    vec2f position = {0,0};
    vec2f velocity = {0,0};
    vec2f control_force = {0,0};

    float rotation = 0;
    float angular_velocity = 0;
    float control_angular_force = 0;

    std::vector<float> vert_dist;
    std::vector<float> vert_angle;
    std::vector<vec3f> vert_cols;

    void init_rectangular(vec2f dim);
    void render(sf::RenderWindow& window);

    float approx_rad = 0;

    bool drag = false;

    virtual void tick(double dt_s) {};
    void tick_phys(double dt_s);
    virtual void tick_pre_phys(double dt_s){}
    void apply_inputs(double dt_s, double velocity_mult, double angular_mult);

    double angular_drag = M_PI/8;
    double velocity_drag = angular_drag * 20;

    float scale = 2;

    vec2f get_world_pos(int vertex_id);
    bool point_within(vec2f point);

    entity_manager* get_parent();
    entity_manager* parent = nullptr;

    virtual void on_collide(entity_manager& em, entity& other){}

    virtual ~entity(){}
};

bool collides(entity& e1, entity& e2);

struct entity_manager
{
    std::vector<entity*> entities;

    static inline size_t gid = 0;

    template<typename T, typename... U>
    T* make_new(U&&... u)
    {
        T* e = new T(std::forward<U>(u)...);

        entities.push_back(e);
        e->parent = this;
        e->id = gid++;

        return e;
    }

    void tick(double dt_s)
    {
        for(entity* e : entities)
        {
            e->tick(dt_s);
        }

        for(entity* e : entities)
        {
            e->tick_pre_phys(dt_s);
        }

        for(entity* e : entities)
        {
            e->tick_phys(dt_s);
        }

        for(int i=0; i < (int)entities.size(); i++)
        {
            if(!entities[i]->collides)
                continue;

            vec2f p1 = entities[i]->position;
            float r1 = entities[i]->approx_rad;

            for(int j = 0; j < (int)entities.size(); j++)
            {
                if(entities[j]->collides)
                {
                    if(j <= i)
                        continue;
                }

                vec2f p2 = entities[j]->position;
                float r2 = entities[j]->approx_rad;

                if((p2 - p1).squared_length() > ((r1 * 1.5f + r2 * 1.5f) * (r1 * 1.5f + r2 * 1.5f)) * 4.f)
                    continue;

                if(collides(*entities[i], *entities[j]))
                {
                    entities[i]->on_collide(*this, *entities[j]);
                    entities[j]->on_collide(*this, *entities[i]);
                }
            }
        }
    }

    void render(sf::RenderWindow& window)
    {
        for(entity* e : entities)
        {
            e->render(window);
        }
    }

    void cleanup()
    {
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
};

#endif // ENTITY_HPP_INCLUDED
