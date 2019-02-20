#include "entity.hpp"
#include <SFML/Graphics.hpp>

bool collides(entity& e1, entity& e2)
{
    for(auto& i : e1.phys_ignore)
    {
        if(i == e2.id)
            return false;
    }

    for(auto& i : e2.phys_ignore)
    {
        if(i == e1.id)
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
        i = {1,1,1};
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

    if(drag)
    {
        float sign = signum(angular_velocity);

        /*if(fabs(angular_velocity) < fabs(sign))
        {
            sign = angular_velocity;
        }*/

        angular_velocity -= sign * angular_drag * dt_s;

        if(signum(angular_velocity) != sign)
            angular_velocity = 0;

        vec2f fsign = signum(velocity);

        /*for(int i=0; i < 2; i++)
        {
            if(fabs(velocity.v[i]) < fabs(fsign.v[i]))
            {
                fsign.v[i] = velocity.v[i];
            }
        }*/

        velocity = velocity - fsign * velocity_drag * dt_s;

        if(signum(velocity).x() != fsign.x())
            velocity.x() = 0;

        if(signum(velocity).y() != fsign.y())
            velocity.y() = 0;
    }
}

void client_renderable::render(sf::RenderWindow& window)
{
    float thickness = 0.75f;

    //vec3f lcol = col * 255.f;

    for(int i=0; i<(int)vert_dist.size(); i++)
    {
        int cur = i;
        int next = (i + 1) % vert_dist.size();

        vec3f lcol = vert_cols[cur] * 255;

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

        v[0].position = sf::Vector2f(l1.x() - ov.x()/2.f, l1.y() - ov.y()/2.f);
        v[1].position = sf::Vector2f(l2.x() - ov.x()/2.f, l2.y() - ov.y()/2.f);
        v[2].position = sf::Vector2f(l2.x() + ov.x()/2.f, l2.y() + ov.y()/2.f);
        v[3].position = sf::Vector2f(l1.x() + ov.x()/2.f, l1.y() + ov.y()/2.f);

        sf::Color scol = sf::Color(lcol.x(), lcol.y(), lcol.z());

        v[0].color = scol;
        v[1].color = scol;
        v[2].color = scol;
        v[3].color = scol;

        window.draw(v, 4, sf::Quads);
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
        int cur = i;
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
