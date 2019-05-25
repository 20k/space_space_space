#include "playspace.hpp"
#include <networking/serialisable.hpp>
#include "entity.hpp"
#include "radar_field.hpp"
#include "ship_components.hpp"
#include <SFML/System/Clock.hpp>

struct room_entity : entity
{
    room* ren = nullptr;
    room_entity(room* in) : ren(in)
    {
        r.init_rectangular({5, 5});

        collides = false;
    }
};

room::room()
{
    field = std::make_shared<alt_radar_field>((vec2f){1200, 1200});
    field->has_finite_bound = true;
    //field->space_scaling = ROOM_POI_SCALE;

    entity_manage = new entity_manager;
}

room::~room()
{
    delete entity_manage;
}

void room::add(entity* e)
{
    if(auto s = dynamic_cast<ship*>(e); s != nullptr)
    {
        s->current_radar_field = field;
        s->room_type = space_type::REAL_SPACE;
    }

    if(auto a = dynamic_cast<asteroid*>(e); a != nullptr)
    {
        a->current_radar_field = field;
    }

    if(!entity_manage->contains(e))
    {
        e->r.position = get_in_local(e->r.position);
    }

    entity_manage->steal(e);
}

void room::rem(entity* e)
{
    if(entity_manage->contains(e))
    {
        e->r.position = get_in_absolute(e->r.position);
    }

    entity_manage->forget(e);
}

vec2f room::get_in_local(vec2f absolute)
{
    return (absolute - this->position) / ROOM_POI_SCALE;
}

vec2f room::get_in_absolute(vec2f local)
{
    return (local * ROOM_POI_SCALE) + this->position;
}

void room::serialise(serialise_context& ctx, nlohmann::json& data, self_t* other)
{
    DO_SERIALISE(friendly_id);
    DO_SERIALISE(type);
    DO_SERIALISE(name);
    DO_SERIALISE(position);
    DO_SERIALISE(entity_manage);
}

void client_poi_data::serialise(serialise_context& ctx, nlohmann::json& data, self_t* other)
{
    DO_SERIALISE(name);
    DO_SERIALISE(position);
}

alt_frequency_packet transform_space(alt_frequency_packet& in, room& r, alt_radar_field& parent_field)
{
    alt_frequency_packet ret = in;

    alt_radar_field& new_field = *r.field;

    if(ret.last_packet)
    {
        ret.last_packet = std::make_shared<alt_frequency_packet>(transform_space(*ret.last_packet, r, parent_field));
    }

    ret.origin = r.get_in_local(ret.origin);

    if(ret.reflected_by != -1)
        ret.reflected_position = r.get_in_local(ret.reflected_position);

    //uint32_t it_diff = (parent_field.iteration_count - ret.start_iteration) * ROOM_POI_SCALE;

    uint32_t it_diff = (parent_field.iteration_count - ret.start_iteration);

    ret.start_iteration = new_field.iteration_count - it_diff;
    //ret.scale = ROOM_POI_SCALE;

    //std::cout << "start " << ret.start_iteration << " real " << new_field.iteration_count << std::endl;

    //float rdist = (new_field.iteration_count)

    //std::cout << "expr? " << ret.force_cleanup << std::endl;

    return ret;
}

void room::import_radio_waves_from(alt_radar_field& theirs)
{
    //sf::Clock clk;

    float lrad = entity_manage->collision.half_dim.largest_elem();

    for(alt_frequency_packet& pack : theirs.packets)
    {
        if(imported_waves.find(pack.id) != imported_waves.end())
            continue;

        alt_frequency_packet fixed_pack = transform_space(pack, *this, theirs);

        float current_radius = (field->iteration_count - fixed_pack.start_iteration) * field->speed_of_light_per_tick;
        float next_radius = current_radius + field->speed_of_light_per_tick;

        if(!entity_manage->collision.intersects(fixed_pack.origin, current_radius, next_radius, fixed_pack.precalculated_start_angle, fixed_pack.restrict_angle, fixed_pack.left_restrict, fixed_pack.right_restrict))
            continue;

        imported_waves[pack.id] = true;
        field->packets.push_back(fixed_pack);

        auto subtr_it = theirs.subtractive_packets.find(pack.id);

        if(subtr_it != theirs.subtractive_packets.end())
        {
            auto vec = subtr_it->second;

            for(auto& i : vec)
            {
                i = transform_space(i, *this, theirs);
            }

            field->subtractive_packets[pack.id] = vec;
        }

        auto f_ignore = theirs.ignore_map.find(pack.id);

        if(f_ignore != theirs.ignore_map.end())
        {
            field->ignore_map[pack.id] = f_ignore->second;
        }
    }

    //std::cout << "import time " << clk.getElapsedTime().asMicroseconds() / 1000. << std::endl;
}

