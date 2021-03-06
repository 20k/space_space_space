#include "playspace.hpp"
#include <networking/serialisable.hpp>
#include "entity.hpp"
#include "radar_field.hpp"
#include "ship_components.hpp"
#include <SFML/System/Clock.hpp>
#include <iostream>
#include "random.hpp"
#include <set>

aggregate<int> get_room_aggregate_absolute(room* r1)
{
    aggregate<int> agg;

    vec2f position = r1->get_in_absolute(r1->entity_manage->collision.pos);

    vec2f approx_dim = r1->entity_manage->collision.half_dim * ROOM_POI_SCALE;
    approx_dim = max(approx_dim, (vec2f){25 * ROOM_POI_SCALE, 25 * ROOM_POI_SCALE});

    agg.pos = position;
    agg.half_dim = approx_dim;

    agg.recalculate_bounds();

    return agg;
}

struct star_entity : asteroid
{
    bool neutron = false;
    float n_width = M_PI/24;

    star_entity(std::shared_ptr<alt_radar_field> field) : asteroid(field)
    {
        r.rotation = randf_s(0, 2 * M_PI);
    }

    void make_neutron(std::minstd_rand& rng)
    {
        n_width = rand_det_s(rng, M_PI/12, M_PI/32);
        angular_velocity = rand_det_s(rng, 2 * M_PI/10, 2 * M_PI / 40);
        neutron = true;
    }

    virtual void tick(double dt_s) override
    {
        heatable_entity::tick(dt_s);

        float emitted = latent_heat * HEAT_EMISSION_FRAC + permanent_heat;

        if(neutron)
        {
            alt_frequency_packet heat;
            heat.make(permanent_heat + emitted, HEAT_FREQ);
            heat.restrict(r.rotation, n_width);

            current_radar_field->emit(heat, r.position, *this);

            heat.restrict(r.rotation + M_PI, n_width);

            current_radar_field->emit(heat, r.position, *this);
        }
        else
        {
            alt_frequency_packet heat;
            heat.make(permanent_heat + emitted, HEAT_FREQ);

            current_radar_field->emit(heat, r.position, *this);
        }

        latent_heat -= emitted;
    }
};

struct room_entity : entity
{
    room* ren = nullptr;
    room_entity(room* in) : ren(in)
    {
        r.init_rectangular({5, 5});

        collides = false;
    }

    virtual void tick(double dt_s) override
    {
        if(cleanup)
            return;

        ///not good enough
        //if(ren->ptype == poi_type::DEAD_SPACE)
        //    r.position = ren->get_in_absolute(ren->entity_manage->collision.pos);
    }
};

struct packet_harvester_type : precise_aggregator
{
    room* ren = nullptr;

    packet_harvester_type(room* in) : ren(in)
    {
        r.init_rectangular({5, 5});
        precise_harvestable = true;
        aggregate_unconditionally = true;

        collides = false;

        r.vert_angle.clear();
        r.vert_cols.clear();
        r.vert_dist.clear();
    }

    virtual void tick(double dt_s) override
    {
        if(cleanup)
            return;

        active = ren->radar_active;

        r.position = ren->get_in_absolute(ren->entity_manage->collision.pos);
        r.approx_dim = ren->entity_manage->collision.half_dim * ROOM_POI_SCALE;

        r.approx_dim = max(r.approx_dim, (vec2f){25 * ROOM_POI_SCALE, 25 * ROOM_POI_SCALE});

        r.approx_dim += (vec2f){50*ROOM_POI_SCALE, 50*ROOM_POI_SCALE};

        agg.pos = r.position;
        agg.half_dim = r.approx_dim;

        agg.recalculate_bounds();

        //r.init_rectangular(r.approx_dim);

        //std::cout << "RADIM " << r.approx_dim << std::endl;
    }
};

room::room()
{
    field = std::make_shared<alt_radar_field>((vec2f){1200, 1200});
    field->has_finite_bound = true;
    field->space = RENDER_LAYER_REALSPACE;
    //field->space_scaling = ROOM_POI_SCALE;

    entity_manage = new entity_manager;
}

room::~room()
{
    delete entity_manage;
}

void room::add(std::shared_ptr<entity> e)
{
    if(auto s = std::dynamic_pointer_cast<ship>(e); s != nullptr)
    {
        s->current_radar_field = field;
        s->room_type = space_type::REAL_SPACE;
    }

    if(auto a = std::dynamic_pointer_cast<asteroid>(e); a != nullptr)
    {
        a->current_radar_field = field;
    }

    if(!entity_manage->contains(e))
    {
        e->r.position = get_in_local(e->r.position);
    }

    entity_manage->steal(e);
}

void room::rem(std::shared_ptr<entity> e)
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

void room::serialise(serialise_context& ctx, nlohmann::json& data, room* other)
{
    DO_SERIALISE(friendly_id);
    DO_SERIALISE(name);
    DO_SERIALISE(position);
    DO_SERIALISE(entity_manage);
    DO_SERIALISE(ptype);
    DO_SERIALISE(poi_offset);
}


struct transform_data
{
    vec2f origin;
    vec2f reflected_position;
    uint32_t start_iteration = 0;
};

