#ifndef ENTITY_HPP_INCLUDED
#define ENTITY_HPP_INCLUDED

#include <vec/vec.hpp>
#include <networking/serialisable.hpp>
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

    std::vector<float> vert_dist;
    std::vector<float> vert_angle;
    std::vector<vec4f> vert_cols;

    float approx_rad = 0;
    float scale = 2;

    ///turns out this is half dim
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
    void render(camera& cam, sf::RenderWindow& window);

    //client_renderable carve(float start_angle, float half_angle);

    client_renderable split(float world_angle);
    client_renderable merge_into_me(client_renderable& r1);

    void insert(float dist, float angle, vec4f col);
};

struct entity : virtual serialisable
{
    size_t id = 0;
    bool cleanup = false;
    int cleanup_rounds = 0;
    bool collides = true;

    float mass = 1;

    std::vector<size_t> phys_ignore;

    client_renderable r;

    vec2f velocity = {0,0};
    vec2f control_force = {0,0};
    vec2f force = {0,0};

    float angular_velocity = 0;
    float control_angular_force = 0;
    float angular_force = 0;

    bool drag = false;

    ///this is a performance compromise
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

    virtual void pre_collide(entity& other){}
    virtual void on_collide(entity_manager& em, entity& other){}

    float get_cross_section(float angle);

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

    all_aggregates<aggregate<entity*>> collision;

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

    std::optional<entity*> fetch(uint32_t id)
    {
        for(entity* e : entities)
        {
            if(e->id == id)
                return e;
        }

        return std::nullopt;
    }

    void tick(double dt_s)
    {
        force_spawn();

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

        handle_aggregates();

        /*for(int i=0; i < (int)last_entities.size(); i++)
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
                else
                {
                    continue;
                }

                vec2f p2 = last_entities[j]->r.position;
                float r2 = last_entities[j]->r.approx_rad;

                if((p2 - p1).squared_length() > ((r1 * 1.5f + r2 * 1.5f) * (r1 * 1.5f + r2 * 1.5f)) * 4.f)
                    continue;

                if(collides(*last_entities[i], *last_entities[j]))
                {
                    last_entities[i]->pre_collide(*last_entities[j]);
                    last_entities[j]->pre_collide(*last_entities[i]);

                    last_entities[i]->on_collide(*this, *last_entities[j]);
                    last_entities[j]->on_collide(*this, *last_entities[i]);
                }
            }
        }*/

        for(int c_1=0; c_1 < (int)collision.data.size(); c_1++)
        {
            for(int c_2=c_1; c_2 < (int)collision.data.size(); c_2++)
            {
                auto& coarse_1 = collision.data[c_1];
                auto& coarse_2 = collision.data[c_2];

                if(c_1 != c_2 && !coarse_1.intersects(coarse_2))
                    continue;

                for(int f_1 = 0; f_1 < (int)coarse_1.data.size(); f_1++)
                {
                    int f_2_start = c_1 == c_2 ? f_1 : 0;

                    for(int f_2 = f_2_start; f_2 < (int)coarse_2.data.size(); f_2++)
                    {
                        auto& fine_1 = coarse_1.data[f_1];
                        auto& fine_2 = coarse_2.data[f_2];

                        if(f_1 != f_2 && !fine_1.intersects(fine_2))
                            continue;

                        for(int i_1 = 0; i_1 < (int)fine_1.data.size(); i_1++)
                        {
                            ///no self intersect
                            int i_2_start = f_1 == f_2 ? i_1+1 : 0;

                            for(int i_2 = i_2_start; i_2 < (int)fine_2.data.size(); i_2++)
                            {
                                entity* e1 = fine_1.data[i_1];
                                entity* e2 = fine_2.data[i_2];

                                if(!e1->collides || !e2->collides)
                                    continue;

                                vec2f p1 = e1->r.position;
                                float r1 = e1->r.approx_rad;

                                vec2f p2 = e2->r.position;
                                float r2 = e2->r.approx_rad;

                                if((p2 - p1).squared_length() > ((r1 * 1.5f + r2 * 1.5f) * (r1 * 1.5f + r2 * 1.5f)) * 4.f)
                                    continue;

                                if(collides(*e1, *e2))
                                {
                                    e1->pre_collide(*e2);
                                    e2->pre_collide(*e1);

                                    e1->on_collide(*this, *e2);
                                    e2->on_collide(*this, *e1);
                                }
                            }
                        }
                    }
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

        force_spawn();
    }

    void render(camera& cam, sf::RenderWindow& window)
    {
        for(entity* e : entities)
        {
            e->r.render(cam, window);
        }
    }

    void force_spawn()
    {
        for(auto& i : to_spawn)
        {
            entities.push_back(i);
        }

        to_spawn.clear();
    }

    void handle_aggregates()
    {
        all_aggregates<entity*> nsecond = collect_aggregates(entities, 20);

        all_aggregates<aggregate<entity*>> second_level;
        nsecond.data.reserve(nsecond.data.size());

        aggregate<entity*> aggregates;

        for(aggregate<entity*>& to_process : nsecond.data)
        {
            all_aggregates<entity*> subaggr = collect_aggregates(to_process.data, 10);

            second_level.data.push_back(subaggr);
        }

        collision = second_level;
    }

    void cleanup()
    {
        force_spawn();

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
                entities[i]->cleanup_rounds++;
            }
        }

        for(int i=0; i < (int)entities.size(); i++)
        {
            if(entities[i]->cleanup && entities[i]->cleanup_rounds == 2)
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

#endif // ENTITY_HPP_INCLUDED
