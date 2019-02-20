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

std::map<std::string, double> info_dump;

struct info_dumper
{
    std::string name;

    info_dumper(const std::string& _name) : name(_name)
    {

    }

    void add(double d)
    {
        info_dump[name] += d;
    }
};

struct profile_dumper
{
    sf::Clock clk;
    std::string name;

    static void dump()
    {
        for(auto& i : info_dump)
        {
            std::cout << i.first << " " << i.second << std::endl;
        }

        info_dump.clear();
    }

    profile_dumper(const std::string& _name) : name(_name)
    {

    }

    ~profile_dumper()
    {
        info_dump[name] += clk.getElapsedTime().asMicroseconds() / 1000.;

        //std::cout << name << " " << clk.getElapsedTime().asMicroseconds() / 1000. << std::endl;
    }
};

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

void radar_field::add_packet_to(std::vector<std::vector<frequencies>>& field, const frequency_packet& in_packet, vec2f absolute_location, bool update_origin, bool distribute) const
{
    vec2f relative_loc = (absolute_location - offset) * dim / target_dim;

    if(relative_loc.x() < 1 || relative_loc.y() < 1 || relative_loc.x() >= dim.x() - 1 || relative_loc.y() >= dim.y() - 1)
        return;

    vec2f cell_floor = floor(relative_loc);

    frequency_packet packet = in_packet;

    if(update_origin)
    {
        packet.origin = absolute_location;
        packet.id = frequency_packet::gid++;
    }

    std::array<frequency_packet, 4> distributed;

    if(distribute)
        distributed = distribute_packet(relative_loc - cell_floor, packet);
    else
    {
        for(auto& i : distributed)
        {
            i = packet;
        }
    }

    double my_freq_frac = (packet.frequency - MIN_FREQ) / (MAX_FREQ - MIN_FREQ);

    int band = my_freq_frac * FREQUENCY_BUCKETS;

    band = clamp(band, 0, FREQUENCY_BUCKETS-1);

    /*freq[(int)cell_floor.y()][(int)cell_floor.x()].buckets[band].packets.push_back(distributed[0]);
    freq[(int)cell_floor.y() + 1][(int)cell_floor.x()].buckets[band].packets.push_back(distributed[1]);
    freq[(int)cell_floor.y()][(int)cell_floor.x() + 1].buckets[band].packets.push_back(distributed[2]);
    freq[(int)cell_floor.y() + 1][(int)cell_floor.x() + 1].buckets[band].packets.push_back(distributed[3]);*/

    if(distribute)
    {
        add_raw_packet_to(field, distributed[0], cell_floor.x(), cell_floor.y());
        add_raw_packet_to(field, distributed[1], cell_floor.x(), cell_floor.y()+1);
        add_raw_packet_to(field, distributed[2], cell_floor.x()+1, cell_floor.y());
        add_raw_packet_to(field, distributed[3], cell_floor.x()+1, cell_floor.y()+1);
    }

    std::vector<vec2i> distrib_positions
    {
        {0, 1},
        {0, -1},
        {-1,  0},
        {1,  0}
    };

    //if(!distribute)
    {
        distrib_positions.push_back({1, 1});
        distrib_positions.push_back({1, -1});
        distrib_positions.push_back({-1, 1});
        distrib_positions.push_back({-1, -1});
    }

    /*for(int i=-1; i < 2; i++)
    {
        for(int j=-1; j < 2; j++)
        {*/

    for(vec2i pos : distrib_positions)
    {
        frequency_packet null_packet = packet;

        if(distribute)
            null_packet.intensity = 0;
        else
            null_packet.intensity = packet.intensity;

        add_raw_packet_to(field, null_packet, cell_floor.x() + pos.x(), cell_floor.y() + pos.y());
    }

    /*field[(int)cell_floor.y()][(int)cell_floor.x()].packets.push_back(distributed[0]);
    field[(int)cell_floor.y() + 1][(int)cell_floor.x()].packets.push_back(distributed[1]);
    field[(int)cell_floor.y()][(int)cell_floor.x() + 1].packets.push_back(distributed[2]);
    field[(int)cell_floor.y() + 1][(int)cell_floor.x() + 1].packets.push_back(distributed[3]);*/

    //std::cout << "packet 2 " << cell_floor << std::endl;
}

void radar_field::add_packet(const frequency_packet& packet, vec2f absolute_location, bool update_origin)
{
    add_packet_to(freq, packet, absolute_location, update_origin);
}

