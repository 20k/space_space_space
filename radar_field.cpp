#include "radar_field.hpp"
#include <SFML/Graphics.hpp>

radar_field::radar_field(vec2f target)
{
    freq.resize(dim.y());
    for(auto& i : freq)
    {
        i.resize(dim.x());
    }

    collisions.resize(dim.y());
    for(auto& i : collisions)
    {
        i.resize(dim.x());
    }

    collideables.resize(dim.y());
    for(auto& i : collideables)
    {
        i.resize(dim.x());
    }

    target_dim = target;
}

float cross2d(vec2f p1, vec2f p2)
{
    ///x0*y1 - y0*x1

    return p1.x() * p2.y() - p1.y() * p2.x();
}

///rel is [0 -> 1]

///[0, 2]
///[1, 3]
std::array<frequency_packet, 4> distribute_packet(vec2f rel, frequency_packet packet)
{
    std::array<frequency_packet, 4> ret;

    for(auto& i : ret)
    {
        i = packet;
        i.intensity = 0;
    }

    float x2 = 1;
    float x1 = 0;
    float y2 = 1;
    float y1 = 0;

    float x = rel.x();
    float y = rel.y();

    double div = (x2 - x1) * (y2 - y1);

    double q11 = (x2 - x) * (y2 - y) / div;
    double q21 = (x - x1) * (y2 - y) / div;
    double q12 = (x2 - x) * (y - y1) / div;
    double q22 = (x - x1) * (y - y1) / div;

    ret[0].intensity = q11 * packet.intensity;
    ret[1].intensity = q12 * packet.intensity;
    ret[2].intensity = q21 * packet.intensity;
    ret[3].intensity = q22 * packet.intensity;

    return ret;
}

void radar_field::add_packet_to(std::vector<std::vector<frequencies>>& field, frequency_packet packet, vec2f absolute_location, bool update_origin) const
{
    vec2f relative_loc = (absolute_location - offset) * dim / target_dim;

    if(relative_loc.x() < 1 || relative_loc.y() < 1 || relative_loc.x() >= dim.x() - 1 || relative_loc.y() >= dim.y() - 1)
        return;

    vec2f cell_floor = floor(relative_loc);

    if(update_origin)
    {
        packet.origin = absolute_location;
        packet.id = frequency_packet::gid++;
    }

    std::array<frequency_packet, 4> distributed = distribute_packet(relative_loc - cell_floor, packet);

    double my_freq_frac = (packet.frequency - MIN_FREQ) / (MAX_FREQ - MIN_FREQ);

    int band = my_freq_frac * FREQUENCY_BUCKETS;

    band = clamp(band, 0, FREQUENCY_BUCKETS-1);

    /*freq[(int)cell_floor.y()][(int)cell_floor.x()].buckets[band].packets.push_back(distributed[0]);
    freq[(int)cell_floor.y() + 1][(int)cell_floor.x()].buckets[band].packets.push_back(distributed[1]);
    freq[(int)cell_floor.y()][(int)cell_floor.x() + 1].buckets[band].packets.push_back(distributed[2]);
    freq[(int)cell_floor.y() + 1][(int)cell_floor.x() + 1].buckets[band].packets.push_back(distributed[3]);*/

    add_raw_packet_to(field, distributed[0], cell_floor.x(), cell_floor.y());
    add_raw_packet_to(field, distributed[1], cell_floor.x(), cell_floor.y()+1);
    add_raw_packet_to(field, distributed[2], cell_floor.x()+1, cell_floor.y());
    add_raw_packet_to(field, distributed[3], cell_floor.x()+1, cell_floor.y()+1);

    for(int i=-1; i < 2; i++)
    {
        for(int j=-1; j < 2; j++)
        {
            frequency_packet null_packet = packet;
            null_packet.intensity = 0;

            add_raw_packet_to(field, null_packet, cell_floor.x() + i, cell_floor.y() + j);
        }
    }

    /*field[(int)cell_floor.y()][(int)cell_floor.x()].packets.push_back(distributed[0]);
    field[(int)cell_floor.y() + 1][(int)cell_floor.x()].packets.push_back(distributed[1]);
    field[(int)cell_floor.y()][(int)cell_floor.x() + 1].packets.push_back(distributed[2]);
    field[(int)cell_floor.y() + 1][(int)cell_floor.x() + 1].packets.push_back(distributed[3]);*/

    //std::cout << "packet 2 " << cell_floor << std::endl;
}

