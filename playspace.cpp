#include "playspace.hpp"
#include <networking/serialisable.hpp>
#include "entity.hpp"
#include "radar_field.hpp"
#include "ship_components.hpp"
#include <SFML/System/Clock.hpp>

struct room_entity : entity
{
    room_entity()
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
            field->subtractive_packets[pack.id] = subtr_it->second;
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

room* playspace::make_room(vec2f where)
{
    room* r = new room;
    r->position = where;

    rooms.push_back(r);

    r->my_entity = entity_manage->make_new<room_entity>();
    r->my_entity->r.position = where;
    r->field->sun_id = field->sun_id;

    return rooms.back();
}

void playspace::init_default()
{
    std::minstd_rand rng;
    rng.seed(0);

    {
        vec2f pos = 0;
        float dim = 500;

        room* test_poi = make_room({pos.x(), pos.y()});

        int num_asteroids = 100;

        int tries = 0;

        for(int i=0; i < num_asteroids; i++)
        {
            vec2f found_pos = rand_det(rng, (vec2f){-dim, -dim}, (vec2f){dim, dim});

            bool cont = false;

            for(entity* e : test_poi->entity_manage->entities)
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

            asteroid* a = test_poi->entity_manage->make_new<asteroid>(field);
            a->init(2, 4);
            a->r.position = found_pos; ///poispace
            a->ticks_between_collisions = 2;
            test_poi->entity_manage->cleanup();
        }
    }


    float intensity = STANDARD_SUN_HEAT_INTENSITY;

    asteroid* sun = entity_manage->make_new<asteroid>(field);
    sun->init(3, 4);
    sun->r.position = {400, 400}; ///realspace
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

    for(room* r : rooms)
    {
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
        play->tick(dt_s);
    }

    for(playspace* play : spaces)
    {
        for(room* r : play->rooms)
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
}

void accumulate_entities(const std::vector<entity*>& entities, ship_network_data& ret, size_t id)
{
    #define SEE_ONLY_REAL

    for(entity* e : entities)
    {
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

        if(rem)
        {
            ret.renderables.push_back(rem->r);
        }
    }
}

ship_network_data playspace_manager::get_network_data_for(size_t id)
{
    ship_network_data ret;

    for(playspace* play : spaces)
    {
        accumulate_entities(play->entity_manage->entities, ret, id);
        accumulate_entities(play->entity_manage->to_spawn, ret, id);

        for(room* r : play->rooms)
        {
            accumulate_entities(r->entity_manage->entities, ret, id);
            accumulate_entities(r->entity_manage->to_spawn, ret, id);
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
        for(room* r : play->rooms)
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
