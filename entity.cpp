#include "entity.hpp"
#include <SFML/Graphics.hpp>
#include "camera.hpp"
#include <networking/serialisable.hpp>

SERIALISE_BODY(client_renderable)
{
    DO_SERIALISE(position);
    DO_SERIALISE(rotation);
    DO_SERIALISE(render_layer);

    DO_SERIALISE(vert_dist);
    DO_SERIALISE(vert_angle);
    DO_SERIALISE(vert_cols);

    DO_SERIALISE(approx_rad);
    DO_SERIALISE(scale);
    DO_SERIALISE(network_owner);
}

SERIALISE_BODY(entity)
{
    /*DO_SERIALISE(position);
    DO_SERIALISE(rotation);

    DO_SERIALISE(vert_dist);
    DO_SERIALISE(vert_angle);
    DO_SERIALISE(vert_cols);*/

    DO_SERIALISE(r);
    DO_SERIALISE(velocity);

    //r.serialise(ctx, data);
}

SERIALISE_BODY(entity_manager)
{
    DO_SERIALISE(entities);
}

bool collides(entity& e1, entity& e2)
{
    for(auto& i : e1.phys_ignore)
    {
        if(i == e2._pid)
            return false;
    }

    for(auto& i : e2.phys_ignore)
    {
        if(i == e1._pid)
            return false;
    }

    for(int i=0; i < (int)e1.r.vert_dist.size(); i++)
    {
        vec2f p = e1.get_world_pos(i);

        if(e2.point_within(p))
        {
            return true;
        }
    }

    if(e2.point_within(e1.r.position))
        return true;

    for(int i=0; i < (int)e2.r.vert_dist.size(); i++)
    {
        vec2f p = e2.get_world_pos(i);

        if(e1.point_within(p))
        {
            return true;
        }
    }

    if(e1.point_within(e2.r.position))
        return true;

    return false;
}

void client_renderable::init_xagonal(float rad, int n)
{
    vert_angle.resize(n);
    vert_dist.resize(n);
    vert_cols.resize(n);

    for(auto& i : vert_dist)
    {
        i = rad;
    }

    float fangle = 0;

    for(auto& i : vert_angle)
    {
        i = fangle;

        fangle += (M_PI * 2) / n;
    }

    for(auto& i : vert_cols)
    {
        i = {1,1,1,1};
    }

    approx_rad = n;
    approx_dim = {n, n};
}

void client_renderable::init_rectangular(vec2f dim)
{
    float corner_rads = dim.length();

    vert_angle.resize(4);
    vert_dist.resize(4);
    vert_cols.resize(4);

    for(auto& i : vert_dist)
    {
        i = corner_rads;
    }

    float offset_angle = atan2(dim.y(), dim.x());

    vert_angle[0] = -offset_angle;
    vert_angle[1] = offset_angle;
    vert_angle[2] = -offset_angle + M_PI;
    vert_angle[3] = offset_angle + M_PI;

    for(auto& i : vert_cols)
    {
        i = {1,1,1,1};
    }

    approx_rad = corner_rads;
    approx_dim = dim;
}

void entity::apply_inputs(double dt_s, double velocity_mult, double angular_mult)
{
    //velocity += control_force * dt_s * velocity_mult;
    //angular_velocity += control_angular_force * dt_s * angular_mult;

    velocity += control_force * velocity_mult;
    angular_velocity += control_angular_force * angular_mult;

    control_force = {0,0};
    control_angular_force = 0;
}

void entity::tick_phys(double dt_s)
{
    velocity += force * dt_s;
    force = {0,0};

    angular_velocity += angular_force * dt_s;
    angular_force = 0;

    ///need to take into account mass
    r.position += velocity * dt_s;
    r.rotation += angular_velocity * dt_s;

    if(phys_drag)
    {
        float sign = signum(angular_velocity);

        angular_velocity -= sign * angular_drag * dt_s;

        if(signum(angular_velocity) != sign)
            angular_velocity = 0;

        vec2f fsign = signum(velocity);

        velocity = velocity - velocity.norm() * velocity_drag * dt_s;

        if(signum(velocity).x() != fsign.x())
            velocity.x() = 0;

        if(signum(velocity).y() != fsign.y())
            velocity.y() = 0;
    }
}