void radar_field::add_packet(frequency_packet packet, vec2f absolute_location, bool update_origin)
{
    add_packet_to(freq, packet, absolute_location, update_origin);
}

void radar_field::add_raw_packet_to(std::vector<std::vector<frequencies>>& field, frequency_packet p, int x, int y) const
{
    if(x < 0 || y < 0 || x >= dim.x() || y >= dim.y())
        return;

    /*for(frequency_packet& existing : field[y][x].packets)
    {
        if(existing.frequency == p.frequency && existing.origin == p.origin && existing.iterations == p.iterations && existing.id == p.id)
        {
            existing.intensity += p.intensity;
            return;
        }
    }*/

    if(field[y][x].packets.find(p.id) != field[y][x].packets.end())
    {
        field[y][x].packets[p.id].intensity += p.intensity;
        return;
    }

    //field[y][x].packets.push_back(p);

    field[y][x].packets[p.id] = p;
}

void radar_field::render(sf::RenderWindow& win)
{
    sf::CircleShape circle;
    circle.setRadius(2);
    circle.setOrigin(circle.getRadius(), circle.getRadius());

    for(int y=0; y < (int)freq.size(); y++)
    {
        for(int x=0; x < (int)freq[y].size(); x++)
        {
            vec2f real_pos = index_to_position(x, y);

            //circle.setRadius(1);

            float total_intensity = get_intensity_at(x, y);

            if(total_intensity == 0)
                continue;

            /*for(auto& ppair : collisions[y][x].packets)
            {
                vec2f real = index_to_position(x, y);

                circle.setPosition(real.x(), real.y());
                circle.setRadius(5);

                win.draw(circle);
            }*/

            circle.setRadius(total_intensity);
            circle.setOrigin(circle.getRadius(), circle.getRadius());

            circle.setPosition(real_pos.x(), real_pos.y());

            win.draw(circle);
        }
    }
}

float radar_field::get_intensity_at(int x, int y)
{
    float total_intensity = 0;
    float subtractive_intensity = 0;

    //for(frequency_band& band : freq[y][x].buckets)
    {
        //for(frequency_packet& packet : freq[y][x].packets)

        for(auto& ppair : freq[y][x].packets)
        {
            auto packet = ppair.second;

            total_intensity += packet.intensity;

            /*sf::CircleShape origin;
            origin.setRadius(5);
            origin.setFillColor(sf::Color(255, 0, 0, 255));

            origin.setPosition(packet.origin.x(), packet.origin.y());
            origin.setOrigin(origin.getRadius(), origin.getRadius());

            win.draw(origin);*/
        }

        for(auto& ppair : collisions[y][x].packets)
        {
            subtractive_intensity += ppair.second.intensity;
        }
    }

    if(subtractive_intensity > 0)
        return 0;

    return total_intensity;
}

float rcollideable::get_cross_section(float angle)
{
    return dim.max_elem() * 5;
}