void playspace::serialise(serialise_context& ctx, nlohmann::json& data, self_t* other)
{
    DO_SERIALISE(friendly_id);
    DO_SERIALISE(type);
    DO_SERIALISE(name);
    DO_SERIALISE(position);
    DO_SERIALISE(entity_manage);
}

playspace::playspace()
{
    field = std::make_shared<alt_radar_field>((vec2f){800, 800});
    entity_manage = new entity_manager;
    //field->space_scaling = ROOM_POI_SCALE;
    field->speed_of_light_per_tick = field->speed_of_light_per_tick * ROOM_POI_SCALE;
    field->space_scaling = 1/ROOM_POI_SCALE;
}

playspace::~playspace()
{
    delete entity_manage;
}

std::vector<room*> playspace::all_rooms()
{
    auto r1 = rooms;

    for(auto& i : pending_rooms)
    {
        r1.push_back(i);
    }

    return r1;
}

room* playspace::make_room(vec2f where)
{
    room* r = new room;
    r->position = where;

    pending_rooms.push_back(r);

    r->my_entity = entity_manage->make_new<room_entity>(r);

    {
        r->my_entity->r = client_renderable();

        int n = 7;

        for(int i=0; i < n; i++)
        {
            r->my_entity->r.vert_dist.push_back(4);

            float angle = ((float)i / n) * 2 * M_PI;

            r->my_entity->r.vert_angle.push_back(angle);

            r->my_entity->r.vert_cols.push_back({1,0.7,0,1});
        }

        r->my_entity->r.approx_rad = 4;
        r->my_entity->r.approx_dim = {4, 4};
    }

    r->my_entity->r.position = where;
    r->my_entity->collides = false;
    r->field->sun_id = field->sun_id;
    return r;
}

void playspace::delete_room(room* r)
{
    std::vector<room_entity*> ens = entity_manage->fetch<room_entity>();

    for(room_entity* e : ens)
    {
        if(e->ren == r)
        {
            e->cleanup = true;
        }
    }

    delete r;
}

void make_asteroid_poi(std::minstd_rand& rng, room* r, float dim)
{
    r->name = "Asteroid Belt";

    int num_asteroids = 100;

    int tries = 0;

    for(int i=0; i < num_asteroids; i++)
    {
        vec2f found_pos = rand_det(rng, (vec2f){-dim, -dim}, (vec2f){dim, dim});

        bool cont = false;

        for(entity* e : r->entity_manage->entities)
        {
            if((e->r.position - found_pos).length() < 40)
            {
                cont = true;
                break;
            }
        }

        if(tries > 100)
            break;

        if(cont)
        {
            tries++;

            i--;
            continue;
        }

        tries = 0;

        asteroid* a = r->entity_manage->make_new<asteroid>(r->field);
        a->init(2, 4);
        a->r.position = found_pos; ///poispace
        a->ticks_between_collisions = 2;
        r->entity_manage->cleanup();
    }
}

void playspace::init_default()
{
    std::minstd_rand rng;
    rng.seed(0);

    for(int i=0; i < 100; i++)
        rng();

    int real_belts = 1;

    for(int i=0; i < real_belts; i++)
    {
        float rad = 200;

        float poi_angle = rand_det_s(rng, 0, 2 * M_PI);

        std::cout << "POIANG " << poi_angle << std::endl;

        {
            vec2f pos = (vec2f){rad, 0}.rot(poi_angle);
            float dim = 500;

            room* test_poi = make_room({pos.x(), pos.y()});

            make_asteroid_poi(rng, test_poi, dim);
        }

        float cfrac = poi_angle + 0.01;

        int max_num = 100;

        for(int idx = 0; idx < max_num; idx++)
        {
            asteroid* a = entity_manage->make_new<asteroid>(field);
            a->init(1, 2);
            a->is_heat = false;
            a->collides = false;

            float ang = cfrac + idx * 2 * M_PI / max_num;

            float my_rad = rad + rand_det_s(rng, -rad * 0.1, rad * 0.1);

            vec2f pos = (vec2f){my_rad, 0}.rot(ang);

            a->r.position = pos;
        }
    }

    float intensity = STANDARD_SUN_HEAT_INTENSITY;

    asteroid* sun = entity_manage->make_new<asteroid>(field);
    sun->init(3, 4);
    sun->r.position = {0, 0}; ///realspace
    sun->permanent_heat = intensity * (1/ROOM_POI_SCALE) * (1/ROOM_POI_SCALE);
    sun->reflectivity = 0;

    field->sun_id = sun->id;
}

