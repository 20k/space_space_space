#include "playspace.hpp"
#include <networking/serialisable.hpp>
#include "entity.hpp"
#include "radar_field.hpp"
#include "ship_components.hpp"

void room::add(entity* e)
{
    for(auto& i : entities)
    {
        if(i == e)
            return;
    }

    entities.push_back(e);
}

bool room::rem(entity* e)
{
    for(int i=0; i < (int)entities.size(); i++)
    {
        if(entities[i] == e)
        {
            entities.erase(entities.begin() + i);
            return true;
        }
    }

    return false;
}

void room::serialise(serialise_context& ctx, nlohmann::json& data, self_t* other)
{
    DO_SERIALISE(friendly_id);
    DO_SERIALISE(type);
    DO_SERIALISE(name);
    DO_SERIALISE(position);
    DO_SERIALISE(entities);
}

void playspace::serialise(serialise_context& ctx, nlohmann::json& data, self_t* other)
{
    DO_SERIALISE(friendly_id);
    DO_SERIALISE(type);
    DO_SERIALISE(name);
    DO_SERIALISE(position);
    DO_SERIALISE(entities);
}

playspace::playspace()
{
    field = new alt_radar_field({800, 800});
}

playspace::~playspace()
{
    delete field;
}

room* playspace::make_room(vec2f where)
{
    room r;
    r.position = where;

    rooms.push_back(r);

    return &rooms.back();
}

void playspace::init_default(entity_manager& entities)
{
    std::minstd_rand rng;
    rng.seed(0);

    {
        vec2f pos = 200;
        float dim = 50;

        room* test = make_room({pos.x(), pos.y()});

        int num_asteroids = 100;

        for(int i=0; i < num_asteroids; i++)
        {
            //float ffrac = (float)i / num_asteroids;

            float ffrac = rand_det_s(rng, 0, 1);

            //float fangle = ffrac * 2 * M_PI;

            //vec2f rpos = rand_det(rng, (vec2f){100, 100}, (vec2f){800, 600});

            //float rdist = rand_det_s(rng, 1, dim);

            //vec2f found_pos = (vec2f){rdist, 0}.rot(fangle) + (vec2f){400, 400};

            vec2f found_pos = rand_det(rng, pos - dim, pos + dim);

            bool cont = false;

            for(entity* e : entities.entities)
            {
                if((e->r.position - found_pos).length() < 10)
                {
                    cont = true;
                    break;
                }
            }

            if(cont)
            {
                i--;
                continue;
            }

            asteroid* a = entities.make_new<asteroid>();
            a->init(1, 1);
            a->r.position = found_pos;
            a->ticks_between_collisions = 2;

            test->add(a);

            entities.cleanup();
        }
    }

    #if 0
    int num_asteroids = 1000;

    for(int i=0; i < num_asteroids; i++)
    {
        //float ffrac = (float)i / num_asteroids;

        float ffrac = rand_det_s(rng, 0, 1);

        float fangle = ffrac * 2 * M_PI;

        //vec2f rpos = rand_det(rng, (vec2f){100, 100}, (vec2f){800, 600});

        float rdist = rand_det_s(rng, 30, 6000);

        vec2f found_pos = (vec2f){rdist, 0}.rot(fangle) + (vec2f){400, 400};

        bool cont = false;

        for(entity* e : entities.entities)
        {
            if((e->r.position - found_pos).length() < 100)
            {
                cont = true;
                break;
            }
        }

        if(cont)
        {
            i--;
            continue;
        }

        asteroid* a = entities.make_new<asteroid>();
        a->init(2, 4);
        a->r.position = found_pos;
        a->ticks_between_collisions = 2;
        //a->permanent_heat = 100;

        asteroids.push_back(a);

        entities.cleanup();
    }

    /*float solar_size = STANDARD_SUN_EMISSIONS_RADIUS;

    ///intensity / ((it * sol) * (it * sol)) = 0.1

    ///intensity / (dist * dist) = 0.1

    float intensity = 0.1 * (solar_size * solar_size);*/

    float intensity = STANDARD_SUN_HEAT_INTENSITY;

    sun = entities.make_new<asteroid>();
    sun->init(3, 4);
    sun->r.position = {400, 400};
    sun->permanent_heat = intensity;
    sun->reflectivity = 0;

    field.sun_id = sun->id;
    #endif // 0



}

void playspace_manager::serialise(serialise_context& ctx, nlohmann::json& data, self_t* other)
{
    DO_SERIALISE(spaces);
}