void client_renderable::render(camera& cam, sf::RenderWindow& window)
{
    float thickness = 0.75f;

    //vec3f lcol = col * 255.f;

    vec2f tl = position - approx_dim;
    vec2f br = position + approx_dim;

    vec2f tr = position + (vec2f){approx_dim.x(), -approx_dim.y()};
    vec2f bl = position + (vec2f){-approx_dim.x(), approx_dim.y()};

    if(!cam.within_screen(cam.world_to_screen(tl)) &&
       !cam.within_screen(cam.world_to_screen(tr)) &&
       !cam.within_screen(cam.world_to_screen(bl)) &&
       !cam.within_screen(cam.world_to_screen(br))
       )
        return;

    for(int i=0; i<(int)vert_dist.size(); i++)
    {
        int cur = i;
        int next = (i + 1) % vert_dist.size();

        vec4f lcol1 = vert_cols[cur] * 255;
        //vec4f lcol2 = vert_cols[next] * 255;

        float d1 = vert_dist[cur];
        float d2 = vert_dist[next];

        float a1 = vert_angle[cur];
        float a2 = vert_angle[next];

        a1 += rotation;
        a2 += rotation;

        vec2f l1 = d1 * (vec2f) {cosf(a1), sinf(a1)} * scale;
        vec2f l2 = d2 * (vec2f) {cosf(a2), sinf(a2)} * scale;

        l1 += position;
        l2 += position;

        vec2f perp = perpendicular((l2 - l1).norm());

        sf::Vertex v[4];

        vec2f ov = perp * scale * thickness;

        vec2f v1 = {l1.x() - ov.x()/2.f, l1.y() - ov.y()/2.f};
        vec2f v2 = {l2.x() - ov.x()/2.f, l2.y() - ov.y()/2.f};
        vec2f v3 = {l2.x() + ov.x()/2.f, l2.y() + ov.y()/2.f};
        vec2f v4 = {l1.x() + ov.x()/2.f, l1.y() + ov.y()/2.f};

        v1 = cam.world_to_screen(v1, z_level);
        v2 = cam.world_to_screen(v2, z_level);
        v3 = cam.world_to_screen(v3, z_level);
        v4 = cam.world_to_screen(v4, z_level);

        if(!cam.within_screen(v1) && !cam.within_screen(v2) && !cam.within_screen(v3) && !cam.within_screen(v4))
            continue;

        v[0].position = sf::Vector2f(v1.x(), v1.y());
        v[1].position = sf::Vector2f(v2.x(), v2.y());
        v[2].position = sf::Vector2f(v3.x(), v3.y());
        v[3].position = sf::Vector2f(v4.x(), v4.y());

        sf::Color scol1 = sf::Color(lcol1.x(), lcol1.y(), lcol1.z(), lcol1.w());
        //sf::Color scol2 = sf::Color(lcol2.x(), lcol2.y(), lcol2.z(), lcol2.w());

        #ifdef PROGRESSIVE
        v[0].color = scol1;
        v[1].color = scol2;
        v[2].color = scol2;
        v[3].color = scol1;
        #else
        v[0].color = scol1;
        v[1].color = scol1;
        v[2].color = scol1;
        v[3].color = scol1;
        #endif

        window.draw(v, 4, sf::Quads);
    }
}

/*client_renderable client_renderable::carve(float start_angle, float half_angle)
{
    std::vector<float> rotated_angles = vert_angle;

    for(auto& i : rotated_angles)
    {
        i += rotation;
    }

    std::vector<float> ret_angles;
    std::vector<float> ret_dist;
    std::vector<vec4f> ret_cols;

    vec2f start_dir = (vec2f){1, 0}.rot(start_angle);

    int first = -1;
    int last = -1;

    for(int i=0; i < (int)vert_angle.size(); i++)
    {
        vec2f cdir = (vec2f){1, 0}.rot(rotated_angles[i]);

        if(angle_between_vectors(start_dir, cdir) >= half_angle)
        {
            if(first == -1)
            {
                first = i;
            }

            last = i;
            continue;
        }

        ret_angles.push_back(vert_dist[i]);
        ret_dist.push_back(vert_angle[i]);
        ret_cols.push_back(vert_cols[i]);
    }

    client_renderable ret = *this;

    ret.vert_dist = ret_dist;
    ret.vert_angle = ret_angles;
    ret.vert_cols = ret_cols;

    if(first == -1 && last == -1)
        return ret;

    int first_prev = (first - 1 + (int)vert_angle.size()) % (int)vert_angle.size():
    int last_next = (last + 1) % vert_angle.size();
}*/