void playspace::add(entity* e)
{
    if(auto s = dynamic_cast<ship*>(e); s != nullptr)
    {
        s->current_radar_field = field;
        s->room_type = space_type::S_SPACE;
    }

    if(auto a = dynamic_cast<asteroid*>(e); a != nullptr)
    {
        a->current_radar_field = field;
    }

    entity_manage->steal(e);
}

void playspace::rem(entity* e)
{
    entity_manage->forget(e);
}

void playspace_manager::serialise(serialise_context& ctx, nlohmann::json& data, self_t* other)
{
    DO_SERIALISE(spaces);
}

void room::tick(double dt_s)
{
    entity_manage->tick(dt_s);
    entity_manage->cleanup();

    /*for(entity* e : entity_manage->entities)
    {
        ship* s = dynamic_cast<ship*>(e);

        if(s)
        {
            std::vector<pending_transfer> all_transfers;
            s->consume_all_transfers(all_transfers);

            std::vector<std::optional<ship>> removed_ships;

            for(auto& i : all_transfers)
            {
                removed_ships.push_back(s->remove_ship_by_id(i.pid_ship));
            }

            for(int i=0; i < (int)removed_ships.size(); i++)
            {
                if(!removed_ships[i])
                    continue;

                s->add_ship_to_component(removed_ships[i].value(), all_transfers[i].pid_component);
            }
        }
    }*/

    std::vector<ship*> ships = entity_manage->fetch<ship>();

    for(ship* s : ships)
    {
        s->last_sample = field->sample_for(s->r.position, *s, *entity_manage, true, s->get_radar_strength());
    }

    field->finite_bound = entity_manage->collision.half_dim.largest_elem() * 1.5;
    field->finite_centre = entity_manage->collision.pos;

    //std::cout << "finite bound " << field->finite_bound << std::endl;

    field->tick(*entity_manage, dt_s);

    /*for(alt_frequency_packet& pack : field->packets)
    {
        float rad = (field->iteration_count - pack.start_iteration) * field->speed_of_light_per_tick;

        std::cout << "rad " << rad << std::endl;
    }*/

    //std::cout << "fdnum " << field->packets.size() << std::endl;
}

void playspace::tick(double dt_s)
{
    /*std::vector<ship*> preships = entity_manage->fetch<ship>();

    for(auto& s : preships)
    {
        s->current_radar_field = field;
    }

    for(entity* e : entity_manage->to_spawn)
    {
        ship* s = dynamic_cast<ship*>(e);

        if(!s)
            continue;

        s->current_radar_field = field;
    }

    std::vector<asteroid*> roids = entity_manage->fetch<asteroid>();

    for(auto& a : roids)
    {
        a->current_radar_field = field;
    }*/

    for(auto& i : pending_rooms)
    {
        rooms.push_back(i);
    }

    pending_rooms.clear();

    for(int i=0; i < (int)rooms.size(); i++)
    {
        if(rooms[i]->entity_manage->entities.size() == 0 && rooms[i]->entity_manage->to_spawn.size() == 0)
        {
            room* ptr = rooms[i];
            rooms.erase(rooms.begin() + i);
            i--;
            delete_room(ptr);
            continue;
        }
    }

    for(room* r : rooms)
    {
        r->field->sun_id = field->sun_id;

        r->import_radio_waves_from(*field);

        r->tick(dt_s);
    }

    entity_manage->tick(dt_s);
    entity_manage->cleanup();

    std::vector<ship*> ships = entity_manage->fetch<ship>();

    for(ship* s : ships)
    {
        s->last_sample = field->sample_for(s->r.position, *s, *entity_manage, true, s->get_radar_strength());
    }

    field->tick(*entity_manage, dt_s);

    //std::cout << "parent num " << field->packets.size() << std::endl;
}