void radar_field::add_raw_packet_to(std::vector<std::vector<frequencies>>& field, const frequency_packet& p, int x, int y) const
{
    if(x < 0 || y < 0 || x >= dim.x() || y >= dim.y())
        return;

    //profile_dumper arpt("arpt");

    /*for(frequency_packet& existing : field[y][x].packets)
    {
        if(existing.frequency == p.frequency && existing.origin == p.origin && existing.iterations == p.iterations && existing.id == p.id)
        {
            existing.intensity += p.intensity;
            return;
        }
    }*/

    //std::cout << "psize " << field[y][x].packets.size() << std::endl;



    //info_dumper id("tfield " + std::to_string(y) + "|" + std::to_string(x));

    //id.add(field[y][x].packets.size());

    /*for(auto& i : field[y][x].packets)
    {
        std::cout << "i.first " << i.first << " cnt " << field[y][x].packets.size() << std::endl;
    }*/

    /*if(field[y][x].packets.find(p.id) != field[y][x].packets.end())
    {
        //field[y][x].packets[p.id].intensity += p.intensity;
        return;
    }*/

    for(auto& i : field[y][x].packets)
    {
        if(i.id == p.id)
            return;
    }

    field[y][x].packets.push_back(p);

    //field[y][x].packets[p.id] = p;
}

void radar_field::prune(frequency_chart& in)
{
    //profile_dumper prun("prune");

    for(int y=0; y < dim.y(); y++)
    {
        for(int x=0; x < dim.x(); x++)
        {
            std::map<uint32_t, frequency_packet> good_map;

            for(auto& i : in[y][x].packets)
            {
                if(good_map.find(i.id) != good_map.end())
                    continue;

                good_map[i.id] = i;
            }

            in[y][x].packets.clear();

            for(auto& i : good_map)
            {
                in[y][x].packets.push_back(i.second);
            }
        }
    }
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
            /*for(auto& ppair : collisions[y][x].packets)
            {
                vec2f real = index_to_position(x, y);

                circle.setPosition(real.x(), real.y());
                circle.setRadius(5);

                win.draw(circle);
            }*/

            /*for(auto& ppair : freq[y][x].packets)
            {
                vec2f real = ppair.second.origin;

                circle.setPosition(real.x(), real.y());
                circle.setRadius(5);

                win.draw(circle);
            }*/

            float total_intensity = get_intensity_at(x, y);

            if(total_intensity == 0)
                continue;

            if(total_intensity > 10)
                total_intensity = 10;

            float ffrac = 1;

            if(total_intensity < 1)
            {
                ffrac = total_intensity;
                total_intensity = 1;
            }

            circle.setRadius(total_intensity);
            circle.setOrigin(circle.getRadius(), circle.getRadius());

            circle.setFillColor(sf::Color(255 * ffrac, 255 * ffrac, 255 * ffrac, 255));

            circle.setPosition(real_pos.x(), real_pos.y());

            win.draw(circle);
        }
    }
}

///make sure to time correct this!
float radar_field::get_intensity_at(int x, int y)
{
    float total_intensity = 0;
    //float subtractive_intensity = 0;

    //for(frequency_band& band : freq[y][x].buckets)
    {
        //for(frequency_packet& packet : freq[y][x].packets)

        for(auto& ppair : freq[y][x].packets)
        {
            frequency_packet& packet = ppair;

            bool ignore = false;

            for(auto& spair : collisions[y][x].packets)
            {
                if(spair.collides_with == packet.id)
                {
                    ignore = true;
                    break;
                }
            }

            if(ignore)
                continue;

            vec2f index_position = index_to_position(x, y);

            /*

            vec2f packet_real = packet.origin + light_move_per_tick * packet.iterations;

            float distance_to_packet = (real_position - packet.origin).length();*/

            float real_distance = packet.iterations * light_move_per_tick;
            float my_angle = (index_position - packet.origin).angle();

            vec2f packet_vector = (vec2f){real_distance, 0}.rot(my_angle);

            vec2f packet_position = packet_vector + packet.origin;

            float distance_to_packet = (packet_position - packet.origin).length();

            if(distance_to_packet <= 0.0001)
            {
                total_intensity += packet.intensity;
            }
            else
            {
                total_intensity += packet.intensity / (distance_to_packet * distance_to_packet);
            }


            /*sf::CircleShape origin;
            origin.setRadius(5);
            origin.setFillColor(sf::Color(255, 0, 0, 255));

            origin.setPosition(packet.origin.x(), packet.origin.y());
            origin.setOrigin(origin.getRadius(), origin.getRadius());

            win.draw(origin);*/
        }

        /*for(auto& ppair : collisions[y][x].packets)
        {
            subtractive_intensity += ppair.second.intensity;
        }*/
    }

    //if(subtractive_intensity > 0)
    //    return 0;

    return total_intensity;
}