client_renderable client_renderable::split(float world_angle)
{
    client_renderable ret = *this;

    //vec2f world_dir = (vec2f){1, 0}.rot(world_angle);
    vec2f local_dir = (vec2f){1, 0}.rot(world_angle - rotation);

    /*static sf::Clock clk;
    float fangle = clk.getElapsedTime().asMicroseconds() / 1000. / 1000.;
    local_dir = (vec2f){1, 0}.rot(fangle);*/

    std::vector<float> ret_dist;
    std::vector<float> ret_angle;
    std::vector<vec4f> ret_cols;

    for(int i=0; i < (int)vert_angle.size(); i++)
    {
        int cur = i;
        int next = (i + 1) % (int)vert_angle.size();

        float cangle = vert_angle[cur];
        float nangle = vert_angle[next];

        vec2f cdir = (vec2f){1, 0}.rot(cangle) * vert_dist[cur];
        vec2f ndir = (vec2f){1, 0}.rot(nangle) * vert_dist[next];

        if(!is_left_side(-local_dir, local_dir, cdir) && !is_left_side(-local_dir, local_dir, ndir))
            continue;

        ///left side me and next
        if(is_left_side(-local_dir, local_dir, cdir) && is_left_side(-local_dir, local_dir, ndir))
        {
            ret_dist.push_back(vert_dist[i]);
            ret_angle.push_back(vert_angle[i]);
            ret_cols.push_back(vert_cols[i]);

            continue;
        }

        if(is_left_side(-local_dir, local_dir, cdir) && !is_left_side(-local_dir, local_dir, ndir))
        {
            ///left side me, right side next

            vec2f intersection = point2line_intersection({0, 0}, local_dir, cdir, ndir);

            ret_dist.push_back(vert_dist[i]);
            ret_angle.push_back(vert_angle[i]);
            ret_cols.push_back(vert_cols[i]);

            ret_dist.push_back(intersection.length());
            ret_angle.push_back(intersection.angle());
            ret_cols.push_back(vert_cols[i]);

            continue;
        }

        if(!is_left_side(-local_dir, local_dir, cdir) && is_left_side(-local_dir, local_dir, ndir))
        {
            ///left side me, right side next

            vec2f intersection = point2line_intersection({0, 0}, -local_dir, cdir, ndir);

            ret_dist.push_back(intersection.length());
            ret_angle.push_back(intersection.angle());
            ret_cols.push_back(vert_cols[next]);

            ret_dist.push_back(vert_dist[next]);
            ret_angle.push_back(vert_angle[next]);
            ret_cols.push_back(vert_cols[next]);

            continue;
        }

        assert(false);
    }

    ret.vert_angle = ret_angle;
    ret.vert_dist = ret_dist;
    ret.vert_cols = ret_cols;

    return ret;
}

bool same_vertex(float dist1, float angle1, float dist2, float angle2)
{
    vec2f dir_1 = (vec2f){1, 0}.rot(angle1) * dist1;
    vec2f dir_2 = (vec2f){1, 0}.rot(angle2) * dist2;

    return (dir_1 - dir_2).length() < 0.00001;
}

bool any_vertex_same(float dist1, float angle1, std::vector<float>& distances, std::vector<float>& angles)
{
    for(int i=0; i < (int)distances.size(); i++)
    {
        if(same_vertex(dist1, angle1, distances[i], angles[i]))
            return true;
    }

    return false;
}

client_renderable client_renderable::merge_into_me(client_renderable& r1)
{
    client_renderable ret = r1;

    for(int i=0; i < (int)vert_dist.size(); i++)
    {
        ret.insert(vert_dist[i], vert_angle[i], vert_cols[i]);
    }

    return ret;
}

struct svertex
{
    float dist = 0;
    float angle = 0;
    vec4f col = {1,1,1,1};
};

void client_renderable::insert(float dist, float angle, vec4f col)
{
    if(any_vertex_same(dist, angle, vert_dist, vert_angle))
        return;

    std::vector<svertex> vert;
    vert.reserve(vert_dist.size() + 1);

    for(int i=0; i < (int)vert_dist.size(); i++)
    {
        vert.push_back({vert_dist[i], vert_angle[i], vert_cols[i]});
    }

    vert.push_back({dist, angle, col});

    std::sort(vert.begin(), vert.end(), [](auto& i1, auto& i2)
    {
        vec2f n1 = (vec2f){1, 0}.rot(i1.angle);
        vec2f n2 = (vec2f){1, 0}.rot(i2.angle);

        return n1.angle() < n2.angle();
    });

    vert_dist.clear();
    vert_angle.clear();
    vert_cols.clear();

    for(svertex& v : vert)
    {
        vert_dist.push_back(v.dist);
        vert_angle.push_back(v.angle);
        vert_cols.push_back(v.col);
    }

    /*if(vert_dist.size() <= 20)
        return;*/

    for(int i=0; i < (int)vert_dist.size() + 2; i++)
    {
        int cur = i % (int)vert_dist.size();
        int next = (i + 1) % (int)vert_dist.size();
        int nnext = (i + 2) % (int)vert_dist.size();

        vec2f vec_1 = (vec2f){1, 0}.rot(vert_angle[cur]) * vert_dist[cur];
        vec2f vec_2 = (vec2f){1, 0}.rot(vert_angle[next]) * vert_dist[next];
        vec2f vec_3 = (vec2f){1, 0}.rot(vert_angle[nnext]) * vert_dist[nnext];

        //if(fabs(angle_between_vectors(vec_2 - vec_1, vec_3 - vec_1)) >= 0.01)
        //    continue;

        if(point2line_shortest(vec_1, vec_3 - vec_1, vec_2).length() > 0.00001)
            continue;

        vert_dist.erase(vert_dist.begin() + next);
        vert_angle.erase(vert_angle.begin() + next);
        vert_cols.erase(vert_cols.begin() + next);

        i--;
        continue;
    }
}