transform_data transform_space(const alt_frequency_packet& in, room& my_room, alt_radar_field& parent_field, std::optional<room*> them)
{
    //alt_frequency_packet ret = in;

    transform_data ret;

    alt_radar_field& new_field = *my_room.field;

    if(them == std::nullopt)
        ret.origin = my_room.get_in_local(in.origin);
    else
        ret.origin = my_room.get_in_local(them.value()->get_in_absolute(in.origin));

    if(in.reflected_by != -1)
    {
        if(them == std::nullopt)
            ret.reflected_position = my_room.get_in_local(in.reflected_position);
        else
            ret.reflected_position = my_room.get_in_local(them.value()->get_in_absolute(in.origin));
    }

    uint32_t it_diff = (parent_field.iteration_count - in.start_iteration);

    ret.start_iteration = new_field.iteration_count - it_diff;
    //ret.scale = ROOM_POI_SCALE;

    //std::cout << "start " << ret.start_iteration << " real " << new_field.iteration_count << std::endl;

    //float rdist = (new_field.iteration_count)

    //std::cout << "expr? " << ret.force_cleanup << std::endl;

    return ret;
}

void import_radio_fast(room& me, const std::vector<alt_frequency_packet>& pack, alt_radar_field& theirs, std::optional<room*> them)
{
    for(const alt_frequency_packet& pack : pack)
    {
        transform_data data = transform_space(pack, me, theirs, them);

        alt_frequency_packet fixed_pack = pack;
        fixed_pack.origin = data.origin;
        fixed_pack.reflected_position = data.reflected_position;
        fixed_pack.start_iteration = data.start_iteration;

        me.field->packets.push_back(fixed_pack);

        auto subtr_it = theirs.subtractive_packets.find(pack.id);

        if(subtr_it != theirs.subtractive_packets.end())
        {
            auto vec = subtr_it->second;

            for(auto& i : vec)
            {
                transform_data trans = transform_space(i, me, theirs, them);

                i.origin = trans.origin;
                i.reflected_position = trans.reflected_position;
                i.start_iteration = trans.start_iteration;
            }

            me.field->subtractive_packets[pack.id] = vec;
        }
    }
}

void room_merge(room* r1, room* r2)
{
    if(r1->ptype == poi_type::DEAD_SPACE && r2->ptype != poi_type::DEAD_SPACE)
        return room_merge(r2, r1);

    auto e2 = r2->entity_manage->entities;
    auto e3 = r2->entity_manage->to_spawn;

    auto transform_func = [&](std::shared_ptr<entity>& e)
    {
        ship* s = dynamic_cast<ship*>(e.get());

        if(s)
        {
            vec2f cpos = {s->move_args.x, s->move_args.y};

            cpos = r1->get_in_local(r2->get_in_absolute(cpos));

            s->move_args.x = cpos.x();
            s->move_args.y = cpos.y();
        }

        r2->rem(e);
        r1->add(e);
    };

    for(auto& i : e2)
    {
        transform_func(i);
    }

    for(auto& i : e3)
    {
        transform_func(i);
    }

    import_radio_fast(*r1, r2->field->packets, *r2->field, r2);

    r2->field->packets.clear();
    r2->field->subtractive_packets.clear();

    r2->entity_manage->entities.clear();
    r2->entity_manage->to_spawn.clear();
}

#define MERGE_DIST 500

void room_handle_split(playspace* play, room* r1)
{
    ///so
    ///can we orchestrate the entities of us into two separate rooms, such that there are no overlapping aggregates,
    ///with a certain distance between them?
    if(r1->entity_manage->entities.size() <= 1)
        return;

    ///db scan
    std::vector<aggregate<std::shared_ptr<entity>>> aggs;

    float minimum_distance = MERGE_DIST*4;

    for(std::shared_ptr<entity> e : r1->entity_manage->entities)
    {
        bool found = false;

        vec2f dim_with_padding = e->r.approx_dim + (vec2f){minimum_distance, minimum_distance};

        vec2f tl = e->r.position - dim_with_padding;
        vec2f br = e->r.position + dim_with_padding;

        for(int i=0; i < (int)aggs.size(); i++)
        {
            aggregate<std::shared_ptr<entity>>& found_agg = aggs[i];

            if(rect_intersect(found_agg.tl, found_agg.br, tl, br))
            {
                ///conservative pad
                //vec2f ctl = e->r.position - e->r.approx_dim;
                //vec2f cbr = e->r.position + e->r.approx_dim;

                found_agg.data.push_back(e);

                ///???? unsure if this performance optimisation is valid
                ///should be, but as its not a big problem atm i'm sitting on it
                //if(!rect_intersect(found_agg.tl, found_agg.br, cbr, ctl))
                    found_agg.complete();

                found = true;
                break;
            }
        }

        if(!found)
        {
            aggregate<std::shared_ptr<entity>> nen;
            nen.data.push_back(e);
            nen.complete();

            aggs.push_back(nen);
        }
    }

    bool any_change = true;

    while(any_change)
    {
        any_change = false;

        for(int i=0; i < (int)aggs.size(); i++)
        {
            for(int j=i+1; j < (int)aggs.size(); j++)
            {
                if(aggs[i].intersects_with_bound(aggs[j], minimum_distance))
                {
                    aggs[i].data.insert(aggs[i].data.end(), aggs[j].data.begin(), aggs[j].data.end());
                    aggs[i].complete();

                    i--;
                    aggs.erase(aggs.begin() + j);
                    any_change = true;
                    break;
                }
            }
        }
    }

    if(aggs.size() <= 1)
        return;

    std::sort(aggs.begin(), aggs.end(), [](auto& i1, auto& i2){return i1.data.size() > i2.data.size();});

    for(int i=1; i < (int)aggs.size(); i++)
    {
        aggregate<std::shared_ptr<entity>>& found_agg = aggs[i];

        room* r2 = play->make_room(r1->get_in_absolute(found_agg.pos), 5, poi_type::DEAD_SPACE);

        for(std::shared_ptr<entity> e : found_agg.data)
        {
            ship* s = dynamic_cast<ship*>(e.get());

            if(s)
            {
                vec2f cpos = {s->move_args.x, s->move_args.y};

                cpos = r2->get_in_local(r1->get_in_absolute(cpos));

                s->move_args.x = cpos.x();
                s->move_args.y = cpos.y();
            }

            r1->rem(e);
            r2->add(e);
        }
    }
}