///make sure to time correct this!
float radar_field::get_intensity_at_of(int x, int y, const frequency_packet& packet)
{
    float total_intensity = 0;

    //frequency_packet& packet = freq[y][x].packets[pid];

    for(auto& spair : collisions[y][x].packets)
    {
        if(spair.collides_with == packet.id)
        {
            return 0;
        }
    }

    vec2f index_position = index_to_position(x, y);

    float real_distance = packet.iterations * light_move_per_tick;
    float my_angle = (index_position - packet.origin).angle();

    vec2f packet_vector = (vec2f){real_distance, 0}.rot(my_angle);

    vec2f packet_position = packet_vector + packet.origin;

    float distance_to_packet = (packet_position - packet.origin).length();

    if(distance_to_packet <= 0.0001)
    {
        total_intensity += packet.intensity;
    }
    else
    {
        total_intensity += packet.intensity / (distance_to_packet * distance_to_packet);
    }

    return total_intensity;
}

float rcollideable::get_cross_section(float angle)
{
    return dim.max_elem() * 5;
}

frequency_chart radar_field::tick_raw(double dt_s, frequency_chart& first, bool collides, uint32_t iterations)
{
    frequency_chart next;

    next.resize(dim.y());

    for(auto& i : next)
    {
        i.resize(dim.x());
    }

    float light_propagation = 1;

    //profile_dumper prop("tick_raw");

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

    profile_dumper dump("Full loop");

    for(int y=1; y < dim.y() - 2; y++)
    {
        for(int x=1; x < dim.x() - 2; x++)
        {
            //std::vector<frequency_packet>& packs = first[y][x].packets;

            auto& packs = first[y][x].packets;

            //if(packs.size() != 0)
            //    std::cout << "packs " << packs.size() << std::endl;

            vec2f index_position = index_to_position(x, y);

            //for(frequency_packet& pack : packs)
            for(auto& ppair : packs)
            {
                //profile_dumper pack_loop("pack_loop");

                frequency_packet& pack = ppair;

                if(collides)
                {
                    //profile_dumper coll_loop("coll_loop");

                    if(get_intensity_at_of(x, y, pack) <= 0.00000001f)
                        continue;
                }

                float real_distance = pack.iterations * light_move_per_tick;
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

                float wavecentre_distance = pack.packet_wavefront_width - distance_to_real_packet;

                //std::cout << "intensity " << intensity << std::endl;

                if((wavecentre_distance) < 0)
                    continue;

                float intensity = wavecentre_distance;


                vec2f propagation_direction = (packet_position - pack.origin).norm();

                vec2f next_location = packet_position + propagation_direction * light_move_per_tick;

                //std::cout << "intens " << intensity << std::endl;

                if(collides)
                {
                    //profile_dumper coll_check("coll_check");

                    /*bool make_collide = collideables[y][x];

                    if(make_collide)
                    {
                        frequency_packet cpack = pack;
                        cpack.id = frequency_packet::gid++;

                        cpack.start_angle = my_angle;
                        cpack.restrict_angle = M_PI / 32; ///use real object size

                        collisions[y][x].packets[cpack.id] = cpack;
                    }*/

                    bool ignore = false;

                    float largest_cross = 0;

                    for(auto& i : collideables[y][x])
                    {
                        /*for(auto& j : pack.ignore_list)
                        {
                            if(j == i.uid)
                            {
                                ignore = true;
                                break;
                            }
                        }*/

                        /*for(auto& j : ignore_map[i.uid])
                        {
                            if(j.skips(pack.id))
                            {
                                ignore = true;
                                break;
                            }

                            if(j.skips(pack.transient_id))
                            {
                                ignore = true;
                                break;
                            }
                        }*/

                        /*if(ignore_map[i.uid].skips(pack.id) || ignore_map[i.uid].skips(pack.transient_id))
                            ignore = true;*/

                        if(ignore_map[i.uid].ignored(pack.id))
                            ignore = true;

                        if(ignore)
                            break;

                        largest_cross = std::max(largest_cross, i.get_cross_section(my_angle));
                    }

                    if(largest_cross > 0 && !ignore)
                    {
                        ///spawn collisions
                        float circle_circumference = 2 * M_PI * real_distance;

                        if(circle_circumference < 0.00001)
                            continue;

                        float my_fraction = largest_cross / circle_circumference;

                        frequency_packet cpack = pack;
                        cpack.id = frequency_packet::gid++;
                        cpack.collides_with = pack.id;

                        cpack.start_angle = my_angle;
                        cpack.restrict_angle = my_fraction * 2 * M_PI;
                        //cpack.iterations++;

                        //collisions[y][x].packets[cpack.id] = cpack;

                        add_packet_to(collisions, cpack, index_position, false);


                        ///spawn reflection
                        /*frequency_packet reflect = pack;
                        reflect.id = frequency_packet::gid++;

                        std::cout << "packets " << packs.size() << std::endl;

                        reflect.start_angle = -my_angle;
                        reflect.restrict_angle = my_fraction * 2 * M_PI;

                        ///opposide side of our current position
                        reflect.origin = pack.origin + packet_vector * 2;
                        //reflect.intensity = wavecentre_distance;
                        reflect.intensity = wavecentre_distance;
                        reflect.iterations++;

                        for(auto& r : collideables[y][x])
                        {
                            reflect.ignore_list.push_back(r.uid);
                        }

                        reflect.iterations = 0;*/

                        frequency_packet reflect = pack;
                        reflect.id = frequency_packet::gid++;
                        //reflect.transient_id = frequency_packet::gid++;

                        ///maybe intensity should be distributed here to avoid energy increase
                        reflect.intensity = pack.intensity;
                        reflect.origin = pack.origin + packet_vector * 2 - packet_vector.norm() * 10;
                        //reflect.start_angle = -my_angle;
                        reflect.start_angle = (index_position - reflect.origin).angle();
                        reflect.restrict_angle = my_fraction * 2 * M_PI;

                        for(auto& r : collideables[y][x])
                        {
                            //ignore_map[r.uid].add(reflect.id);
                            //ignore_map[r.uid].push_back(pack.id);
                            //ignore_map[r.uid].add_for(pack.id, 500);

                            ignore_map[r.uid].ignore(reflect.id);
                            ignore_map[r.uid].ignore(pack.id);

                            //ignore_map[r.uid].push_back(pack.transient_id);

                            //reflect.ignore_list.push_back(r.uid);
                        }

                        //add_packet_to(next, reflect, next_location, false);
                        add_packet_to(next, reflect, packet_position - packet_vector.norm() * 10, false, false);
                        continue;
                    }
                }

                //profile_dumper packet_add("packet_add");

                /*vec2f origin = pack.origin;

                vec2f propagation_direction = (real_pos - origin).norm();

                vec2f location = real_pos + propagation_direction * light_propagation * light_move_per_tick;*/

                frequency_packet nextp = pack;
                //nextp.transient_id = pack.iterations + pow(2, 30);

                //nextp.intensity = wavecentre_distance;
                nextp.intensity = pack.intensity;
                //nextp.intensity = 1;
                //nextp.intensity = intensity;
                nextp.iterations++;

                add_packet_to(next, nextp, next_location, false, false);
            }
        }
    }

    profile_dumper::dump();

    return next;
}