vec2f entity::get_world_pos(int id)
{
    return (vec2f){cosf(r.vert_angle[id] + r.rotation), sinf(r.vert_angle[id] + r.rotation)} * r.vert_dist[id] * r.scale + r.position;
}

bool entity::point_within(vec2f point)
{
    int lside = 0;
    int rside = 0;

    //debug_circle->setPosition(point.x(), point.y());
    //debug_window->draw(*debug_circle);

    if(r.vert_angle.size() == 0 || r.vert_dist.size() == 0)
        return false;

    vec2f last_pos = get_world_pos(0);

    for(int i=0; i < (int)r.vert_dist.size(); i++)
    {
        //int cur = i;
        int next = (i + 1) % (int)r.vert_dist.size();

        vec2f p1 = last_pos;
        vec2f p2 = get_world_pos(next);

        last_pos = p2;

        //debug_circle->setPosition(p1.x(), p1.y());
        //debug_window->draw(*debug_circle);
        //debug_circle->setPosition(p2.x(), p2.y());
        //debug_window->draw(*debug_circle);

        if(is_left_side(p1, p2, point))
        {
            lside++;
        }
        else
        {
            rside++;
        }

        if(lside != 0 && rside != 0)
            return false;
    }

    if(lside > 0 && rside == 0)
        return true;

    if(rside > 0 && lside == 0)
        return true;

    return false;
}

void entity::set_parent_entity(entity* en, vec2f absolute_position)
{
    assert(en);

    parent_entity = en;
    parent_offset = absolute_position - en->r.position;
}

vec2f entity::get_pos()
{
    return r.position;
}

vec2f entity::get_dim()
{
    return r.approx_dim*2*r.scale;
}

float entity::get_cross_section(float angle)
{
    return r.approx_dim.max_elem()*2;
}

void entity_manager::forget(entity* in)
{
    aggregates_dirty = true;

    for(int i=0; i < (int)entities.size(); i++)
    {
        if(entities[i] == in)
        {
            entities.erase(entities.begin() + i);
            i--;
            continue;
        }
    }

    for(int i=0; i < (int)to_spawn.size(); i++)
    {
        if(to_spawn[i] == in)
        {
            to_spawn.erase(to_spawn.begin() + i);
            i--;
            continue;
        }
    }
}

void entity_manager::steal(entity* in)
{
    assert(in->parent);

    if(in->parent == this)
        return;

    in->parent->aggregates_dirty = true;
    this->aggregates_dirty = true;

    in->parent->forget(in);
    to_spawn.push_back(in);
    in->parent = this;
}

bool entity_manager::contains(entity* e)
{
    for(auto& i : entities)
    {
        if(i == e)
            return true;
    }

    for(auto& i : to_spawn)
    {
        if(i == e)
            return true;
    }

    return false;
}

