#include "radar_field.hpp"
#include <SFML/Graphics.hpp>
#include <set>
#include "player.hpp"

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
    bool stopped = false;

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
        if(stopped)
            return;

        info_dump[name] += clk.getElapsedTime().asMicroseconds() / 1000.;

        //std::cout << name << " " << clk.getElapsedTime().asMicroseconds() / 1000. << std::endl;
    }

    void stop()
    {
        if(stopped)
            return;

        stopped = true;

        info_dump[name] += clk.getElapsedTime().asMicroseconds() / 1000.;
    }
};

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

void alt_radar_field::add_simple_collideable(heatable_entity* en)
{
    assert(en);

    if(en->cleanup)
        return;

    alt_collideable rc;
    rc.angle = en->r.rotation;
    rc.dim = en->r.approx_dim;
    rc.uid = en->id;
    rc.pos = en->r.position;
    rc.en = en;

    collideables.push_back(rc);
}

void alt_radar_field::emit(alt_frequency_packet freq, vec2f pos, uint32_t uid)
{
    freq.id = alt_frequency_packet::gid++;
    freq.emitted_by = uid;

    ignore_map[freq.id][uid].restart();

    add_packet_raw(freq, pos);
}

void alt_radar_field::emit_with_imaginary_packet(alt_frequency_packet freq, vec2f pos, uint32_t uid, player_model* model)
{
    assert(model != nullptr);

    freq.id = alt_frequency_packet::gid++;
    freq.emitted_by = uid;

    ignore_map[freq.id][uid].restart();

    freq.origin = pos;

    packets.push_back(freq);

    freq.id = alt_frequency_packet::gid++;

    ignore_map[freq.id][uid].restart();

    imaginary_packets.push_back(freq);
    imaginary_collideable_list[freq.id] = model;
}

void alt_radar_field::add_player_model(player_model* model)
{
    models.push_back(model);
}

bool alt_radar_field::packet_expired(const alt_frequency_packet& packet)
{
    if(packet.force_cleanup)
        return true;

    float real_distance = packet.iterations * speed_of_light_per_tick;

    if(packet.iterations == 0)
        return false;

    float real_intensity = packet.intensity / (real_distance * real_distance);

    return real_intensity < RADAR_CUTOFF;
}

std::optional<reflect_info>
alt_radar_field::test_reflect_from(alt_frequency_packet& packet, alt_collideable& collide, std::map<uint32_t, std::vector<alt_frequency_packet>>& subtractive)
{
    float current_radius = packet.iterations * speed_of_light_per_tick;
    float next_radius = (packet.iterations + 1) * speed_of_light_per_tick;

    if(!packet.has_cs && packet.emitted_by == collide.uid)
    {
        packet.cross_dim = collide.dim;
        packet.cross_angle = collide.angle;
        packet.has_cs = true;
    }

    vec2f relative_pos = collide.pos - packet.origin;

    float len_sq = relative_pos.squared_length();

    if(len_sq >= next_radius*next_radius || len_sq < current_radius*current_radius)
        return std::nullopt;

    vec2f packet_to_collide = collide.pos - packet.origin;
    vec2f packet_angle = (vec2f){1, 0}.rot(packet.start_angle);

    if(angle_between_vectors(packet_to_collide, packet_angle) > packet.restrict_angle)
        return std::nullopt;

    float cross_section = collide.get_cross_section(relative_pos.angle());

    cross_section = 0;

    float len = sqrtf(len_sq);

    if(len < next_radius + cross_section/2 && len >= current_radius - cross_section/2)
    {
        if(ignore_map[packet.id][collide.uid].should_ignore())
            return std::nullopt;

        float local_intensity = get_intensity_at_of(collide.pos, packet, subtractive);

        if(local_intensity <= 0.001)
            return std::nullopt;

        float circle_circumference = 2 * M_PI * len;

        if(circle_circumference < 0.00001)
            return std::nullopt;

        float my_fraction = collide.get_cross_section(relative_pos.angle()) / circle_circumference;

        vec2f packet_vector = collide.pos - packet.origin;

        alt_frequency_packet collide_packet = packet;
        collide_packet.id_block = packet.id;
        collide_packet.id = alt_frequency_packet::gid++;
        //collide_packet.emitted_by = -1;

        collide_packet.start_angle = relative_pos.angle();
        collide_packet.restrict_angle = my_fraction * 2 * M_PI;

        //#define NO_DOUBLE_REFLECT
        #ifdef NO_DOUBLE_REFLECT
        if(packet.reflected_by != -1)
            return {{std::nullopt, collide_packet}};
        #endif // NO_DOUBLE_REFLECT

        if(collide.en && packet.frequency == HEAT_FREQ)
        {
            ///only absorb 10% of heat? we only reflect 90%
            collide.en->latent_heat += local_intensity * time_between_ticks_s * 0.1;
        }

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

        ignore_map[packet.id][collide.uid].restart();
        ignore_map[reflect.id][collide.uid].restart();

        //return {{std::nullopt, collide_packet}};

        return {{reflect, collide_packet}};
    }

    return std::nullopt;
}