void radar_field::tick(double dt_s, uint32_t iterations)
{
    sf::Clock clk;

    std::vector<std::vector<frequencies>> next = tick_raw(dt_s, freq, true, iterations);

    std::vector<std::vector<frequencies>> next_collide = tick_raw(dt_s, collisions, false, iterations);

    //prune(next);
    //prune(next_collide);

    freq = std::move(next);
    collisions = std::move(next_collide);

    std::cout << "elapsed_ms " << clk.getElapsedTime().asMicroseconds() / 1000 << std::endl;

    collideables.clear();

    collideables.resize(dim.y());

    for(auto& i : collideables)
    {
        i.resize(dim.x());
    }
}

/*std::optional<vec2f> radar_field::get_approximate_location(frequency_chart& chart, vec2f pos, uint32_t packet_id)
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
}*/

vec2f radar_field::index_to_position(int x, int y)
{
    return ((vec2f){(float)x, (float)y} * target_dim / dim) + offset;
}

void radar_field::add_simple_collideable(float angle, vec2f ship_dim, vec2f real_position, uint32_t uid)
{
    vec2f idx = (real_position - offset) * dim / target_dim;

    int ix = floor(idx.x());
    int iy = floor(idx.y());

    if(ix < 0 || iy < 0 || ix >= dim.x() || iy >= dim.y())
        return;

    rcollideable rcollide;
    rcollide.dim = ship_dim;
    rcollide.angle = angle;
    rcollide.uid = uid;

    collideables[iy][ix].push_back(rcollide);
}

float alt_collideable::get_cross_section(float angle)
{
    return dim.max_elem() * 5;
}

float get_physical_cross_section(vec2f dim, float initial_angle, float observe_angle)
{
    return dim.max_elem();
}

float alt_collideable::get_physical_cross_section(float angle)
{
    return dim.max_elem();
}

alt_radar_field::alt_radar_field(vec2f in)
{
    target_dim = in;
}

void alt_radar_field::add_packet(alt_frequency_packet freq, vec2f pos)
{
    freq.origin = pos;
    freq.id = alt_frequency_packet::gid++;

    packets.push_back(freq);
}

void alt_radar_field::add_packet_raw(alt_frequency_packet freq, vec2f pos)
{
    freq.origin = pos;

    packets.push_back(freq);
}

void alt_radar_field::add_simple_collideable(float angle, vec2f dim, vec2f location, uint32_t uid)
{
    alt_collideable rc;
    rc.angle = angle;
    rc.dim = dim;
    rc.uid = uid;
    rc.pos = location;

    collideables.push_back(rc);
}