frequency_chart radar_field::tick_raw(double dt_s, frequency_chart& first, bool collides)
{
    frequency_chart next;

    next.resize(dim.y());

    for(auto& i : next)
    {
        i.resize(dim.x());
    }

    float light_distance_per_tick = 1.5;
    float light_propagation = 1;

    ///so
    ///every tick we propagate propagation amount over light distance
    ///so ok. Lets model it as a packet?
    ///won't work

    ///need to update packet distribution model to distribute over 9 tiles instead of 4?

    ///[0,  1,  2]
    ///[3, {4}, 5]
    ///[6,  7,  8]
    ///so we're trying to model propagation at 4
    ///there's also us at 5, 7, and 8
    ///the centre is at (4 + 5 + 7 + 8)/4
    ///this means 4 needs to propagate to 1, 0, and 3 - 1/4 to 1, 1/2 to 0, and 1/4 to 1? or whatever the appropriate geometric distances are
    ///but propagation angle is also affected by the origin, a far away origin means to propagate less
    ///so at an origin distance of sqrt(1 + 1) = root 2, our propagation angle is 90 degrees right, and at infinity our propagation angle is 0 degrees
    ///so essentially propagation angle is the fraction of the circle that we need to propagate through so that the entire circle is covered
    ///so if the distance between

    ///hmmm
    ///ok maybe if we treat each corner as a packet
    ///then find the origin
    ///then move that packet by the propagation amount in the direction away from the origin
    ///add that packet through bilinear interpolation
    ///would that work? I think yes

    ///ok so the problem is we're propagating from each corner, and each time move only a little
    ///we need to treat the actual propagation centre as the actual packet location, which is the bilinear interpolation coordinate
    ///of the real packet
    ///can't do that without each packet being rigidly a 2x2 entity

    ///ok this is really just fluid dynamics so maybe i need to give up and port across one of them
    ///although, maybe we can calculate packet location with origin * deltatime, because there should be an analytic solution here right?
    ///so each point in space represents a piece of the analytic solution that isn't cancelled, ie the point of discretising the grid
    ///is to keep track of collisions

    for(int y=1; y < dim.y() - 2; y++)
    {
        for(int x=1; x < dim.x() - 2; x++)
        {
            //std::vector<frequency_packet>& packs = first[y][x].packets;

            auto& packs = first[y][x].packets;

            //if(packs.size() != 0)
            //    std::cout << "packs " << packs.size() << std::endl;

            vec2f index_position = index_to_position(x, y);

            float packet_wavefront_width = 5.5;

            //for(frequency_packet& pack : packs)
            for(auto& ppair : packs)
            {
                frequency_packet& pack = ppair.second;

                float real_distance = pack.iterations * light_distance_per_tick;
                float my_angle = (index_position - pack.origin).angle();

                vec2f packet_vector = (vec2f){real_distance, 0}.rot(my_angle);

                vec2f packet_position = packet_vector + pack.origin;
                vec2f packet_angle = (vec2f){1, 0}.rot(pack.start_angle);

                if(angle_between_vectors(packet_vector, packet_angle) > pack.restrict_angle && real_distance > 20)
                    continue;

                /*std::optional<vec2f> apos = get_approximate_location(first, {x, y}, pack.id);

                if(apos.has_value())
                    std::cout << "approx of " << *apos << " real " << packet_position << std::endl;*/

                /*std::optional<vec2f> apos = get_approximate_location(first, {x, y}, pack.id);

                if(!apos)
                    continue;

                packet_position = *apos;*/

                float distance_to_real_packet = (index_position - packet_position).length();

                float wavecentre_distance = packet_wavefront_width - distance_to_real_packet;

                //std::cout << "intensity " << intensity << std::endl;

                if((wavecentre_distance) < 0)
                    continue;

                float intensity = wavecentre_distance;

                //std::cout << "intens " << intensity << std::endl;

                if(collides)
                {
                    /*bool make_collide = collideables[y][x];

                    if(make_collide)
                    {
                        frequency_packet cpack = pack;
                        cpack.id = frequency_packet::gid++;

                        cpack.start_angle = my_angle;
                        cpack.restrict_angle = M_PI / 32; ///use real object size

                        collisions[y][x].packets[cpack.id] = cpack;
                    }*/

                    float largest_cross = 0;

                    for(auto& i : collideables[y][x])
                    {
                        largest_cross = std::max(largest_cross, i.get_cross_section(my_angle));
                    }

                    if(collideables[y][x].size() > 0)
                    {
                        std::cout << "hello " << largest_cross << std::endl;
                    }

                    if(largest_cross > 0)
                    {
                        float circle_circumference = 2 * M_PI * real_distance;

                        if(circle_circumference < 0.00001)
                            continue;

                        float my_fraction = largest_cross / circle_circumference;

                        frequency_packet cpack = pack;
                        cpack.id = frequency_packet::gid++;

                        cpack.start_angle = my_angle;
                        cpack.restrict_angle = my_fraction * 2 * M_PI;

                        collisions[y][x].packets[cpack.id] = cpack;
                    }
                }

                /*vec2f origin = pack.origin;

                vec2f propagation_direction = (real_pos - origin).norm();

                vec2f location = real_pos + propagation_direction * light_propagation * light_distance_per_tick;*/

                vec2f propagation_direction = (packet_position - pack.origin).norm();

                vec2f next_location = packet_position + propagation_direction * light_distance_per_tick;

                frequency_packet nextp = pack;

                nextp.intensity = wavecentre_distance;
                //nextp.intensity = 1;
                //nextp.intensity = intensity;
                nextp.iterations++;

                add_packet_to(next, nextp, next_location, false);
            }
        }
    }

    return next;
}

void radar_field::tick(double dt_s)
{
    sf::Clock clk;

    std::vector<std::vector<frequencies>> first = freq;
    std::vector<std::vector<frequencies>> next = tick_raw(dt_s, first, true);

    std::vector<std::vector<frequencies>> next_collide = tick_raw(dt_s, collisions, false);

    freq = next;
    collisions = next_collide;

    std::cout << "elapsed_ms " << clk.getElapsedTime().asMicroseconds() / 1000 << std::endl;

    collideables.clear();

    collideables.resize(dim.y());

    for(auto& i : collideables)
    {
        i.resize(dim.x());
    }
}