std::vector<uint32_t> clean_old_packets(alt_radar_field& field, std::vector<alt_frequency_packet>& packets, std::map<uint32_t, std::vector<alt_frequency_packet>>& subtractive_packets)
{
    std::vector<uint32_t> ret;

    ///right of course, remove_if leaves the elements in unspecified state
    ///its faster to iterate this twice and use erase remove than iterate once with stable partition

    auto packet_expired = [&](const alt_frequency_packet& in)
                           {
                                return field.packet_expired(in);
                           };

    for(auto it = packets.begin(); it != packets.end(); it++)
    {
        if(field.packet_expired(*it))
        {
            auto f_it = subtractive_packets.find(it->id);

            if(f_it != subtractive_packets.end())
            {
                subtractive_packets.erase(f_it);
            }

            ret.push_back(it->id);
        }
    }

    packets.erase(std::remove_if(packets.begin(), packets.end(), packet_expired), packets.end());

    return ret;
}

vec2f alt_aggregate_collideables::calc_avg()
{
    vec2f fmin = {FLT_MAX, FLT_MAX};
    vec2f fmax = {-FLT_MAX, -FLT_MAX};

    if(collide.size() == 0)
        return {0,0};

    for(auto& i : collide)
    {
        fmin = min(fmin, i.pos);
        fmax = max(fmax, i.pos);
    }

    return ((fmax + fmin) / 2.f);
}

vec2f alt_aggregate_collideables::calc_half_dim()
{
    vec2f fmin = {FLT_MAX, FLT_MAX};
    vec2f fmax = {-FLT_MAX, -FLT_MAX};

    if(collide.size() == 0)
        return {0,0};

    for(auto& i : collide)
    {
        fmin = min(fmin, i.pos);
        fmax = max(fmax, i.pos);
    }

    return ((fmax - fmin) / 2.f);
}

all_alt_aggregate_collideables aggregate_collideables(const std::vector<alt_collideable>& collideables, int num_groups)
{
    all_alt_aggregate_collideables ret;

    if(collideables.size() == 0)
        return ret;

    std::vector<int> used;
    used.resize(collideables.size());

    /*used[0] = 1;

    alt_aggregate_collideables next;
    next.collide.push_back(collideables[0]);*/

    alt_aggregate_collideables next;

    int num_per_group = ceilf((float)collideables.size() / num_groups);

    for(int ng=0; ng < num_groups; ng++)
    {
        for(int ielem = 0; ielem < (int)collideables.size(); ielem++)
        {
            if(used[ielem])
                continue;

            used[ielem] = 1;

            next.collide.push_back(collideables[ielem]);
            break;
        }

        vec2f tavg = next.calc_avg();

        for(int kk=0; kk < num_per_group; kk++)
        {
            float nearest_dist = FLT_MAX;
            int nearest_elem = -1;

            for(int ielem = 0; ielem < (int)collideables.size(); ielem++)
            {
                if(used[ielem])
                    continue;

                ///MANHATTEN DISTANCE
                float next_man = (collideables[ielem].pos - tavg).sum_absolute();

                if(next_man < nearest_dist)
                {
                    nearest_dist = next_man;
                    nearest_elem = ielem;
                }
            }

            if(nearest_elem != -1)
            {
                next.collide.push_back(collideables[nearest_elem]);
                used[nearest_elem] = 1;
            }
            else
            {
                break;
            }
        }

        next.pos = next.calc_avg();
        next.half_dim = next.calc_half_dim();

        ret.aggregate.push_back(next);
        next = alt_aggregate_collideables();
    }

    for(auto& i : used)
    {
        assert(i);
    }

    return ret;
}