void alt_radar_field::emit(alt_frequency_packet freq, vec2f pos, uint32_t uid)
{
    freq.id = alt_frequency_packet::gid++;
    freq.emitted_by = uid;

    ignore_map[freq.id][uid].restart();

    add_packet_raw(freq, pos);
}

bool alt_radar_field::packet_expired(alt_frequency_packet& packet)
{
    float real_distance = packet.iterations * speed_of_light_per_tick;

    if(packet.iterations == 0)
        return false;

    float real_intensity = packet.intensity / (real_distance * real_distance);

    return real_intensity < 0.1f;
}

void alt_radar_field::tick(double dt_s, uint32_t iterations)
{
    //profile_dumper pdump("newtick");

    //packets.insert(packets.end(), speculative_packets.begin(), speculative_packets.end());

    /*for(auto& i : speculative_subtractive_packets)
    {
        subtractive_packets[i.first] = i.second;
    }

    speculative_packets.clear();
    speculative_subtractive_packets.clear();*/

    for(alt_frequency_packet& packet : packets)
    {
        float current_radius = packet.iterations * speed_of_light_per_tick;
        float next_radius = (packet.iterations + 1) * speed_of_light_per_tick;

        for(alt_collideable& collide : collideables)
        {
            if(!packet.has_cs && packet.emitted_by == collide.uid)
            {
                packet.cross_dim = collide.dim;
                packet.cross_angle = collide.angle;
                packet.has_cs = true;
            }

            if(ignore_map[packet.id][collide.uid].should_ignore())
                continue;

            vec2f packet_to_collide = collide.pos - packet.origin;
            vec2f packet_angle = (vec2f){1, 0}.rot(packet.start_angle);

            if(angle_between_vectors(packet_to_collide, packet_angle) > packet.restrict_angle)
                continue;

            vec2f relative_pos = collide.pos - packet.origin;

            float len = relative_pos.length();

            float cross_section = collide.get_cross_section(relative_pos.angle());

            cross_section = 0;

            if(len < next_radius + cross_section/2 && len >= current_radius - cross_section/2)
            {
                if(get_intensity_at_of(collide.pos, packet) <= 0.001)
                    continue;

                alt_frequency_packet collide_packet = packet;
                collide_packet.id_block = packet.id;
                collide_packet.id = alt_frequency_packet::gid++;
                //collide_packet.iterations++;
                //collide_packet.emitted_by = -1;

                float circle_circumference = 2 * M_PI * len;

                if(circle_circumference < 0.00001)
                    continue;

                float my_fraction = collide.get_cross_section(relative_pos.angle()) / circle_circumference;

                collide_packet.start_angle = relative_pos.angle();
                collide_packet.restrict_angle = my_fraction * 2 * M_PI;

                subtractive_packets[packet.id].push_back(collide_packet);


                vec2f packet_vector = collide.pos - packet.origin;


                alt_frequency_packet reflect = packet;
                reflect.id = alt_frequency_packet::gid++;

                ///maybe intensity should be distributed here to avoid energy increase
                reflect.intensity = packet.intensity * 0.90;
                reflect.origin = collide.pos + packet_vector;
                reflect.start_angle = (collide.pos - reflect.origin).angle();
                reflect.restrict_angle = my_fraction * 2 * M_PI;
                //reflect.emitted_by = -1;
                reflect.reflected_by = collide.uid;
                //reflect.prev_reflected_by = packet.reflected_by;
                reflect.reflected_position = collide.pos;
                //reflect.last_reflected_position = packet.last_reflected_position;
                //reflect.iterations++;
                reflect.cross_dim = collide.dim;
                reflect.cross_angle = collide.angle;
                reflect.has_cs = true;

                reflect.last_packet = std::make_shared<alt_frequency_packet>(packet);

                //reflect.iterations = ceilf(((collide.pos - reflect.origin).length() + cross_section * 1.1) / speed_of_light_per_tick);

                speculative_packets.push_back(reflect);

                ignore_map[packet.id][collide.uid].restart();
                ignore_map[reflect.id][collide.uid].restart();
            }
        }
    }

    packets.insert(packets.end(), speculative_packets.begin(), speculative_packets.end());

    speculative_packets.clear();

    for(auto it = packets.begin(); it != packets.end();)
    {
        if(packet_expired(*it))
        {
            //std::cout << "erase\n";

            auto f_it = subtractive_packets.find(it->id);

            if(f_it != subtractive_packets.end())
            {
                subtractive_packets.erase(f_it);
            }

            it = packets.erase(it);
        }
        else
        {
            it++;
        }
    }

    //packets.insert(packets.end(), speculative_packets.begin(), speculative_packets.end());

    for(alt_frequency_packet& packet : packets)
    {
        packet.iterations++;
    }

    //for(alt_frequency_packet& packet : subtractive_packets)
    for(auto& i : subtractive_packets)
    {
        for(alt_frequency_packet& packet : i.second)
        {
            packet.iterations++;
        }
    }

    //pdump.dump();

    collideables.clear();

    std::cout << packets.size() << std::endl;

    int num_subtract = 0;

    for(auto& i : packets)
    {
        if(subtractive_packets.find(i.id) == subtractive_packets.end())
            continue;

        num_subtract += subtractive_packets[i.id].size();
    }

    std::cout << "sub " << num_subtract << std::endl;
}

