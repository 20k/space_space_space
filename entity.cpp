#include "entity.hpp"
#include <SFML/Graphics.hpp>
#include "camera.hpp"

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

    if(drag)
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

    for(int i=0; i<(int)vert_dist.size(); i++)
    {
        int cur = i;
        int next = (i + 1) % vert_dist.size();

        vec4f lcol1 = vert_cols[cur] * 255;
        vec4f lcol2 = vert_cols[next] * 255;

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
        sf::Color scol2 = sf::Color(lcol2.x(), lcol2.y(), lcol2.z(), lcol2.w());

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