void all_alt_aggregate_collideables::get_collideables(alt_radar_field& field, alt_frequency_packet& packet, std::vector<alt_collideable>& out)
{
    out.clear();

    float current_radius = packet.iterations * field.speed_of_light_per_tick;
    float next_radius = (packet.iterations + 1) * field.speed_of_light_per_tick;

    for(alt_aggregate_collideables& agg : aggregate)
    {
        if(rect_intersects_doughnut(agg.pos, agg.half_dim, packet.origin, current_radius, next_radius))
        {
            for(alt_collideable& coll : agg.collide)
            {
                out.push_back(coll);
            }
        }
    }
}

struct alt_aggregate_packet
{
    vec2f pos = {0,0};
    vec2f half_dim;

    std::vector<alt_frequency_packet> packets;

    vec2f calc_avg(alt_radar_field& field)
    {
        vec2f fmin = {FLT_MAX, FLT_MAX};
        vec2f fmax = {-FLT_MAX, -FLT_MAX};

        if(packets.size() == 0)
            return {0,0};

        for(auto& i : packets)
        {
            float next_rad = (i.iterations + 1) * field.speed_of_light_per_tick;

            fmin = min(fmin, i.origin - (vec2f){next_rad, next_rad});
            fmax = max(fmax, i.origin + (vec2f){next_rad, next_rad});
        }

        return ((fmax + fmin) / 2.f);
    }

    vec2f calc_half_dim(alt_radar_field& field)
    {
        vec2f fmin = {FLT_MAX, FLT_MAX};
        vec2f fmax = {-FLT_MAX, -FLT_MAX};

        if(packets.size() == 0)
            return {0,0};

        for(auto& i : packets)
        {
            float next_rad = (i.iterations + 1) * field.speed_of_light_per_tick;

            fmin = min(fmin, i.origin - (vec2f){next_rad, next_rad});
            fmax = max(fmax, i.origin + (vec2f){next_rad, next_rad});
        }

        return ((fmax - fmin) / 2.f);
    }
};

struct all_alt_aggregate_packets
{
    std::vector<alt_aggregate_packet> packets;
};

all_alt_aggregate_packets aggregate_packets(const std::vector<alt_frequency_packet>& packets, int num_groups, alt_radar_field& field)
{
    all_alt_aggregate_packets ret;

    if(packets.size() == 0)
        return ret;

    std::vector<int> used;
    used.resize(packets.size());

    /*used[0] = 1;

    alt_aggregate_collideables next;
    next.collide.push_back(collideables[0]);*/

    alt_aggregate_packet next;

    int num_per_group = ceilf((float)packets.size() / num_groups);

    for(int ng=0; ng < num_groups; ng++)
    {
        for(int ielem = 0; ielem < (int)packets.size(); ielem++)
        {
            if(used[ielem])
                continue;

            used[ielem] = 1;

            next.packets.push_back(packets[ielem]);
            break;
        }

        vec2f tavg = next.calc_avg(field);

        for(int kk=0; kk < num_per_group; kk++)
        {
            float nearest_dist = FLT_MAX;
            int nearest_elem = -1;

            for(int ielem = 0; ielem < (int)packets.size(); ielem++)
            {
                if(used[ielem])
                    continue;

                ///MANHATTEN DISTANCE
                float next_man = (packets[ielem].origin - tavg).sum_absolute() + (packets[ielem].iterations + 1) * field.speed_of_light_per_tick;

                if(next_man < nearest_dist)
                {
                    nearest_dist = next_man;
                    nearest_elem = ielem;
                }
            }

            if(nearest_elem != -1)
            {
                next.packets.push_back(packets[nearest_elem]);
                used[nearest_elem] = 1;
            }
            else
            {
                break;
            }
        }

        next.pos = next.calc_avg(field);
        next.half_dim = next.calc_half_dim(field);

        ret.packets.push_back(next);
        next = alt_aggregate_packet();
    }

    for(auto& i : used)
    {
        assert(i);
    }

    return ret;
}