void client_poi_data::serialise(serialise_context& ctx, nlohmann::json& data, client_poi_data* other)
{
    DO_SERIALISE(name);
    DO_SERIALISE(position);
    DO_SERIALISE(poi_pid);
    DO_SERIALISE(type);
    DO_SERIALISE(offset);
}

void room::import_radio_waves_from(alt_radar_field& theirs)
{
    //sf::Clock clk;

    assert(packet_harvester);

    bool should_simulate = false;

    for(std::shared_ptr<entity>& e: entity_manage->entities)
    {
        ship* s = dynamic_cast<ship*>(e.get());

        if(s)
        {
            should_simulate = true;
            break;
        }
    }

    radar_active = should_simulate;

    #define FAST
    #ifdef FAST
    if(should_simulate)
    #endif // FAST
        import_radio_fast(*this, packet_harvester->samples, theirs, std::nullopt);

    packet_harvester->samples.clear();

    //std::cout << "import time " << clk.getElapsedTime().asMicroseconds() / 1000. << std::endl;
}

std::vector<std::shared_ptr<ship>> get_nearby_ships(room& r, ship& me, float range)
{
    std::vector<std::shared_ptr<ship>> ret;

    bool found_me = false;

    std::vector<std::shared_ptr<ship>> srch = r.entity_manage->fetch<ship>();

    for(std::shared_ptr<ship>& s : srch)
    {
        if(s.get() == &me)
        {
            found_me = true;
            continue;
        }

        if((me.r.position - s->r.position).length() > range)
            continue;

        ret.push_back(s);
    }

    if(!found_me)
        throw std::runtime_error("Attempted to query room that ship is not in");

    return ret;
}

std::optional<std::shared_ptr<ship>> room::get_nearby_unfinished_ship(ship& me, size_t ship_pid)
{
    std::vector<std::shared_ptr<ship>> near = get_nearby_unfinished_ships(me);

    for(auto& i : near)
    {
        if(i->_pid == ship_pid)
            return i;
    }

    return std::nullopt;
}

std::vector<std::shared_ptr<ship>> room::get_nearby_unfinished_ships(ship& me)
{
    std::vector<std::shared_ptr<ship>> ships = get_nearby_ships(*this, me, 100);

    std::vector<std::shared_ptr<ship>> ret;

    for(std::shared_ptr<ship>& s : ships)
    {
        if(s->is_ship && s->is_build_holder)
        {
            ret.push_back(s);
        }
    }

    return ret;
}

std::vector<std::pair<ship, std::vector<component>>> room::get_nearby_accessible_ships(ship& me, std::optional<size_t> unconditional_access)
{
    std::vector<std::pair<ship, std::vector<component>>> ret;

    std::vector<std::shared_ptr<ship>> found_ships = get_nearby_ships(*this, me, 100);

    for(std::shared_ptr<ship>& s : found_ships)
    {
        std::vector<component> access;

        for(component& c : s->components)
        {
            if(!c.is_storage())
                continue;

            bool has_access = false;

            if(unconditional_access.has_value() && unconditional_access.value() == s->_pid)
                has_access = true;

            if(has_access || c.foreign_access.allowed(me._pid))
            {
                access.push_back(c);
            }
        }

        if(access.size() > 0)
        {
            ret.push_back({*s, access});
        }
    }

    return ret;
}

bool room::try_dock_to(size_t child, size_t parent)
{
    if(child == parent)
        return false;

    ship* child_ship = nullptr;
    ship* parent_ship = nullptr;

    std::vector<std::shared_ptr<ship>> ships = entity_manage->fetch<ship>();

    for(auto& i : ships)
    {
        if(i->_pid == child)
        {
            child_ship = i.get();
        }

        if(i->_pid == parent)
        {
            parent_ship = i.get();
        }
    }

    if(child_ship == nullptr || parent_ship == nullptr)
        return false;

    if((child_ship->r.position - parent_ship->r.position).length() > 100)
        return false;

    ship scopy = *child_ship;

    for(component& c : parent_ship->components)
    {
        if(!c.foreign_access.can_dock(child))
            continue;

        if(!c.can_store(scopy))
            continue;

        c.store(scopy);
        child_ship->cleanup = true;
        return true;
    }

    return false;
}

void playspace::serialise(serialise_context& ctx, nlohmann::json& data, playspace* other)
{
    DO_SERIALISE(friendly_id);
    DO_SERIALISE(type);
    DO_SERIALISE(name);
    DO_SERIALISE(position);
    DO_SERIALISE(entity_manage);
    DO_SERIALISE(drawables);
}

playspace::playspace()
{
    field = std::make_shared<alt_radar_field>((vec2f){800, 800});
    field->space = RENDER_LAYER_SSPACE;
    entity_manage = new entity_manager;
    //field->space_scaling = ROOM_POI_SCALE;
    field->speed_of_light_per_tick = field->speed_of_light_per_tick * ROOM_POI_SCALE;
    field->space_scaling = 1/ROOM_POI_SCALE;

    drawables = new entity_manager;
}