void playspace_manager::tick(double dt_s)
{
    for(playspace* play : spaces)
    {
        auto all = play->all_rooms();

        for(room* r : all)
        {
            std::vector<ship*> ships = r->entity_manage->fetch<ship>();

            for(ship* s : ships)
            {
                s->check_space_rules(*this, play, r);
            }
        }

        std::vector<ship*> ships = play->entity_manage->fetch<ship>();

        for(ship* s : ships)
        {
            s->check_space_rules(*this, play, nullptr);
        }
    }

    for(playspace* play : spaces)
    {
        play->tick(dt_s);
    }
}

void accumulate_entities(const std::vector<entity*>& entities, ship_network_data& ret, size_t id, bool get_room_entity)
{
    #define SEE_ONLY_REAL

    for(entity* e : entities)
    {
        if(e->cleanup)
            continue;

        ship* s = dynamic_cast<ship*>(e);
        room_entity* rem = dynamic_cast<room_entity*>(e);

        if(s)
        {
            #ifdef SEE_ONLY_REAL
            if(s->network_owner != id)
                continue;
            #endif // SEE_ONLY_REAL

            ret.ships.push_back(s);
            ret.renderables.push_back(s->r);
        }
        else
        {
            #ifndef SEE_ONLY_REAL
            ret.renderables.push_back(e->r);
            #endif // SEE_ONLY_REAL

        }

        if(rem && get_room_entity)
        {
            ret.renderables.push_back(rem->r);
        }
    }
}

ship_network_data playspace_manager::get_network_data_for(entity* e, size_t id)
{
    ship_network_data ret;

    /*for(playspace* play : spaces)
    {
        accumulate_entities(play->entity_manage->entities, ret, id);
        accumulate_entities(play->entity_manage->to_spawn, ret, id);

        for(room* r : play->rooms)
        {
            accumulate_entities(r->entity_manage->entities, ret, id);
            accumulate_entities(r->entity_manage->to_spawn, ret, id);
        }
    }*/

    if(e == nullptr)
        return ret;

    bool in_fsd_space = false;

    for(playspace* play : spaces)
    {
        if(play->entity_manage->contains(e))
        {
            in_fsd_space = true;
            break;
        }
    }

    for(playspace* play : spaces)
    {
        accumulate_entities(play->entity_manage->entities, ret, id, in_fsd_space);
        accumulate_entities(play->entity_manage->to_spawn, ret, id, in_fsd_space);

        if(in_fsd_space)
        {
            for(auto& e : play->entity_manage->entities)
            {
                if(!e->collides)
                {
                    ret.renderables.push_back(e->r);
                    continue;
                }
            }
        }

        auto all = play->all_rooms();

        for(room* r : all)
        {
            accumulate_entities(r->entity_manage->entities, ret, id, false);
            accumulate_entities(r->entity_manage->to_spawn, ret, id, false);

            if(in_fsd_space)
            {
                client_poi_data poi;
                poi.name = r->name;
                poi.position = r->position;

                ret.pois.push_back(poi);
            }
        }
    }

    return ret;
}

std::optional<room*> playspace_manager::get_nearby_room(entity* e)
{
    for(playspace* play : spaces)
    {
        if(play->entity_manage->contains(e))
        {
            if(play->rooms.size() == 0)
                return std::nullopt;

            room* nearest = nullptr;
            float near_dist = FLT_MAX;

            for(room* r : play->rooms)
            {
                float dist = (e->r.position - r->position).length();

                if(dist < near_dist)
                {
                    near_dist = dist;
                    nearest = r;
                }
            }

            if(near_dist >= 300)
                return std::nullopt;

            assert(nearest);

            return nearest;
        }
    }

    return std::nullopt;
}

void playspace_manager::exit_room(entity* e)
{
    for(playspace* play : spaces)
    {
        auto rms = play->all_rooms();

        for(room* r : rms)
        {
            if(r->entity_manage->contains(e))
            {
                std::cout << "exited room\n";

                r->rem(e);
                play->add(e);

                return;
            }
        }
    }
}

void playspace_manager::enter_room(entity* e, room* r)
{
    bool found = false;

    for(playspace* play : spaces)
    {
        if(play->entity_manage->contains(e))
        {
            play->rem(e);
            found = true;
            break;
        }
    }

    if(!found)
        return;

    r->add(e);

    std::cout << "entered room\n";
}