void alt_radar_field::tick(double dt_s, uint32_t iterations)
{
    profile_dumper pdump("newtick");

    //packets.insert(packets.end(), speculative_packets.begin(), speculative_packets.end());

    /*for(auto& i : speculative_subtractive_packets)
    {
        subtractive_packets[i.first] = i.second;
    }

    speculative_packets.clear();
    speculative_subtractive_packets.clear();*/

    auto next_subtractive = decltype(subtractive_packets)();

    #if 1
    profile_dumper build_time("btime");
    all_alt_aggregate_collideables aggregates = aggregate_collideables(collideables, 30);
    build_time.stop();

    std::vector<alt_collideable> coll_out;

    //profile_dumper exec_time("etime");

    for(alt_frequency_packet& packet : packets)
    {
        /*aggregates.get_collideables(*this, packet, coll_out);

        //std::cout << "colls " << coll_out.size() << " real " << collideables.size() << std::endl;

        //for(alt_collideable& collide : collideables)
        for(alt_collideable& collide : coll_out)
        {
            std::optional<reflect_info> reflected = test_reflect_from(packet, collide, subtractive_packets);

            if(reflected)
            {
                reflect_info inf = reflected.value();

                ///should really make these changes pending so it doesn't affect future results, atm its purely ordering dependent
                ///which will affect compat with imaginary shadows

                if(inf.collide)
                    next_subtractive[packet.id].push_back(inf.collide.value());

                if(inf.reflect)
                    speculative_packets.push_back(inf.reflect.value());
            }
        }*/


        float current_radius = packet.iterations * speed_of_light_per_tick;
        float next_radius = (packet.iterations + 1) * speed_of_light_per_tick;

        for(alt_aggregate_collideables& agg : aggregates.aggregate)
        {
            if(rect_intersects_doughnut(agg.pos, agg.half_dim, packet.origin, current_radius, next_radius))
            {
                for(alt_collideable& collide : agg.collide)
                {
                    std::optional<reflect_info> reflected = test_reflect_from(packet, collide, subtractive_packets);

                    if(reflected)
                    {
                        reflect_info inf = reflected.value();

                        ///should really make these changes pending so it doesn't affect future results, atm its purely ordering dependent
                        ///which will affect compat with imaginary shadows

                        if(inf.collide)
                            next_subtractive[packet.id].push_back(inf.collide.value());

                        if(inf.reflect)
                            speculative_packets.push_back(inf.reflect.value());
                    }
                }
            }
        }
    }

    //exec_time.stop();
    #endif // 0

    //all_alt_aggregate_packets agg_packets = aggregate_packets(packets, 100, *this);

    ///ok alternate optimisation design
    ///say we divide up the map into physx squares (modulo the position the position to find the bucket)
    ///then for each square we find the maximum packet radius and store it there

    ///won't work
    ///ok maybe do the same chunking system, take the 10 nearest packets, find the bounding box, and then go backwards and check
    ///collideables against the bounding boxes
    ///generally there's a lot more packets than collideables so should provide a substantial perf increase
    ///other than the has_cs thing which might be able to die, there's no need to touch a packet other than incrementing tick (which could be done away with)
    ///that turns it to: one pass over packets, collideables * log(n) packets
    ///this is much better than log(n) collideables * packets


    //#define DEBUG_AGGREGATE
    #ifdef DEBUG_AGGREGATE
    for(alt_aggregate_collideables& agg : aggregates.aggregate)
    {
        sf::RectangleShape shape;
        shape.setSize({agg.half_dim.x()*2, agg.half_dim.y()*2});
        shape.setPosition(agg.pos.x(), agg.pos.y());
        shape.setOrigin(shape.getSize().x/2, shape.getSize().y/2);
        shape.setFillColor(sf::Color::Transparent);
        shape.setOutlineThickness(2);
        shape.setOutlineColor(sf::Color::White);

        debug.draw(shape);
    }
    #endif // DEBUG_AGGREGATE

    for(auto& i : next_subtractive)
    {
        subtractive_packets[i.first] = i.second;
    }

    auto next_imaginary_subtractive = decltype(subtractive_packets)();

    std::vector<alt_frequency_packet> imaginary_speculative_packets;

    ///ok so here's where it gets a bit crazy
    ///ships or anything which performs 'realistic' persistent tracking emit 'fake' packets that reflect off the 'fake' objects they have stored
    ///if a ship gets a reflection from this fake packet that does not meet the real expectations, the object does not exist
    ///will need to update this down the line to have fake collideables themselves emit fake radiation
    ///this can be a simple cut down model - only one reflection

    int icollide = 0;

    //#define NO_MULTI_REFLECT_IMAGINARY

    for(alt_frequency_packet& packet : imaginary_packets)
    {
        player_model* player = imaginary_collideable_list[packet.id];

        #ifdef NO_MULTI_REFLECT_IMAGINARY
        if(player == nullptr)
            continue;
        #endif // NO_MULTI_REFLECT_IMAGINARY

        assert(player != nullptr);

        for(auto& [pid, detailed] : player->renderables)
        {
            alt_collideable collide;
            collide.dim = detailed.r.approx_dim;
            collide.angle = detailed.r.rotation;
            collide.uid = pid;
            collide.pos = detailed.r.position;
            collide.en = nullptr; ///nope! this is imaginary thanks

            auto reflected = test_reflect_from(packet, collide, imaginary_subtractive_packets);

            if(reflected)
            {
                reflect_info inf = reflected.value();

                ///should really make these changes pending so it doesn't affect future results, atm its purely ordering dependent
                ///which will affect compat with imaginary shadows

                if(inf.collide)
                    next_imaginary_subtractive[packet.id].push_back(inf.collide.value());

                if(inf.reflect)
                    imaginary_speculative_packets.push_back(inf.reflect.value());

                #ifndef NO_MULTI_REFLECT_IMAGINARY
                if(inf.reflect)
                    imaginary_collideable_list[inf.reflect.value().id] = player;
                #endif // NO_MULTI_REFLECT_IMAGINARY
            }

            icollide++;
        }
    }

    for(auto& i : next_imaginary_subtractive)
    {
        imaginary_subtractive_packets[i.first] = i.second;
    }

    packets.insert(packets.end(), speculative_packets.begin(), speculative_packets.end());
    imaginary_packets.insert(imaginary_packets.end(), imaginary_speculative_packets.begin(), imaginary_speculative_packets.end());

    speculative_packets.clear();
    imaginary_speculative_packets.clear();

    clean_old_packets(*this, packets, subtractive_packets);
    std::vector<uint32_t> to_imaginary_clean = clean_old_packets(*this, imaginary_packets, imaginary_subtractive_packets);

    for(auto& i : to_imaginary_clean)
    {
        if(imaginary_collideable_list.find(i) != imaginary_collideable_list.end())
        {
            imaginary_collideable_list.erase(imaginary_collideable_list.find(i));
        }
    }

    //std::cout << "ipackets " << icollide << std::endl;

    //packets.insert(packets.end(), speculative_packets.begin(), speculative_packets.end());


    for(alt_frequency_packet& packet : packets)
    {
        packet.iterations++;
    }

    for(auto& i : subtractive_packets)
    {
        for(alt_frequency_packet& packet : i.second)
        {
            packet.iterations++;
        }
    }


    for(alt_frequency_packet& packet : imaginary_packets)
    {
        packet.iterations++;
    }

    for(auto& i : imaginary_subtractive_packets)
    {
        for(alt_frequency_packet& packet : i.second)
        {
            packet.iterations++;
        }
    }

    //pdump.dump();
    pdump.stop();
    profile_dumper::dump();

    collideables.clear();

    std::cout << "mpackets " << packets.size() << std::endl;

    /*int num_subtract = 0;

    for(auto& i : packets)
    {
        if(subtractive_packets.find(i.id) == subtractive_packets.end())
            continue;

        num_subtract += subtractive_packets[i.id].size();
    }

    std::cout << "sub " << num_subtract << std::endl;*/
}