playspace::~playspace()
{
    delete drawables;
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

room* playspace::make_room(vec2f where, float entity_rad, poi_type::type ptype)
{
    room* r = new room;
    r->position = where;
    r->ptype = ptype;

    std::map<poi_type::type, int> counts;

    auto all_room = all_rooms();

    for(room* old : all_room)
    {
        counts[old->ptype]++;
    }

    r->poi_offset = counts[ptype];

    pending_rooms.push_back(r);

    r->my_entity = entity_manage->make_new<room_entity>(r);
    r->packet_harvester = entity_manage->make_new<packet_harvester_type>(r);

    room_specific_cleanup[r->_pid].push_back(r->my_entity);
    room_specific_cleanup[r->_pid].push_back(r->packet_harvester);

    {
        r->my_entity->r = client_renderable();

        /*int n = 7;

        for(int i=0; i < n; i++)
        {
            r->my_entity->r.vert_dist.push_back(entity_rad);

            float angle = ((float)i / n) * 2 * M_PI;

            r->my_entity->r.vert_angle.push_back(angle);

            r->my_entity->r.vert_cols.push_back({1,0.7,0,1});
        }

        r->my_entity->r.approx_rad = 4;
        r->my_entity->r.approx_dim = {4, 4};*/

        r->my_entity->r.init_rectangular({1, 0.5});
        r->my_entity->r.rotation = where.angle() + M_PI/2;
    }

    r->my_entity->r.position = where;
    r->my_entity->collides = false;

    r->packet_harvester->r.position = where;

    r->field->iteration_count = field->iteration_count;

    r->name = "Dead Space";
    return r;
}

void playspace::delete_room(room* r)
{
    /*std::vector<room_entity*> ens = entity_manage->fetch<room_entity>();

    for(room_entity* e : ens)
    {
        if(e->ren == r)
        {
            e->cleanup = true;
        }
    }*/

    auto it = room_specific_cleanup.find(r->_pid);

    if(it != room_specific_cleanup.end())
    {
        //std::cout << "FOUND " << it->second.size() << std::endl;

        for(auto& i : it->second)
            i->cleanup = true;

        room_specific_cleanup.erase(it);
    }

    assert(r->entity_manage->fetch<ship>().size() == 0);

    delete r;
}

void make_asteroid_poi(std::minstd_rand& rng, room* r, float dim, int num_asteroids)
{
    r->name = "Asteroid Belt";

    int tries = 0;

    for(int i=0; i < num_asteroids; i++)
    {
        vec2f found_pos = rand_det(rng, (vec2f){-dim, -dim}, (vec2f){dim, dim});

        bool cont = false;

        for(std::shared_ptr<entity>& e : r->entity_manage->entities)
        {
            if((e->r.position - found_pos).length() < 10)
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

        std::vector<vec2f> sizes;
        sizes.push_back({1, 1.5});
        sizes.push_back({1.5, 2.5});
        sizes.push_back({2.5, 3});
        sizes.push_back({3.5, 4});
        sizes.push_back({9, 10});

        float rval = rand_det_s(rng, 0, 1);

        vec2f val;

        piecewise_linear(val, sizes[0], sizes[1], 0, 0.3, rval);
        piecewise_linear(val, sizes[1], sizes[2], 0.3, 0.7, rval);
        piecewise_linear(val, sizes[2], sizes[3], 0.7, 0.95, rval);
        piecewise_linear(val, sizes[3], sizes[4], 0.95, 1, rval);


        std::shared_ptr<asteroid> a = r->entity_manage->make_new<asteroid>(r->field);
        a->init(val.x(), val.y());
        a->r.position = found_pos; ///poispace
        a->ticks_between_collisions = 2;
        //a->is_heat = false;
        a->angular_velocity = rand_det_s(rng, -0.01, 0.3);

        if(val.y() < 3)
            a->sun_reflectivity = 0;

        r->entity_manage->cleanup();
    }
}

bool overlaps_any_merge(playspace* play, vec2f to_spawn_absolute, vec2f to_spawn_local_dim)
{
    aggregate<int> test_aggregate;
    test_aggregate.half_dim = to_spawn_local_dim * ROOM_POI_SCALE;
    test_aggregate.half_dim = max(test_aggregate.half_dim, (vec2f){25 * ROOM_POI_SCALE, 25 * ROOM_POI_SCALE});
    test_aggregate.pos = to_spawn_absolute;

    test_aggregate.recalculate_bounds();

    auto all = play->all_rooms();

    for(room* r1 : all)
    {
        /*vec2f position = r1->get_in_absolute(r1->entity_manage->collision.pos);

        vec2f approx_dim = r1->entity_manage->collision.half_dim * ROOM_POI_SCALE;
        approx_dim = max(approx_dim, (vec2f){25 * ROOM_POI_SCALE, 25 * ROOM_POI_SCALE});

        agg.pos = position;
        agg.half_dim = approx_dim;

        agg.recalculate_bounds();*/

        auto agg = get_room_aggregate_absolute(r1);

        if(test_aggregate.intersects_with_bound(agg, MERGE_DIST * ROOM_POI_SCALE))
            return true;
    }

    return false;
}

void playspace::init_default(int seed)
{
    std::minstd_rand rng;
    rng.seed(seed);

    for(int i=0; i < 10000; i++)
        rng();

    float intensity = STANDARD_SUN_HEAT_INTENSITY;

    std::shared_ptr<star_entity> sun = entity_manage->make_new<star_entity>(field);
    sun->init(3, 4);
    sun->r.position = {0, 0}; ///realspace
    sun->permanent_heat = intensity * (1/ROOM_POI_SCALE) * (1/ROOM_POI_SCALE);
    sun->reflectivity = 0;
    sun->collides = false;
    //sun->make_neutron(rng);

    field->sun_id = sun->_pid;

    /*int real_belts = 80;

    float min_rad = 100;
    float max_rad = 800;

    for(int i=0; i < real_belts; i++)
    {
        //float rad = 200;

        float rad = ((float)i / real_belts) * (max_rad - min_rad) + min_rad;

        float poi_angle = rand_det_s(rng, 0, 2 * M_PI);

        float dim = 1000;

        {
            vec2f pos = (vec2f){rad, 0}.rot(poi_angle);

            room* test_poi = make_room({pos.x(), pos.y()}, dim * ROOM_POI_SCALE, poi_type::ASTEROID_BELT);

            make_asteroid_poi(rng, test_poi, dim, 20);
        }
    }*/

    int belts = 5;

    ///inner belt
    for(int belt_num = 0; belt_num < belts; belt_num++)
    {
        float inner_belt_distance = rand_det_s(rng, 100, 600);

        int num_in_belt = 5;
        float belt_scatter = 0.2 * 0;
        float uniform_scatter = 40;

        float belt_offset = rand_det_s(rng, 0, M_PI*2);
        float belt_angle = M_PI/4;

        for(int i=0; i < (int)num_in_belt; i++)
        {
            //float poi_angle = ((float)i / num_in_belt) * 2 * M_PI + ;

            float jitter = rand_det_s(rng, -M_PI/64, M_PI/64) * 0;
            float rad_scatter = rand_det_s(rng, -uniform_scatter, uniform_scatter);

            float poi_angle = ((float)i / num_in_belt) * belt_angle + jitter + belt_offset;

            float scatter_frac = rand_det_s(rng, -1, 1);

            scatter_frac = signum(scatter_frac) * pow(scatter_frac, 2);

            float scatter_len = inner_belt_distance * belt_scatter * scatter_frac + rad_scatter;

            //scatter_len = signum(scatter_len) * pow(scatter_len, 2);

            float rad = inner_belt_distance + scatter_len;
            inner_belt_distance = rad;

            vec2f pos = (vec2f){rad, 0}.rot(poi_angle);

            float dim = 400;

            if(overlaps_any_merge(this, pos, dim))
                continue;

            room* my_poi = make_room(pos, dim * ROOM_POI_SCALE, poi_type::ASTEROID_BELT);

            make_asteroid_poi(rng, my_poi, dim, 20);
        }
    }


    drawables->force_spawn();
}

void playspace::add(std::shared_ptr<entity> e)
{
    if(auto s = std::dynamic_pointer_cast<ship>(e); s != nullptr)
    {
        s->current_radar_field = field;
        s->room_type = space_type::S_SPACE;
    }

    if(auto a = std::dynamic_pointer_cast<asteroid>(e); a != nullptr)
    {
        a->current_radar_field = field;
    }

    entity_manage->steal(e);
}

void playspace::rem(std::shared_ptr<entity> e)
{
    entity_manage->forget(e);
}

void playspace_manager::serialise(serialise_context& ctx, nlohmann::json& data, playspace_manager* other)
{
    DO_SERIALISE(spaces);
}

void room::tick(double dt_s, bool reaggregate)
{
    for(auto& i : entity_manage->entities)
    {
        if(auto s = std::dynamic_pointer_cast<ship>(i); s != nullptr)
        {
            s->my_room = this;
        }
    }

    entity_manage->tick(dt_s, reaggregate);
    entity_manage->cleanup();

    std::vector<std::shared_ptr<ship>> ships = entity_manage->fetch<ship>();

    for(std::shared_ptr<ship> s : ships)
    {
        s->last_sample = field->sample_for(s->r.position, *s, *entity_manage, true, s->get_sensor_strength());
        s->samples.clear();
    }

    /*field->finite_bound = entity_manage->collision.half_dim.largest_elem() * sqrt(2);*/

    vec2f bound = entity_manage->collision.half_dim;
    bound = std::max(bound, (vec2f){30, 30});

    bound += (vec2f){80, 80};

    field->set_finite_stats(entity_manage->collision.pos, bound);

    //std::cout << "FCENTRE " << field->finite_centre << " RAD " << field->finite_bound << std::endl;

    //std::cout << "finite bound " << field->finite_bound << std::endl;

    //std::cout << "FNAME " << name << std::endl;


    //if(field->packets.size() != 0)
    //    std::cout << "pfdnum " << field->packets.size() << std::endl;

    field->tick(*entity_manage, dt_s);

    /*for(alt_frequency_packet& pack : field->packets)
    {
        float rad = (field->iteration_count - pack.start_iteration) * field->speed_of_light_per_tick;

        std::cout << "rad " << rad << std::endl;
    }*/

    //if(field->packets.size() != 0)
    //    std::cout << "fdnum " << field->packets.size() << std::endl;
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

    //sf::Clock split_merge;

    ///split rooms
    for(int i=0; i < (int)rooms.size(); i++)
    {
        if((iteration % (int)rooms.size()) != i)
            continue;

        room* r1 = rooms[i];

        room_handle_split(this, r1);
    }

    ///merge rooms
    for(int i=0; i < (int)rooms.size(); i++)
    {
        if((iteration % (int)rooms.size()) != i)
            continue;

        for(int j=i+1; j < (int)rooms.size(); j++)
        {
            room* r1 = rooms[i];
            room* r2 = rooms[j];

            auto agg_1 = get_room_aggregate_absolute(r1);
            auto agg_2 = get_room_aggregate_absolute(r2);

            if(agg_1.intersects_with_bound(agg_2, MERGE_DIST * ROOM_POI_SCALE))
            {
                room_merge(r1, r2);
            }
        }
    }

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

    //double sclock = split_merge.getElapsedTime().asMicroseconds()/1000.;
    //std::cout << "SCLOCK " << sclock << std::endl;

    int rid = 0;
    for(room* r : rooms)
    {
        r->field->sun_id = field->sun_id;

        r->import_radio_waves_from(*field);

        bool agg = (iteration % (int)rooms.size()) == rid;

        r->tick(dt_s, agg);

        rid++;
    }

    for(auto& i : entity_manage->entities)
    {
        if(auto s = std::dynamic_pointer_cast<ship>(i); s != nullptr)
        {
            s->my_room = nullptr;
        }
    }

    entity_manage->tick(dt_s);
    entity_manage->cleanup();

    std::vector<std::shared_ptr<ship>> ships = entity_manage->fetch<ship>();

    for(std::shared_ptr<ship>& s : ships)
    {
        s->last_sample = field->sample_for(s->r.position, *s, *entity_manage, true, s->get_sensor_strength());
        s->samples.clear();
    }

    field->tick(*entity_manage, dt_s);

    iteration++;

    //std::cout << "parent num " << field->packets.size() << std::endl;
}

void playspace_connect(playspace* p1, playspace* p2)
{
    p1->connections.push_back(p2);
    p2->connections.push_back(p1);
}

bool playspaces_connected(playspace* p1, playspace* p2)
{
    for(auto& i : p1->connections)
    {
        if(i == p2)
            return true;
    }

    return false;
}

void playspace_manager::tick(double dt_s)
{
    for(playspace* play : spaces)
    {
        auto all = play->all_rooms();

        for(room* r : all)
        {
            std::vector<std::shared_ptr<ship>> ships = r->entity_manage->fetch<ship>();

            for(std::shared_ptr<ship>& s : ships)
            {
                s->check_space_rules(dt_s, *this, play, r);
                s->step_cpus(*this, play, r);
            }
        }

        std::vector<std::shared_ptr<ship>> ships = play->entity_manage->fetch<ship>();

        for(std::shared_ptr<ship>& s : ships)
        {
            s->check_space_rules(dt_s, *this, play, nullptr);
            s->step_cpus(*this, play, nullptr);
        }
    }

    for(playspace* play : spaces)
    {
        play->tick(dt_s);
    }
}

void accumulate_entities(const std::vector<std::shared_ptr<entity>>& entities, ship_network_data& ret, size_t id, bool get_room_entity, bool is_poi)
{
    #define SEE_ONLY_REAL

    for(const std::shared_ptr<entity>& e : entities)
    {
        if(e->cleanup)
            continue;

        std::shared_ptr<ship> s = std::dynamic_pointer_cast<ship>(e);
        std::shared_ptr<room_entity> rem = std::dynamic_pointer_cast<room_entity>(e);

        if(s)
        {
            #ifdef SEE_ONLY_REAL
            if(s->network_owner != id)
                continue;
            #endif // SEE_ONLY_REAL

            ret.ships.push_back(s);
            ret.renderables.push_back(s->r);

            if(is_poi)
                ret.renderables.back().render_layer = RENDER_LAYER_REALSPACE;
            else
                ret.renderables.back().render_layer = RENDER_LAYER_SSPACE;
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
            ret.renderables.back().render_layer = RENDER_LAYER_SSPACE;
        }
    }
}

ship_network_data playspace_manager::get_network_data_for(const std::shared_ptr<entity>& e, size_t id)
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

    /*for(playspace* play : spaces)
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
    }*/

    auto [play, r] = get_location_for(e);

    if(play == nullptr)
        return ret;

    accumulate_entities(play->entity_manage->entities, ret, id, in_fsd_space, false);
    accumulate_entities(play->entity_manage->to_spawn, ret, id, in_fsd_space, false);

    //if(in_fsd_space)
    {
        for(auto& e : play->entity_manage->entities)
        {
            if(!e->collides)
            {
                ret.renderables.push_back(e->r);
                ret.renderables.back().render_layer = RENDER_LAYER_SSPACE;
            }
        }

        for(auto& e : play->drawables->entities)
        {
            if(!e->collides)
            {
                ret.renderables.push_back(e->r);
                ret.renderables.back().render_layer = RENDER_LAYER_SSPACE;
            }
        }
    }

    auto all = play->all_rooms();

    for(room* r : all)
    {
        //if(in_fsd_space)
        {
            client_poi_data poi;
            poi.name = r->name;
            poi.position = r->position;
            poi.poi_pid = r->_pid;
            poi.type = poi_type::rnames[(int)r->ptype];
            poi.offset = r->poi_offset;

            ret.pois.push_back(poi);
        }

        accumulate_entities(r->entity_manage->entities, ret, id, false, true);
        accumulate_entities(r->entity_manage->to_spawn, ret, id, false, true);
    }

    return ret;
}

std::optional<room*> playspace_manager::get_nearby_room(const std::shared_ptr<entity>& e)
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

void playspace_manager::exit_room(const std::shared_ptr<entity>& e)
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

                if(auto s = std::dynamic_pointer_cast<ship>(e); s != nullptr)
                {
                    s->my_room = nullptr;
                }

                return;
            }
        }
    }
}