float alt_radar_field::get_intensity_at_of(vec2f pos, alt_frequency_packet& packet)
{
    float real_distance = packet.iterations * speed_of_light_per_tick;
    float my_angle = (pos - packet.origin).angle();

    vec2f packet_vector = (vec2f){real_distance, 0}.rot(my_angle);

    vec2f packet_position = packet_vector + packet.origin;
    vec2f packet_angle = (vec2f){1, 0}.rot(packet.start_angle);

    float distance_to_packet = (pos - packet_position).length();

    float my_distance_to_packet = (pos - packet.origin).length();
    float my_packet_angle = (pos - packet.origin).angle();

    if(angle_between_vectors(packet_vector, packet_angle) > packet.restrict_angle)
        return 0;

    if(subtractive_packets.find(packet.id) != subtractive_packets.end())
    {
        for(alt_frequency_packet& shadow : subtractive_packets[packet.id])
        {
            float shadow_real_distance = shadow.iterations * speed_of_light_per_tick;
            float shadow_next_real_distance = (shadow.iterations + 1) * speed_of_light_per_tick;

            float shadow_my_angle = (pos - shadow.origin).angle();

            vec2f shadow_vector = (vec2f){shadow_real_distance, 0}.rot(shadow_my_angle);
            vec2f shadow_position = shadow_vector + shadow.origin;
            vec2f shadow_angle = (vec2f){1, 0}.rot(shadow.start_angle);

            float distance_to_shadow = (pos - shadow_position).length();

            if(angle_between_vectors(shadow_vector, shadow_angle) > shadow.restrict_angle)
                continue;

            /*if(distance_to_shadow < shadow_next_real_distance && distance_to_shadow >= shadow_real_distance)
            {
                shadowed = true;
                continue;
            }*/

            if(distance_to_shadow <= shadow.packet_wavefront_width)
            {
                return 0;
            }
        }
    }

    if(distance_to_packet > packet.packet_wavefront_width)
    {
        return 0;
    }
    else
    {
        float ivdistance = (packet.packet_wavefront_width - distance_to_packet) / packet.packet_wavefront_width;

        //float err = 0.01;

        float err = 1;

        if(my_distance_to_packet > err)
            return ivdistance * packet.intensity / (my_distance_to_packet * my_distance_to_packet);
        else
            return ivdistance * packet.intensity / (err * err);
    }

    return 0;
}

float alt_radar_field::get_intensity_at(vec2f pos)
{
    float total_intensity = 0;

    for(alt_frequency_packet& packet : packets)
    {
        total_intensity += get_intensity_at_of(pos, packet);
    }

    return total_intensity;
}