float alt_radar_field::get_intensity_at_of(vec2f pos, alt_frequency_packet& packet, std::map<uint32_t, std::vector<alt_frequency_packet>>& subtractive) const
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

    if(subtractive.find(packet.id) != subtractive.end())
    {
        for(alt_frequency_packet& shadow : subtractive[packet.id])
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
        total_intensity += get_intensity_at_of(pos, packet, subtractive_packets);
    }

    return total_intensity;
}

float alt_radar_field::get_imaginary_intensity_at(vec2f pos)
{
    float total_intensity = 0;

    for(alt_frequency_packet& packet : imaginary_packets)
    {
        total_intensity += get_intensity_at_of(pos, packet, imaginary_subtractive_packets);
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
            //float intensity = get_imaginary_intensity_at({x, y});

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

alt_radar_sample alt_radar_field::sample_for(vec2f pos, uint32_t uid, entity_manager& entities, std::optional<player_model*> player)
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

        float intensity = get_intensity_at_of(pos, packet, subtractive_packets);

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

    std::set<uint32_t> considered_packets;
    std::set<uint32_t> pseudo_packets;

    for(alt_frequency_packet packet : imaginary_packets)
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

        float intensity = get_intensity_at_of(pos, packet, imaginary_subtractive_packets);

        packet.intensity = intensity;

        uint64_t search_entity = packet.emitted_by;

        if(packet.reflected_by != -1)
            search_entity = packet.reflected_by;

        if(packet.emitted_by == uid && packet.reflected_by == -1)
            continue;

        if(packet.intensity >= 0.01)
            pseudo_packets.insert(search_entity);
    }

    //std::cout << "pseudo size " << pseudo_packets.size() << std::endl;

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

        uint64_t search_entity = packet.emitted_by;

        if(packet.reflected_by != -1)
            search_entity = packet.reflected_by;

        if(intensity >= 0.01f)
            considered_packets.insert(search_entity);

        /*if(consider.reflected_by == uid && consider.last_packet)
        {
            consider = *consider.last_packet;
        }*/

        //std::cout << intensity << std::endl;

        float uncertainty = intensity / BEST_UNCERTAINTY;
        uncertainty = 1 - clamp(uncertainty, 0, 1);

        #if 1
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
        #endif // 0

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

    if(player)
    {
        std::set<uint32_t> high_detail_entities;
        std::set<uint32_t> low_detail_entities;
        std::set<uint32_t> all_entities;

        for(alt_frequency_packet& packet : merged)
        {
            if(packet.emitted_by == uid && packet.reflected_by == -1)
                continue;

            float intensity = packet.intensity;

            if(intensity == 0)
                continue;

            uint64_t search_entity = packet.emitted_by;

            if(packet.reflected_by != -1)
                search_entity = packet.reflected_by;

            if(intensity >= 1)
            {
                high_detail_entities.insert(search_entity);
                all_entities.insert(search_entity);
            }
            else if(intensity >= 0.01)
            {
                low_detail_entities.insert(search_entity);
                all_entities.insert(search_entity);
            }
        }

        ///initialise player collide
        {
            std::optional<entity*> e = entities.fetch(uid);

            if(e)
            {
                common_renderable play;
                play.type = 1;
                play.r = e.value()->r;
                play.velocity = e.value()->velocity;

                player.value()->renderables[uid] = play;
            }
        }

        for(uint32_t id : low_detail_entities)
        {
            if(id != uid)
            {
                client_renderable rs;
                entity* found = nullptr;

                for(entity* e : entities.entities)
                {
                    if(e->id == id)
                    {
                        rs = e->r;
                        found = e;
                        break;
                    }
                }

                if(found && rs.vert_dist.size() >= 3)
                {
                    if(player.value()->renderables.find(id) != player.value()->renderables.end())
                    {
                        common_renderable& cr = player.value()->renderables[id];

                        if(cr.type == 1)
                        {
                            cr.velocity = found->velocity;
                            cr.r.position = rs.position;
                            cr.r.rotation = rs.rotation;
                            continue;
                        }
                    }

                    if(high_detail_entities.find(id) != high_detail_entities.end())
                        continue;

                    common_renderable split = player.value()->renderables[id];
                    client_renderable r = rs.split((pos - rs.position).angle() - M_PI/2);

                    if(split.type == 1)
                    {
                        split.r.vert_dist.clear();
                        split.r.vert_angle.clear();
                        split.r.vert_cols.clear();
                    }

                    //split.r = r;
                    split.r = split.r.merge_into_me(r);
                    split.r.init_rectangular(split.r.approx_dim);
                    split.r.approx_dim = rs.approx_dim;
                    split.r.approx_rad = rs.approx_rad;
                    split.velocity = found->velocity;
                    split.type = 0;

                    player.value()->renderables[id] = split;
                }
            }
        }

        for(uint32_t id : high_detail_entities)
        {
            if(id != uid)
            {
                client_renderable rs;
                entity* found = nullptr;

                for(entity* e : entities.entities)
                {
                    if(e->id == id)
                    {
                        rs = e->r;
                        found = e;
                        break;
                    }
                }

                if(found && rs.vert_dist.size() >= 3)
                {
                    common_renderable split = player.value()->renderables[id];
                    client_renderable r = rs.split((pos - rs.position).angle() - M_PI/2);

                    if(split.type == 0)
                    {
                        split.r.vert_dist.clear();
                        split.r.vert_angle.clear();
                        split.r.vert_cols.clear();
                    }

                    split.r = split.r.merge_into_me(r);
                    split.r.approx_dim = rs.approx_dim;
                    split.r.approx_rad = rs.approx_rad;
                    split.velocity = found->velocity;
                    split.type = 1;

                    player.value()->renderables[id] = split;
                }
            }
        }


        for(auto& i : player.value()->renderables)
        {
            s.renderables.push_back({i.first, i.second});
        }

        ///ok so
        ///if we have a collide shadow packet but not a real packet
        ///start a timer to erase it
        ///cancel the timer if we receive more packets

        std::optional<entity*> plr = entities.fetch(uid);

        if(plr && player)
        {
            player_model* model = player.value();

            for(auto& i : model->renderables)
            {
                ///if this is a pseudo packet we've received but not got a considered packet, cleanup
                if(pseudo_packets.find(i.first) != pseudo_packets.end() && considered_packets.find(i.first) == considered_packets.end())
                {
                    i.second.no_signal();

                    //std::cout << "nope " << std::endl;
                }

                /*if(pseudo_packets.find(i.first) != pseudo_packets.end())
                {
                    std::cout << "received imaginary " << i.first << std::endl;
                }*/

                if(considered_packets.find(i.first) != considered_packets.end())
                {
                    i.second.got_signal();

                    //std::cout << "got sig\n";
                }

                /*if(all_entities.find(i.first) == all_entities.end())
                {
                    i.second.unknown();
                }*/

                i.second.unknown(all_entities.find(i.first) == all_entities.end());
            }

            player.value()->cleanup(plr.value()->r.position);
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
