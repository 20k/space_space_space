#ifndef ENTITY_HPP_INCLUDED
#define ENTITY_HPP_INCLUDED

#include <vec/vec.hpp>
#include <networking/serialisable.hpp>
#include "aggregates.hpp"
#include <SFML/Graphics.hpp>
#include "camera.hpp"

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

    SERIALISE_SIGNATURE()
    {
        DO_SERIALISE(position);
        DO_SERIALISE(rotation);

        DO_SERIALISE(vert_dist);
        DO_SERIALISE(vert_angle);
        DO_SERIALISE(vert_cols);

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
    bool is_collided_with = true;
    int ticks_between_collisions = 1;

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

    SERIALISE_SIGNATURE()
    {
        /*DO_SERIALISE(position);
        DO_SERIALISE(rotation);

        DO_SERIALISE(vert_dist);
        DO_SERIALISE(vert_angle);
        DO_SERIALISE(vert_cols);*/

        DO_SERIALISE(r);

        //r.serialise(ctx, data);
    }
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

        if(use_aggregates)
        {
            //sf::Clock aggy;

            if(aggregates_dirty)
                handle_aggregates();
            else
                partial_reaggregate();

            aggregates_dirty = false;

            //double aggy_time = aggy.getElapsedTime().asMicroseconds() / 1000.;
            //std::cout << "aggyt " << aggy_time << std::endl;

            //#define ALL_RECTS
            #ifdef ALL_RECTS
            int num_fine_intersections = 0;

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

                            num_fine_intersections++;

                            for(int i_1 = 0; i_1 < (int)fine_1.data.size(); i_1++)
                            {
                                ///no self intersect
                                int i_2_start = (c_1 == c_2 && f_1 == f_2) ? i_1+1 : 0;

                                for(int i_2 = i_2_start; i_2 < (int)fine_2.data.size(); i_2++)
                                {
                                    entity* e1 = fine_1.data[i_1];
                                    entity* e2 = fine_2.data[i_2];

                                    if(!e1->collides || !e2->collides)
                                        continue;

                                    if(e1 == e2)
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

            #endif // ALL_RECTS

            #define HALF_RECTS
            #ifdef HALF_RECTS
            for(entity* e1 : entities)
            {
                if(!e1->collides)
                    continue;

                auto id = e1->id;
                auto ticks_between_collisions = e1->ticks_between_collisions;

                if(ticks_between_collisions > 1)
                {
                    if((iteration % ticks_between_collisions) != (id % ticks_between_collisions))
                        continue;
                }

                vec2f tl = e1->r.position - e1->r.approx_dim;
                vec2f br = e1->r.position + e1->r.approx_dim;

                for(auto& coarse : collision.data)
                {
                    if(!rect_intersect(coarse.tl, coarse.br, tl, br))
                        continue;

                    for(auto& fine : coarse.data)
                    {
                        if(!rect_intersect(fine.tl, fine.br, tl, br))
                            continue;

                        for(entity* e2 : fine.data)
                        {
                            if(!e2->is_collided_with)
                                continue;

                            if(!e2->collides)
                                continue;

                            if(e1 == e2)
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
            #endif // HALF_RECTS

            //printf("fine %i\n", num_fine_intersections);
        }
        else
        {
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
                    else
                    {
                        continue;
                    }

                    if(!last_entities[j]->is_collided_with)
                        continue;

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

        iteration++;
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

        aggregates_dirty = aggregates_dirty || (to_spawn.size() > 0);

        to_spawn.clear();
    }

    ///ok, need to modify this so we only update collision meshes intermittently
    ///so: need to pick one coarse, go through it and its subobjects and redistribute between
    ///other meshes, then recalculate any affected meshes
    ///only fully reaggregate on a spawn for the moment?
    void handle_aggregates()
    {
        std::vector<entity*> my_entities;
        my_entities.reserve(entities.size());

        for(entity* e : entities)
        {
            if(e->collides && e->is_collided_with)
            {
                my_entities.push_back(e);
            }
        }

        all_aggregates<entity*> nsecond = collect_aggregates(my_entities, 20);

        collision.data.clear();
        collision.data.reserve(nsecond.data.size());

        for(aggregate<entity*>& to_process : nsecond.data)
        {
            collision.data.push_back(collect_aggregates(to_process.data, 10));
        }
    }

    void partial_reaggregate()
    {
        if(collision.data.size() == 0)
            return;

        int ccoarse = iteration % collision.data.size();

        auto& to_restrib = collision.data[ccoarse];

        ///for fine in coarse
        for(auto& trf : to_restrib.data)
        {
            ///don't fully deplete
            if(trf.data.size() == 1)
                continue;

            ///for entities in fine
            for(auto entity_it = trf.data.begin(); entity_it != trf.data.end();)
            {
                entity* e = *entity_it;

                float min_dist = FLT_MAX;
                int nearest_fine = -1;
                int nearest_coarse = -1;

                for(int coarse_id = 0; coarse_id < (int)collision.data.size(); coarse_id++)
                {
                    auto& coarse = collision.data[coarse_id];

                    for(int fine_id = 0; fine_id < (int)coarse.data.size(); fine_id++)
                    {
                        auto& fine = coarse.data[fine_id];

                        float dist = (e->r.position - fine.get_pos()).length();

                        if(dist < min_dist)
                        {
                            min_dist = dist;
                            nearest_fine = fine_id;
                            nearest_coarse = coarse_id;
                        }
                    }
                }

                if(nearest_coarse == -1 || nearest_fine == -1)
                {
                    entity_it++;
                    continue;
                }

                if(&collision.data[nearest_coarse].data[nearest_fine] == &trf)
                {
                    entity_it++;
                    continue;
                }

                entity_it = trf.data.erase(entity_it);
                collision.data[nearest_coarse].data[nearest_fine].data.push_back(e);
            }
        }

        collision.complete();

        for(auto& coarse : collision.data)
        {
            for(auto& fine : coarse.data)
            {
                fine.complete();
            }

            coarse.complete();
        }
    }

    void debug_aggregates(camera& cam, sf::RenderWindow& window)
    {
        for(aggregate<aggregate<entity*>>& agg : collision.data)
        {
            vec2f rpos = cam.world_to_screen(agg.pos, 1);

            sf::RectangleShape shape;
            shape.setSize({agg.half_dim.x()*2, agg.half_dim.y()*2});
            shape.setPosition(rpos.x(), rpos.y());
            shape.setOrigin(shape.getSize().x/2, shape.getSize().y/2);
            shape.setFillColor(sf::Color::Transparent);
            shape.setOutlineThickness(2);
            shape.setOutlineColor(sf::Color::White);

            window.draw(shape);

            for(aggregate<entity*>& subagg : agg.data)
            {
                vec2f rpos = cam.world_to_screen(subagg.pos, 1);

                sf::RectangleShape shape;
                shape.setSize({subagg.half_dim.x()*2, subagg.half_dim.y()*2});
                shape.setPosition(rpos.x(), rpos.y());
                shape.setOrigin(shape.getSize().x/2, shape.getSize().y/2);
                shape.setFillColor(sf::Color::Transparent);
                shape.setOutlineThickness(2);
                shape.setOutlineColor(sf::Color::Red);

                window.draw(shape);
            }
        }
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

                aggregates_dirty = true;

                delete ptr;
                i--;
                continue;
            }
        }
    }

    SERIALISE_SIGNATURE()
    {
        DO_SERIALISE(entities);
    }
};

#endif // ENTITY_HPP_INCLUDED