void alt_radar_field::render(sf::RenderWindow& win)
{
    #if 0
    for(alt_frequency_packet& packet : packets)
    {
        float real_distance = packet.iterations * speed_of_light_per_tick;

        sf::CircleShape shape;
        shape.setRadius(real_distance);
        shape.setPosition(packet.origin.x(), packet.origin.y());
        shape.setOutlineThickness(packet.packet_wavefront_width);
        shape.setFillColor(sf::Color(0,0,0,0));
        shape.setOutlineColor(sf::Color(255, 255, 255, 255));
        shape.setOrigin(shape.getRadius(), shape.getRadius());
        shape.setPointCount(100);

        win.draw(shape);

        /*for(alt_frequency_packet& shadow : subtractive_packets[packet.id])
        {
            vec2f pos = shadow.origin;

            float distance = shadow.iterations * speed_of_light_per_tick;

            float angle = shadow.start_angle;

            float min_angle = shadow.start_angle - shadow.restrict_angle;
            float max_angle = shadow.start_angle + shadow.restrict_angle;

            vec2f d1 = (vec2f){distance, 0}.rot(min_angle) + pos;
            vec2f d2 = (vec2f){distance, 0}.rot(max_angle) + pos;

            sf::RectangleShape rshape;
            rshape.setSize({(d1 - d2).length(), 5});
            rshape.setRotation(r2d((d2 - d1).angle()));
            rshape.setPosition(d1.x(), d1.y());

            rshape.setFillColor(sf::Color(0,0,0,255));

            win.draw(rshape);
        }*/
    }
    #endif // 0

    /*for(alt_frequency_packet& packet : packets)
    {
        float subdiv =
    }*/

    /*for(alt_frequency_packet& packet : subtractive_packets)
    {
        float real_distance = packet.iterations * speed_of_light_per_tick;

        sf::CircleShape shape;
        shape.setRadius(real_distance);
        shape.setPosition(packet.origin.x(), packet.origin.y());
        shape.setOutlineThickness(packet.packet_wavefront_width);
        shape.setFillColor(sf::Color(0,0,0,0));
        shape.setOutlineColor(sf::Color(128, 128, 128, 128));
        shape.setOrigin(shape.getRadius(), shape.getRadius());
        shape.setPointCount(100);

        win.draw(shape);
    }*/

    /*std::cout << packets.size() << std::endl;

    int num_subtract = 0;

    for(auto& i : packets)
    {
        if(subtractive_packets.find(i.id) == subtractive_packets.end())
            continue;

        num_subtract += subtractive_packets[i.id].size();
    }

    std::cout << "sub " << num_subtract << std::endl;*/

    sf::CircleShape shape;

    for(int y=0; y < target_dim.y(); y+=20)
    {
        for(int x=0; x < target_dim.x(); x+=20)
        {
            float intensity = get_intensity_at({x, y});

            if(intensity == 0)
                continue;

            if(intensity > 5)
                intensity = 5;

            float ffrac = 1;

            if(intensity < 1)
            {
                ffrac = intensity;
                intensity = 1;
            }

            float fcol = 255 * ffrac;

            shape.setRadius(intensity);
            shape.setPosition(x, y);
            shape.setFillColor(sf::Color(fcol, fcol, fcol, fcol));
            shape.setOrigin(shape.getRadius(), shape.getRadius());

            win.draw(shape);
        }
    }
}

struct random_constants
{
    vec2f err_1;
    vec2f err_2;
    float err_3 = 0;
    float err_4 = 0;

    sf::Clock clk;

    void make(std::minstd_rand0& rng)
    {
        float max_error = 10;

        std::uniform_real_distribution<float> dist_err(-max_error, max_error);
        std::uniform_real_distribution<float> angle_err(-M_PI/4, M_PI/4);

        err_1 = {dist_err(rng), dist_err(rng)};
        err_2 = {dist_err(rng), dist_err(rng)};

        err_3 = angle_err(rng);
        err_4 = angle_err(rng);
    }
};

random_constants& get_random_constants_for(uint32_t uid)
{
    static std::map<uint32_t, random_constants> cst;
    static std::minstd_rand0 mrng;

    random_constants& rconst = cst[uid];

    if(rconst.clk.getElapsedTime().asMicroseconds() / 1000. > 200)
    {
        rconst.make(mrng);
        rconst.clk.restart();
    }

    return rconst;
}