void playspace_manager::enter_room(const std::shared_ptr<entity>& e, room* r)
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

    if(auto s = std::dynamic_pointer_cast<ship>(e); s != nullptr)
    {
        s->my_room = r;
    }

    std::cout << "entered room\n";
}

std::pair<playspace*, room*> playspace_manager::get_location_for(const std::shared_ptr<entity>& e)
{
    for(playspace* play : spaces)
    {
        if(play->entity_manage->contains(e))
            return {play, nullptr};

        for(room* r : play->rooms)
        {
            if(r->entity_manage->contains(e))
                return {play, r};
        }

        for(room* r : play->pending_rooms)
        {
            if(r->entity_manage->contains(e))
                return {play, r};
        }
    }

    return {nullptr, nullptr};
}

std::vector<playspace*> playspace_manager::get_connected_systems_for(const std::shared_ptr<entity>& e)
{
    auto [play, room] = get_location_for(e);

    assert(play);

    return play->connections;
}

std::optional<playspace*> playspace_manager::get_playspace_from_id(size_t pid)
{
    for(playspace* s : spaces)
    {
        if(s->_pid == pid)
            return s;
    }

    return std::nullopt;
}

std::optional<std::pair<playspace*, room*>> playspace_manager::get_room_from_id(size_t pid)
{
    for(playspace* s : spaces)
    {
        auto all = s->all_rooms();

        for(auto& i : all)
        {
            if(i->_pid == pid)
                return {{s, i}};
        }
    }

    return std::nullopt;
}

