#include "radar_field.hpp"
#include <SFML/Graphics.hpp>

radar_field::radar_field(vec2f target)
{
    freq.resize(dim.y());
    for(auto& i : freq)
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
        packet.origin = absolute_location;

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

    for(frequency_packet& existing : field[y][x].packets)
    {
        if(existing.frequency == p.frequency && existing.origin == p.origin && existing.iterations == p.iterations)
        {
            existing.intensity += p.intensity;
            return;
        }
    }

    field[y][x].packets.push_back(p);
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

            float total_intensity = 0;

            //for(frequency_band& band : freq[y][x].buckets)
            {
                for(frequency_packet& packet : freq[y][x].packets)
                {
                    total_intensity += packet.intensity;

                    /*sf::CircleShape origin;
                    origin.setRadius(5);
                    origin.setFillColor(sf::Color(255, 0, 0, 255));

                    origin.setPosition(packet.origin.x(), packet.origin.y());
                    origin.setOrigin(origin.getRadius(), origin.getRadius());

                    win.draw(origin);*/
                }
            }

            if(total_intensity == 0)
                continue;

            //circle.setRadius(total_intensity);

            circle.setPosition(real_pos.x(), real_pos.y());
            //circle.setOrigin(circle.getRadius(), circle.getRadius());

            win.draw(circle);
        }
    }
}

void radar_field::tick(double dt_s)
{
    std::vector<std::vector<frequencies>> first = freq;
    std::vector<std::vector<frequencies>> next;

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
            std::vector<frequency_packet>& packs = first[y][x].packets;

            //if(packs.size() != 0)
            //    std::cout << "packs " << packs.size() << std::endl;

            vec2f index_position = index_to_position(x, y);

            float packet_wavefront_width = 0.5;

            for(frequency_packet& pack : packs)
            {
                float real_distance = pack.iterations * light_distance_per_tick;
                float my_angle = (index_position - pack.origin).angle();

                vec2f packet_position = (vec2f){real_distance, 0}.rot(my_angle) + pack.origin;

                float distance_to_real_packet = (index_position - packet_position).length();

                float intensity = packet_wavefront_width - distance_to_real_packet;

                //std::cout << "intensity " << intensity << std::endl;

                if(fabs(intensity) < 0)
                    continue;



                /*vec2f origin = pack.origin;

                vec2f propagation_direction = (real_pos - origin).norm();

                vec2f location = real_pos + propagation_direction * light_propagation * light_distance_per_tick;*/

                vec2f propagation_direction = (packet_position - pack.origin).norm();

                vec2f next_location = packet_position + propagation_direction * light_distance_per_tick;

                frequency_packet nextp = pack;

                nextp.intensity = 2;
                //nextp.intensity = intensity;
                nextp.iterations++;

                add_packet_to(next, nextp, next_location, false);
            }
        }
    }

    freq = next;
}

vec2f radar_field::index_to_position(int x, int y)
{
    return ((vec2f){(float)x, (float)y} * target_dim / dim) + offset;
}