void entity_manager::tick(double dt_s, bool reaggregate)
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

    any_moving = false;

    for(entity* e : last_entities)
    {
        e->tick_phys(dt_s);

        ///done here because its the last bulk operation done
        {
            if(e->last_position != e->r.position)
                any_moving = true;

            e->last_position = e->r.position;
        }
    }

    if(use_aggregates)
    {
        //sf::Clock aggy;

        if(aggregates_dirty)
            handle_aggregates();
        else
            partial_reaggregate(reaggregate);

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

            if(e1->velocity == (vec2f){0,0})
                continue;

            auto id = e1->_pid;
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

void entity_manager::render(camera& cam, sf::RenderWindow& window)
{
    for(entity* e : entities)
    {
        e->r.render(cam, window);
    }
}

void entity_manager::render_layer(camera& cam, sf::RenderWindow& window, int layer)
{
    for(entity* e : entities)
    {
        if(e->r.render_layer != layer && e->r.render_layer != -1)
            continue;

        e->r.render(cam, window);
    }
}

std::optional<entity*> entity_manager::collides_with_any(vec2f centre, vec2f dim, float angle)
{
    entity test(temporary_owned{});
    test.r.init_rectangular(dim);
    test.r.position = centre;
    test.r.rotation = angle;

    for(entity* e : entities)
    {
        if(collides(*e, test))
            return e;
    }

    return std::nullopt;
}

void entity_manager::force_spawn()
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
void entity_manager::handle_aggregates()
{
    std::vector<entity*> my_entities;
    my_entities.reserve(entities.size());

    for(entity* e : entities)
    {
        if((e->collides && e->is_collided_with) || e->aggregate_unconditionally)
        {
            my_entities.push_back(e);
        }
    }

    all_aggregates<entity*> nsecond = collect_aggregates(my_entities, 10);

    collision.data.clear();
    collision.data.reserve(nsecond.data.size());

    for(aggregate<entity*>& to_process : nsecond.data)
    {
        collision.data.emplace_back(collect_aggregates(to_process.data, 10));
    }

    collision.complete();
}

void entity_manager::partial_reaggregate(bool move_entities)
{
    if(collision.data.size() == 0)
        return;

    if(!any_moving)
        return;

    if(move_entities)
    {
        int ccoarse = (last_aggregated++) % collision.data.size();

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

                /*for(int coarse_id = 0; coarse_id < (int)collision.data.size(); coarse_id++)
                {
                    auto& coarse = collision.data[coarse_id];

                    for(int fine_id = 0; fine_id < (int)coarse.data.size(); fine_id++)
                    {
                        auto& fine = coarse.data[fine_id];

                        float dist = (e->r.position - fine.get_pos()).squared_length();

                        if(dist < min_dist)
                        {
                            min_dist = dist;
                            nearest_fine = fine_id;
                            nearest_coarse = coarse_id;
                        }
                    }
                }*/

                for(int coarse_id = 0; coarse_id < (int)collision.data.size(); coarse_id++)
                {
                    float dist = (collision.data[coarse_id].pos - e->r.position).squared_length();

                    if(dist < min_dist)
                    {
                        min_dist = dist;
                        nearest_coarse = coarse_id;
                    }
                }

                if(nearest_coarse == -1)
                {
                    entity_it++;
                    continue;
                }

                min_dist = FLT_MAX;

                auto& coarse = collision.data[nearest_coarse];

                for(int fine_id = 0; fine_id < (int)coarse.data.size(); fine_id++)
                {
                    auto& fine = coarse.data[fine_id];

                    float dist = (e->r.position - fine.get_pos()).squared_length();

                    if(dist < min_dist)
                    {
                        min_dist = dist;
                        nearest_fine = fine_id;
                    }
                }

                if(nearest_fine == -1)
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
    }

    for(auto& coarse : collision.data)
    {
        for(auto& fine : coarse.data)
        {
            fine.complete();
        }

        coarse.complete();
    }

    collision.complete();
}

void entity_manager::debug_aggregates(camera& cam, sf::RenderWindow& window)
{
    for(aggregate<aggregate<entity*>>& agg : collision.data)
    {
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

        vec2f rpos = cam.world_to_screen(agg.pos, 1);

        sf::RectangleShape shape;
        shape.setSize({agg.half_dim.x()*2, agg.half_dim.y()*2});
        shape.setPosition(rpos.x(), rpos.y());
        shape.setOrigin(shape.getSize().x/2, shape.getSize().y/2);
        shape.setFillColor(sf::Color::Transparent);
        shape.setOutlineThickness(2);
        shape.setOutlineColor(sf::Color::White);

        window.draw(shape);
    }


    vec2f rpos = cam.world_to_screen(collision.pos, 1);

    sf::RectangleShape shape;
    shape.setSize({collision.half_dim.x()*2, collision.half_dim.y()*2});
    shape.setPosition(rpos.x(), rpos.y());
    shape.setOrigin(shape.getSize().x/2, shape.getSize().y/2);
    shape.setFillColor(sf::Color::Transparent);
    shape.setOutlineThickness(2);
    shape.setOutlineColor(sf::Color::Blue);

    window.draw(shape);
}

void entity_manager::cleanup()
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

    if(aggregates_dirty)
    {
        handle_aggregates();
        aggregates_dirty = false;
    }
}