std::optional<room*> playspace_manager::get_room_from_symbol(playspace* play, const std::string& sym)
{
    auto all = play->rooms;

    for(room* r : all)
    {
        std::string rsym = poi_type::rnames[(int)r->ptype] + std::to_string(r->poi_offset);

        if(rsym == sym)
            return r;
    }

    return std::nullopt;
}

std::optional<playspace*> playspace_manager::get_playspace_from_name(const std::string& name)
{
    for(playspace* p : spaces)
    {
        if(p->name == name)
            return p;
    }

    return std::nullopt;
}

bool playspace_manager::start_warp_travel(ship& s, size_t pid)
{
    std::optional sys_opt = get_playspace_from_id(pid);

    if(sys_opt)
    {
        auto [my_sys, my_room] = get_location_for(s.shared_from_this());

        if(my_sys && playspaces_connected(my_sys, sys_opt.value()) && !s.travelling_to_poi)
        {
            s.move_warp = true;
            s.warp_to_pid = pid;

            s.travelling_in_realspace = false;
            s.velocity = {0,0};

            return true;
        }
    }

    return false;
}

bool playspace_manager::start_room_travel(ship& s, size_t pid)
{
    std::optional<std::pair<playspace*, room*>> room_opt = get_room_from_id(pid);
    auto [play, my_room] = get_location_for(s.shared_from_this());

    if(room_opt && play && my_room && play == room_opt.value().first && my_room != room_opt.value().second && s.has_s_power)
    {
        s.destination_poi_pid = room_opt.value().second->_pid;
        s.travelling_to_poi = true;
        s.destination_poi_position = room_opt.value().second->position;

        s.travelling_in_realspace = false;
        s.velocity = {0,0};

        return true;
    }

    return false;
}