std::optional<vec2f> radar_field::get_approximate_location(frequency_chart& chart, vec2f pos, uint32_t packet_id)
{
    if(pos.x() < 1 || pos.y() < 1 || pos.x() >= dim.x() - 2 || pos.y() >= dim.y() - 2)
        return {{0, 0}};

    int ix = floor(pos.x());
    int iy = floor(pos.y());

    frequency_packet& tl = chart[iy][ix].packets[packet_id];
    frequency_packet& tr = chart[iy][ix+1].packets[packet_id];
    frequency_packet& bl = chart[iy+1][ix].packets[packet_id];
    frequency_packet& br = chart[iy+1][ix+1].packets[packet_id];

    ///so... given these corner points, we want to find the point which is 1?
    ///no, we want to do bilinear interpolation where the corners form the weights?

    if(tl.intensity <= 0 && br.intensity <= 0 && bl.intensity <= 0 && tr.intensity <= 0)
        return std::nullopt;

    float tx1_i = tl.intensity;
    float tx2_i = tr.intensity;

    float t_i_frac = 0.5;

    if(fabs(tx1_i + tx2_i) > 0.0001f)
    {
        t_i_frac = (tx1_i) / (tx1_i + tx2_i);
    }

    float bx1_i = bl.intensity;
    float bx2_i = br.intensity;

    float b_i_frac = 0.5;

    if(fabs(bx1_i + bx2_i) > 0.0001f)
        b_i_frac = (bx1_i) / (bx1_i + bx2_i);


    ///so we have a point along the top along the top x axis, and a point along the bottom along the bottom x axis

    ///remember that we want to 1 - these things to get weightings

    ///so, we have one straight line from bottom to top right?

    vec2f top_coordinate = {1.f - t_i_frac, 0};
    vec2f bottom_coordinate = {1.f - b_i_frac, 1};

    float ly1_i = tl.intensity;
    float ly2_i = bl.intensity;

    float l_i_frac = 0.5;

    if(fabs(ly1_i + ly2_i) > 0.0001f)
    {
        l_i_frac = (ly1_i) / (ly1_i + ly2_i);
    }

    float ry1_i = tr.intensity;
    float ry2_i = br.intensity;

    float r_i_frac = 0.5;

    if(fabs(ry1_i + ry2_i) > 0.0001f)
    {
        r_i_frac = (ry1_i) / (ry1_i + ry2_i);
    }

    vec2f left_coordinate = {0, 1 - l_i_frac};
    vec2f right_coordinate = {1, 1 - r_i_frac};


    float x1 = top_coordinate.x();
    float x2 = bottom_coordinate.x();

    float y1 = top_coordinate.y();
    float y2 = bottom_coordinate.y();

    float x3 = left_coordinate.x();
    float x4 = right_coordinate.x();

    float y3 = left_coordinate.y();
    float y4 = right_coordinate.y();


    //printf("%f %f %f %f %f %f %f %f\n", x1, x2, x3, x4, y1, y2, y3, y4);


    float px = ((x1 * y2 - y1 * x2) * (x3 - x4) - (x1 - x2) * (x3 * y4 - y3 * x4)) / ((x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4));
    float py = ((x1 * y2 - y1 * x2) * (y3 - y4) - (y1 - y2) * (x3 * y4 - y3 * x4)) / ((x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4));

    vec2f relative_fraction = (vec2f){px, py} + pos;

    return (relative_fraction * target_dim / dim) + offset;

    //return (vec2f){px, py};

    //vec2f line_1 = bottom_coordinate - top_coordinate;
    //vec2f line_2 = right_coordinate - left_coordinate;
}

vec2f radar_field::index_to_position(int x, int y)
{
    return ((vec2f){(float)x, (float)y} * target_dim / dim) + offset;
}

void radar_field::add_simple_collideable(float angle, vec2f ship_dim, vec2f real_position)
{
    vec2f idx = (real_position - offset) * dim / target_dim;

    int ix = floor(idx.x());
    int iy = floor(idx.y());

    if(ix < 0 || iy < 0 || ix >= dim.x() || iy >= dim.y())
        return;

    rcollideable rcollide;
    rcollide.dim = ship_dim;
    rcollide.angle = angle;

    collideables[iy][ix].push_back(rcollide);
}