alt_radar_sample alt_radar_field::sample_for(vec2f pos, uint32_t uid)
{
    /*if(sample_time[uid].getElapsedTime().asMicroseconds() / 1000. < 500)
    {
        alt_radar_sample sam = cached_samples[uid];
        sam.echo_pos.clear();
        sam.echo_dir.clear();
        sam.receive_dir.clear();

        sam.fresh = false;

        return sam;
    }*/

    sample_time[uid].restart();

    alt_radar_sample s;
    s.location = pos;

    ///need to sum packets first, then iterate them
    ///need to sum packets with the same emitted id and frequency
    random_constants rconst = get_random_constants_for(uid);

    /*std::vector<alt_frequency_packet> merged;

    for(const alt_frequency_packet& packet : packets)
    {
        bool found = false;

        for(alt_frequency_packet& other : merged)
        {
            if(other.emitted_by == packet.emitted_by && other.reflected_by == packet.reflected_by && other.frequency == packet.frequency && other.origin == packet.origin)
            {
                other.intensity += packet.intensity;
                found = true;
                break;
            }
        }

        if(found)
            continue;

        merged.push_back(packet);
    }*/

    ///ok so the problem with merging is that we calculate intensity analytically, which means that issues are caused by naively combining them
    ///need to combine POST intensity merge

    std::vector<alt_frequency_packet> post_intensity_calculate;

    for(alt_frequency_packet packet : packets)
    {
        if(ignore_map.find(packet.id) != ignore_map.end())
        {
            if(ignore_map[packet.id].find(uid) != ignore_map[packet.id].end())
            {
                if(ignore_map[packet.id][uid].should_ignore())
                {
                    if(!packet.last_packet)
                        continue;

                    alt_frequency_packet lpacket = *packet.last_packet;
                    lpacket.iterations = packet.iterations;
                    lpacket.id = packet.id;

                    packet = lpacket;
                }
            }
        }

        float intensity = get_intensity_at_of(pos, packet);

        packet.intensity = intensity;

        post_intensity_calculate.push_back(packet);
    }

    ///could get around the jitter by sending intensity as frequently as we update randomness
    ///aka move to a frequency sampling basis
    /*std::vector<alt_frequency_packet> merged;

    for(const alt_frequency_packet& packet : post_intensity_calculate)
    {
        bool found = false;

        for(alt_frequency_packet& other : merged)
        {
            if(other.emitted_by == packet.emitted_by && other.reflected_by == packet.reflected_by && other.frequency == packet.frequency && other.origin == packet.origin)
            {
                other.intensity += packet.intensity;
                found = true;
                break;
            }
        }

        if(found)
            continue;

        merged.push_back(packet);
    }*/

    auto merged = post_intensity_calculate;

    for(alt_frequency_packet& packet : merged)
    {
        if(packet.emitted_by == uid && packet.reflected_by == -1)
            continue;

        ///ASSUMES THAT PARENT PACKET HAS SAME INTENSITY AS PACKET UNDER CONSIDERATION
        ///THIS WILL NOT BE TRUE IF I MAKE IT DECREASE IN INTENSITY AFTER A REFLECTION SO THIS IS KIND OF WEIRD
        float intensity = packet.intensity;
        //float intensity = get_intensity_at_of(pos, packet);
        float frequency = packet.frequency;

        if(intensity == 0)
            continue;

        /*uint32_t reflected_by = packet.reflected_by;
        vec2f reflected_position = packet.reflected_position;

        if(reflected_by == uid)
        {
            reflected_by = packet.prev_reflected_by;
            reflected_position = packet.last_reflected_position;
        }*/

        alt_frequency_packet consider = packet;

        /*if(consider.reflected_by == uid && consider.last_packet)
        {
            consider = *consider.last_packet;
        }*/

        //std::cout << intensity << std::endl;

        float uncertainty = intensity / BEST_UNCERTAINTY;
        uncertainty = 1 - clamp(uncertainty, 0, 1);

        #define RECT
        #ifdef RECT
        if(consider.emitted_by == uid && consider.reflected_by != -1 && consider.reflected_by != uid && intensity > 1)
        {
            /*s.echo_position.push_back(packet.reflected_position);
            s.echo_id.push_back(packet.reflected_by);*/

            vec2f next_pos = consider.reflected_position + rconst.err_1 * uncertainty;

            float cs = get_physical_cross_section(consider.cross_dim, consider.cross_angle, (next_pos - pos).angle());

            s.echo_pos.push_back({consider.emitted_by, consider.reflected_by, next_pos, consider.frequency, cs});
        }
        #endif // RECT

        #define RECT_RECV
        #ifdef RECT_RECV
        if(consider.emitted_by != uid && consider.reflected_by == -1 && intensity > 1)
        {
            /*s.echo_position.push_back(packet.reflected_position);
            s.echo_id.push_back(packet.reflected_by);*/

            vec2f next_pos = consider.origin + rconst.err_2 * uncertainty;

            float cs = get_physical_cross_section(consider.cross_dim, consider.cross_angle, (next_pos - pos).angle());

            s.echo_pos.push_back({consider.emitted_by, consider.reflected_by, next_pos, consider.frequency, cs});
        }
        #endif // RECT_RECV

        ///specifically excludes self because we never need to know where we are
        if(consider.emitted_by == uid && consider.reflected_by != -1 && consider.reflected_by != uid && intensity > 0)
        {
            vec2f next_dir = (consider.reflected_position - pos).norm();

            next_dir = next_dir.rot(rconst.err_3 * uncertainty);

            s.echo_dir.push_back({consider.emitted_by, consider.reflected_by, next_dir * intensity, consider.frequency, 0});
        }

        if(consider.emitted_by != uid && consider.reflected_by == -1 && intensity > 0)
        {
            vec2f next_dir = (consider.origin - pos).norm();

            next_dir = next_dir.rot(rconst.err_4 * uncertainty);

            s.receive_dir.push_back({consider.emitted_by, consider.reflected_by, next_dir.norm() * intensity, consider.frequency, 0});

            //std::cout << "sent intens " << intensity << std::endl;
        }

        //std::cout << "intens " << intensity << " freq " << frequency << " emmitted " << consider.emitted_by << " ref " << consider.reflected_by << std::endl;

        //std::cout << "loop " << intensity << std::endl;

        bool found = false;

        for(int i=0; i < (int)s.frequencies.size(); i++)
        {
            if(s.frequencies[i] == frequency)
            {
                s.intensities[i] += intensity;
                found = true;
                break;
            }
        }

        if(!found)
        {
            s.frequencies.push_back(frequency);
            s.intensities.push_back(intensity);
        }
    }

    /*for(auto& i : s.intensities)
    {
        std::cout << "rintens " << i << std::endl;
    }*/

    s.fresh = true;
    cached_samples[uid] = s;

    return s;
}