bool playspace_manager::start_realspace_travel(ship& s, const cpu_move_args& args)
{
    if(s.travelling_to_poi)
        return false;

    if(s.move_warp)
        return false;

    ///unreliable due to activation measures
    /*bool can_fly = s.get_max_velocity_thrust() > 0;

    std::cout << "RSPACE? " << (s.room_type == space_type::REAL_SPACE) << std::endl;

    std::cout << "CFLY " << can_fly << std::endl;;

    if(!can_fly)
        return false;*/

    auto [play, r] = get_location_for(s.shared_from_this());

    if(play == nullptr || r == nullptr)
        return false;

    if(args.id != (size_t)-1 && args.id != s._pid)
    {
        std::optional<std::shared_ptr<entity>> e = r->entity_manage->fetch(args.id);

        ///check that its in the same room as me
        if(!e.has_value())
            return false;

        bool in_sensor_range = false;

        for(auto& i : s.last_sample.renderables)
        {
            if(i.uid == args.id)
            {
                in_sensor_range = true;
                break;
            }
        }

        if(!in_sensor_range)
            return false;

        s.move_args = args;

        if(s.move_args.type == instructions::TMOV)
        {
            s.move_args.x = e.value()->r.position.x();
            s.move_args.y = e.value()->r.position.y();
        }

        if(s.move_args.type == instructions::RMOV)
        {
            vec2f destination = e.value()->r.position;
            vec2f to_dest = (destination - s.r.position).norm();

            vec2f forward_component = to_dest * args.y;
            vec2f perp_component = to_dest.rot(M_PI/2) * args.x;

            s.move_args.x = forward_component.x() + perp_component.x() + s.r.position.x();
            s.move_args.y = forward_component.y() + perp_component.y() + s.r.position.y();
        }

        if(s.move_args.type == instructions::TTRN)
        {
            float their_angle = (e.value()->r.position - s.r.position).angle();

            s.move_args.angle = their_angle;
        }

        if(s.move_args.type == instructions::KEEP)
        {
            float distance_away = s.move_args.radius;

            vec2f absolute_keep_position = (s.r.position - e.value()->r.position).norm() * distance_away + e.value()->r.position;

            s.move_args.x = absolute_keep_position.x();
            s.move_args.y = absolute_keep_position.y();
        }

        s.travelling_in_realspace = true;

        if(s.move_args.type != instructions::KEEP && s.move_args.type != instructions::RMOV)
            s.move_args.lax_distance = e.value()->r.approx_dim.length() * 2 * 1.5 + 5;

        return true;
    }
    else
    {
        s.move_args = args;

        if(s.move_args.type == instructions::TMOV)
            return false;

        if(s.move_args.type == instructions::TTRN)
            return false;

        if(s.move_args.type == instructions::RMOV)
        {
            vec2f my_forward = (vec2f){1, 0}.rot(s.r.rotation) * args.y;
            vec2f my_perp = (vec2f){1, 0}.rot(s.r.rotation + M_PI/2) * args.x;

            s.move_args.x = my_forward.x() + my_perp.x() + s.r.position.x();
            s.move_args.y = my_forward.y() + my_perp.y() + s.r.position.y();
        }

        if(s.move_args.type == instructions::RTRN)
        {
            s.move_args.angle = args.angle + s.r.rotation;
        }

        if(s.move_args.type == instructions::KEEP)
            return false;

        s.travelling_in_realspace = true;
        s.move_args.lax_distance = 5;

        return true;
    }
}

#if 0
bool playspace_manager::start_realspace_travel(ship& s, size_t pid)
{
    if(s.travelling_to_poi)
        return false;

    if(s.move_warp)
        return false;

    auto [play, r] = get_location_for(&s);

    if(play == nullptr || r == nullptr)
        return false;

    std::optional<entity*> e = r->entity_manage->fetch(pid);

    ///check that its in the same room as me
    if(!e.has_value())
        return false;

    bool in_sensor_range = false;

    for(auto& i : s.last_sample.renderables)
    {
        if(i.uid == pid)
        {
            in_sensor_range = true;
            break;
        }
    }

    if(!in_sensor_range)
        return false;

    s.realspace_destination = e.value()->r.position;
    s.realspace_pid_target = pid;
    s.travelling_in_realspace = true;

    bool can_fly = s.get_max_velocity_thrust() > 0;

    return can_fly;
}

bool playspace_manager::start_realspace_travel(ship& s, vec2f coord)
{
    if(s.travelling_to_poi)
        return false;

    if(s.move_warp)
        return false;

    auto [play, r] = get_location_for(&s);

    if(play == nullptr || r == nullptr)
        return false;

    s.realspace_destination = coord;
    s.realspace_pid_target = -1;
    s.travelling_in_realspace = true;

    bool can_fly = s.get_max_velocity_thrust() > 0;

    return can_fly;
}
#endif // 0

std::map<uint64_t, ship_location_data> playspace_manager::get_locations_for(const std::vector<uint64_t>& users)
{
    std::map<uint64_t, ship_location_data> ret;
    std::set<uint64_t> user_set;

    for(auto& i : users)
        user_set.insert(i);

    for(playspace* play : spaces)
    {
        for(room* r : play->rooms)
        {
            std::vector<std::shared_ptr<ship>> ships = r->entity_manage->fetch<ship>();

            for(std::shared_ptr<ship>& s : ships)
            {
                if(user_set.contains(s->network_owner))
                {
                    ret[s->network_owner] = {s, play, r};
                }
            }
        }

        std::vector<std::shared_ptr<ship>> ships = play->entity_manage->fetch<ship>();

        for(std::shared_ptr<ship>& s : ships)
        {
            if(user_set.contains(s->network_owner))
            {
                ret[s->network_owner] = {s, play, nullptr};
            }
        }
    }

    return ret;
}

std::optional<ship*> playspace_manager::get_docked_ship(size_t ship_pid)
{
    for(playspace* play : spaces)
    {
        for(room* r : play->rooms)
        {
            std::vector<std::shared_ptr<ship>> ships = r->entity_manage->fetch<ship>();

            for(std::shared_ptr<ship> s : ships)
            {
                auto s_opt = s->fetch_ship_by_id(ship_pid);

                if(s_opt)
                    return s_opt;
            }
        }

        std::vector<std::shared_ptr<ship>> ships = play->entity_manage->fetch<ship>();

        for(std::shared_ptr<ship> s : ships)
        {
            auto s_opt = s->fetch_ship_by_id(ship_pid);

            if(s_opt)
                return s_opt;
        }
    }

    return std::nullopt;
}

void playspace_manager::undock(const std::vector<uint64_t>& in)
{
    if(in.size() == 0)
        return;

    /*std::set<uint64_t> lookup;

    for(auto& i : in)
        lookup.insert(i);*/

    auto check = [&](room* r, std::shared_ptr<ship> s)
    {
        for(auto& id : in)
        {
            auto s_opt = s->remove_ship_by_id(id);

            if(s_opt)
            {
                ship& to_spawn = s_opt.value();

                std::shared_ptr<ship> spawned = r->entity_manage->take(to_spawn);

                spawned->r.position = s->r.position + (vec2f){40, 0}.rot(s->r.rotation);
                spawned->r.rotation = s->r.rotation;

                spawned->velocity = s->velocity;
                //spawned->phys_ignore.push_back(s->_pid);

                spawned->phys_drag = true;
                spawned->current_radar_field = r->field;
            }
        }
    };

    for(playspace* play : spaces)
    {
        for(room* r : play->rooms)
        {
            std::vector<std::shared_ptr<ship>> ships = r->entity_manage->fetch<ship>();

            for(std::shared_ptr<ship> s : ships)
            {
                /*auto s_opt = s->fetch_ship_by_id(ship_pid);

                if(s_opt)
                    return s_opt;*/

                check(r, s);
            }
        }

        /*std::vector<ship*> ships = play->entity_manage->fetch<ship>();

        for(ship* s : ships)
        {
            check(s);
        }*/
    }
}
