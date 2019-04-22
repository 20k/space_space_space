#include <iostream>
#include <SFML/Graphics.hpp>
#include <imgui/imgui.h>
#include <imgui-sfml/imgui-SFML.h>
#include <imgui/misc/freetype/imgui_freetype.h>
#include "ship_components.hpp"
#include "entity.hpp"
#include <windows.h>
#include <networking/networking.hpp>
#include "radar_field.hpp"
#include "camera.hpp"
#include "stardust.hpp"
#include "player.hpp"
#include "fixed_clock.hpp"
#include "aoe_damage.hpp"
#include "design_editor.hpp"
#include <fstream>

template<sf::Keyboard::Key k, int n, int c>
bool once()
{
    static bool last;

    sf::Keyboard key;

    if(key.isKeyPressed(k) && !last)
    {
        last = true;

        return true;
    }

    if(!key.isKeyPressed(k))
    {
        last = false;
    }

    return false;
}

template<sf::Mouse::Button b, int n, int c>
bool once()
{
    static bool last;

    sf::Mouse mouse;

    if(mouse.isButtonPressed(b) && !last)
    {
        last = true;

        return true;
    }

    if(!mouse.isButtonPressed(b))
    {
        last = false;
    }

    return false;
}

#define ONCE_MACRO(x) once<x, __LINE__, __COUNTER__>()

struct solar_system
{
    asteroid* sun = nullptr;

    std::vector<entity*> asteroids;

    solar_system(entity_manager& entities, alt_radar_field& field)
    {
        std::minstd_rand rng;
        rng.seed(0);

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
    }
};

void server_thread(std::atomic_bool& should_term)
{
    fixed_clock::init = true;

    connection conn;
    conn.host("192.168.0.54", 11000);

    //#define SERVER_VIEW
    #ifdef SERVER_VIEW

    sf::RenderWindow debug(sf::VideoMode(1200, 1200), "debug");

    #endif // SERVER_VIEW

    //alt_radar_field radar({800, 800});

    entity_manager entities;
    alt_radar_field& radar = get_radar_field();
    radar.em = &entities;

    ship* test_ship = entities.make_new<ship>();
    test_ship->network_owner = 0;
    test_ship->r.network_owner = 0;

    component component_launch = get_component_default(component_type::COMPONENT_LAUNCHER, 1);

    test_ship->add(get_component_default(component_type::THRUSTER, 1));
    test_ship->add(get_component_default(component_type::WARP, 1));
    test_ship->add(get_component_default(component_type::SHIELDS, 1));
    test_ship->add(component_launch);
    test_ship->add(get_component_default(component_type::LASER, 1));
    test_ship->add(get_component_default(component_type::SENSOR, 1));
    test_ship->add(get_component_default(component_type::COMMS, 1));
    test_ship->add(get_component_default(component_type::ARMOUR, 1));
    test_ship->add(get_component_default(component_type::LIFE_SUPPORT, 1));
    test_ship->add(get_component_default(component_type::RADIATOR, 1));
    test_ship->add(get_component_default(component_type::POWER_GENERATOR, 1));
    test_ship->add(get_component_default(component_type::CREW, 1));

    blueprint default_missile;

    default_missile.overall_size = 1/15.;
    default_missile.add_component_at(get_component_default(component_type::THRUSTER, 1), {50, 50}, 2);
    default_missile.add_component_at(get_component_default(component_type::SENSOR, 1), {100, 100}, 1);
    default_missile.add_component_at(get_component_default(component_type::POWER_GENERATOR, 1), {150, 150}, 3);
    default_missile.add_component_at(get_component_default(component_type::DESTRUCT, 1), {200, 200}, 4);
    default_missile.add_component_at(get_component_default(component_type::MISSILE_CORE, 1), {250, 250}, 1);
    default_missile.add_component_at(get_component_default(component_type::HEAT_BLOCK, 1), {300, 300}, 2);
    default_missile.add_component_at(get_component_default(component_type::RADIATOR, 1), {350, 350}, 2);
    default_missile.name = "Default Missile";

    const component_fixed_properties& cold_fixed = get_component_fixed_props(component_type::STORAGE_TANK, 1);
    const component_fixed_properties& hot_fixed = get_component_fixed_props(component_type::STORAGE_TANK_HS, 1);
    const component_fixed_properties& destruct_fixed = get_component_fixed_props(component_type::DESTRUCT, 1);


    component destruct = get_component_default(component_type::DESTRUCT, 1);
    component cold_tank = get_component_default(component_type::STORAGE_TANK, 1);
    component hot_tank = get_component_default(component_type::STORAGE_TANK_HS, 1);

    component explosives = get_component_default(component_type::MATERIAL, 1);
    explosives.add_composition(material_info::HTX, destruct_fixed.get_internal_volume(destruct.current_scale));

    destruct.store(explosives);

    component coolant_material = get_component_default(component_type::FLUID, 1);
    coolant_material.add_composition(material_info::HYDROGEN, cold_fixed.get_internal_volume(cold_tank.current_scale));

    component coolant_material2 = get_component_default(component_type::FLUID, 1);
    coolant_material2.add_composition(material_info::HYDROGEN, hot_fixed.get_internal_volume(hot_tank.current_scale));

    cold_tank.store(coolant_material);
    hot_tank.store(coolant_material2);

    test_ship->add(destruct);
    test_ship->add(cold_tank);
    test_ship->add(hot_tank);

    storage_pipe rpipe;
    rpipe.id_1 = cold_tank._pid;
    rpipe.id_2 = hot_tank._pid;
    rpipe.max_flow_rate = 1;

    test_ship->add_pipe(rpipe);

    storage_pipe space_pipe;
    space_pipe.id_1 = hot_tank._pid;
    space_pipe.goes_to_space = true;
    space_pipe.max_flow_rate = 0.5;

    test_ship->add_pipe(space_pipe);

    test_ship->my_size = 10;

    for(component& c : test_ship->components)
    {
        c.scale(test_ship->my_size);
    }

    for(component& c : test_ship->components)
    {
        if(c.has_tag(tag_info::TAG_EJECTOR))
        {
            ship mship = default_missile.to_ship();

            for(component& c : mship.components)
            {
                if(!c.has(component_info::THRUST))
                    continue;

                std::cout << "THR " << c.get_fixed(component_info::THRUST).recharge << std::endl;
            }

            for(component& lc : mship.components)
            {
                if(!lc.has(component_info::SELF_DESTRUCT))
                    continue;

                const component_fixed_properties& fixed = lc.get_fixed_props();

                component expl = get_component_default(component_type::MATERIAL, 1);
                expl.add_composition(material_info::HTX, fixed.get_internal_volume(lc.current_scale));

                lc.store(expl);
            }

            float to_store_volume = mship.get_my_volume();

            std::cout << "MSIZE " << to_store_volume << std::endl;
            std::cout << "INTERNAL VOL " << c.get_internal_volume() << std::endl;

            for(int i=0; i < (c.get_internal_volume() / mship.get_my_volume()) - 1; i++)
            {
                ///need to be able to store ships
                ///a single component on its own should really be a ship... right?
                ///but that's pretty weird because an asteroid will then secretly be a ship when its molten
                ///which isn't totally insane but it is a bit odd
                ///that said... we should definitely be able to fire asteroids out of a cannon so what do i know

                printf("HI THERE\n");

                c.store(mship);
            }
        }
    }

    #if 0
    component thruster, warp, shields, missile, laser, sensor, comms, armour,
    ls, radiator, power_generator, crew, missile_core, destruct, coolant_cold, coolant_hot, heat_block;

    thruster.add(component_info::POWER, -1);
    thruster.add(component_info::THRUST, 100);
    thruster.add(component_info::HP, 0, 1);
    thruster.add_composition(material_info::COPPER, 100);
    thruster.set_heat(20);
    thruster.long_name = "Thruster";
    thruster.short_name = "THRST";
    thruster.activation_type = component_info::SLIDER_ACTIVATION;
    thruster.normalise_volume();

    warp.add(component_info::POWER, -1);
    warp.add(component_info::WARP, 0.5, 10);
    warp.add(component_info::WARP, -0.1);
    warp.add(component_info::WARP, 0.1);
    warp.add(component_info::HP, 0, 5);
    warp.add_composition(material_info::COPPER, 10);
    warp.add_composition(material_info::IRON, 10);
    warp.set_no_drain_on_full_production();
    warp.set_heat(100);
    warp.long_name = "Warp Drive";
    warp.short_name = "WARP";
    warp.activation_type = component_info::TOGGLE_ACTIVATION;
    warp.normalise_volume();

    shields.add(component_info::SHIELDS, 0.5, 50);
    shields.add(component_info::POWER, -3);
    shields.add(component_info::HP, 0, 5);
    shields.add_composition(material_info::IRON, 10);
    shields.add_composition(material_info::COPPER, 5);
    shields.set_no_drain_on_full_production();
    shields.set_heat(200);
    shields.long_name = "Shields";
    shields.short_name = "SHLD";
    shields.activation_type = component_info::SLIDER_ACTIVATION;
    shields.normalise_volume();

    /*missile.add(component_info::POWER, -1);
    missile.add(component_info::WEAPONS, 1);
    missile.add(component_info::HP, 0, 2);
    missile.add(component_info::MISSILE_STORE, 0.01, 10);
    missile.add(tag_info::TAG_EJECTOR);
    missile.add_composition(material_info::IRON, 1);
    missile.set_no_drain_on_full_production();
    missile.set_heat(5);
    missile.activation_type = component_info::TOGGLE_ACTIVATION;

    missile.add_on_use(component_info::MISSILE_STORE, -1, 1);
    missile.subtype = "missile";
    missile.long_name = "Missile Launcher";
    missile.short_name = "MISL";*/


    laser.add(component_info::POWER, -1);
    laser.add(component_info::WEAPONS, 1);
    laser.add(component_info::HP, 0, 2);
    laser.add(component_info::CAPACITOR, 1.5, 20);
    laser.add_composition(material_info::COPPER, 5);
    laser.set_no_drain_on_full_production();
    laser.set_heat(5);
    laser.long_name = "Laser";
    laser.short_name = "LAS";
    laser.activation_type = component_info::TOGGLE_ACTIVATION;

    laser.add_on_use(component_info::CAPACITOR, -10, 1);
    laser.subtype = "laser";

    laser.max_use_angle = M_PI/8;
    laser.normalise_volume();

    //laser.info[3].held = 0;

    sensor.add(component_info::POWER, -1);
    sensor.add(component_info::SENSORS, 1);
    sensor.add(component_info::HP, 0, 1);
    sensor.add_composition(material_info::COPPER, 2);

    sensor.add_on_use(component_info::POWER, -35, 1);

    sensor.set_heat(2);
    sensor.long_name = "Sensors";
    sensor.short_name = "SENS";
    sensor.activation_type = component_info::TOGGLE_ACTIVATION;
    sensor.normalise_volume();

    comms.add(component_info::POWER, -0.5);
    comms.add(component_info::COMMS, 1);
    comms.add(component_info::HP, 0, 1);
    comms.add_composition(material_info::COPPER, 1);
    comms.add_composition(material_info::IRON, 1);
    comms.set_heat(1);
    comms.long_name = "Communications";
    comms.short_name = "COMM";
    comms.activation_type = component_info::TOGGLE_ACTIVATION;
    comms.normalise_volume();

    /*sysrepair.add(component_info::POWER, -1);
    sysrepair.add(component_info::SYSTEM, 1);
    sysrepair.add(component_info::HP, 0.1, 1);*/

    armour.add(component_info::ARMOUR, 0.01, 30);
    armour.add(component_info::POWER, -0.5);
    armour.add(component_info::HP, 0, 10);
    armour.add_composition(material_info::IRON, 30);
    armour.set_no_drain_on_full_production();
    armour.set_heat(5);
    armour.long_name = "Armour";
    armour.short_name = "ARMR";
    armour.activation_type = component_info::TOGGLE_ACTIVATION;
    armour.normalise_volume();

    ls.add(component_info::POWER, -1);
    ls.add(component_info::LIFE_SUPPORT, 1, 20);
    ls.add(component_info::HP, 0.01, 5);
    ls.add_composition(material_info::IRON, 2);
    ls.add_composition(material_info::COPPER, 2);
    //ls.set_no_drain_on_full_production();
    ls.set_heat(10);
    ls.long_name = "Life Support";
    ls.short_name = "LS";
    ls.activation_type = component_info::TOGGLE_ACTIVATION;
    ls.normalise_volume();

    /*coolant.add(component_info::COOLANT, 10, 200);
    coolant.add(component_info::HP, 0, 1);
    coolant.add(component_info::POWER, -1);
    coolant.add_composition(material_info::IRON, 5);
    coolant.set_heat(5);
    coolant.long_name = "I smell like poop";
    coolant.activation_type = component_info::SLIDER_ACTIVATION;*/

    radiator.add(component_info::RADIATOR, 0.1);
    radiator.add(component_info::HP, 0, 1);
    radiator.add_composition(material_info::IRON, 5);
    radiator.long_name = "Radiator";
    radiator.short_name = "RAD";
    radiator.activation_type = component_info::NO_ACTIVATION;
    radiator.normalise_volume();

    power_generator.add(component_info::POWER, 8, 50);
    power_generator.add(component_info::HP, 0, 10);
    power_generator.add_composition(material_info::IRON, 30);
    power_generator.add_composition(material_info::COPPER, 20);
    power_generator.set_heat(8 * 20);
    //power_generator.set_heat_scales_by_production(true, component_info::POWER);

    power_generator.set_complex_no_drain_on_full_production();
    power_generator.long_name = "Power Generator";
    power_generator.short_name = "PWR";
    power_generator.activation_type = component_info::SLIDER_ACTIVATION;
    power_generator.normalise_volume();

    //power_generator.info[1].held = 0;
    //power_generator.info[0].held = 0;

    ///need to add the ability for systems to start depleting if they have insufficient
    ///consumption
    ///or... maybe we just cheat and add a crew death component that's offset by crew replenishment?
    //crew.add(component_info::POWER, 1, 5);
    crew.add(component_info::HP, 0.1, 10);
    crew.add(component_info::LIFE_SUPPORT, -0.5, 1);
    crew.add(component_info::COMMS, 0.1);
    crew.add(component_info::CREW, 0.01, 100);
    crew.add(component_info::CREW, -0.01); ///passive death on no o2
    crew.add_composition(material_info::IRON, 5);
    crew.set_heat(1);
    crew.long_name = "Crew";
    crew.short_name = "CRW";
    crew.activation_type = component_info::NO_ACTIVATION;
    crew.normalise_volume();


    missile_core.add(component_info::HP, 0.0, 5);
    ///hacky until we have the concept of control instead
    missile_core.add(component_info::CREW, 0.01, 50);
    missile_core.add(component_info::CREW, -0.01); ///passive death on no o2
    missile_core.add(component_info::POWER, -1);
    missile_core.add(tag_info::TAG_MISSILE_BEHAVIOUR);
    missile_core.add_composition(material_info::IRON, 2);
    missile_core.add_composition(material_info::COPPER, 2);
    missile_core.set_heat(1);
    missile_core.long_name = "Missile Core";
    missile_core.short_name = "MCRE";
    missile_core.activation_type = component_info::NO_ACTIVATION;
    missile_core.normalise_volume();


    destruct.add(component_info::HP, 0, 1);
    destruct.add(component_info::SELF_DESTRUCT, 1);
    destruct.add_composition(material_info::IRON, 6);
    destruct.internal_volume = 5;
    destruct.long_name = "Self Destruct";
    destruct.short_name = "DEST";
    destruct.activation_type = component_info::NO_ACTIVATION;

    {
        component explosives;
        explosives.add(component_info::HP, 1);
        explosives.add_composition(material_info::HTX, 5);
        explosives.long_name = "HTX";

        destruct.store(explosives);
    }

    destruct.normalise_volume();

    coolant_cold.long_name = "Cold Storage";
    coolant_hot.long_name = "Heat Sink";

    coolant_cold.short_name = "COOL";
    coolant_hot.short_name = "HSNK";

    coolant_cold.activation_type = component_info::NO_ACTIVATION;
    coolant_hot.activation_type = component_info::NO_ACTIVATION;

    coolant_cold.add(component_info::HP, 0, 10);
    coolant_hot.add(component_info::HP, 0, 10);

    coolant_cold.internal_volume = 50;
    coolant_hot.internal_volume = 5;

    coolant_cold.add_composition(material_info::IRON, 10);
    coolant_hot.add_composition(material_info::IRON, 2);

    coolant_hot.heat_sink = true;

    component coolant_material;
    ///????
    coolant_material.add(component_info::HP, 0, 1);
    coolant_material.add_composition(material_info::HYDROGEN, 50);
    coolant_material.long_name = "Fluid";
    coolant_material.short_name = "FLUI";
    coolant_material.flows = true;

    component coolant_material2;
    ///????
    coolant_material2.add(component_info::HP, 0, 1);
    coolant_material2.add_composition(material_info::HYDROGEN, 5);
    coolant_material2.long_name = "Fluid";
    coolant_material2.short_name = "FLUI";
    coolant_material2.flows = true;

    assert(coolant_cold.can_store(coolant_material));
    coolant_cold.store(coolant_material);
    coolant_cold.add_heat_to_stored(500);

    coolant_hot.store(coolant_material2);
    coolant_hot.add_heat_to_stored(500);

    coolant_hot.normalise_volume();
    coolant_cold.normalise_volume();

    heat_block.long_name = "Heat Block";
    heat_block.short_name = "HBLK";

    heat_block.activation_type = component_info::NO_ACTIVATION;
    heat_block.add(component_info::HP, 0, 1);
    heat_block.add_composition(material_info::IRON, 0.1);
    heat_block.add_composition(material_info::LITHIUM, 0.9);

    heat_block.heat_sink = true;
    heat_block.normalise_volume();

    blueprint default_missile;
    default_missile.overall_size = 1;
    default_missile.add_component_at(thruster, {50, 50}, 2);
    default_missile.add_component_at(sensor, {100, 100}, 1);
    default_missile.add_component_at(power_generator, {150, 150}, 3);
    default_missile.add_component_at(destruct, {200, 200}, 4);
    default_missile.add_component_at(missile_core, {250, 250}, 1);
    default_missile.add_component_at(heat_block, {300, 300}, 2);
    default_missile.add_component_at(radiator, {350, 350}, 2);
    default_missile.name = "Default Missile";


    missile.add(component_info::POWER, -1);
    missile.add(component_info::HP, 0, 2);
    missile.add(tag_info::TAG_EJECTOR);
    missile.add_composition(material_info::IRON, 1);
    missile.set_no_drain_on_full_production();
    missile.set_heat(5);
    missile.activation_type = component_info::TOGGLE_ACTIVATION;

    missile.internal_volume = 10;

    missile.long_name = "Component Launcher";
    missile.short_name = "CLNCH";

    {
        ship mship = default_missile.to_ship();

        for(component& c : mship.components)
        {
            if(!c.has(component_info::THRUST))
                continue;

            std::cout << "THR " << c.get(component_info::THRUST).recharge << std::endl;
        }

        float to_store_volume = mship.get_my_volume();

        //std::cout << "MSIZE " << to_store_volume << std::endl;

        for(int i=0; i < (missile.internal_volume / default_missile.overall_size) - 1; i++)
        {
            ///need to be able to store ships
            ///a single component on its own should really be a ship... right?
            ///but that's pretty weird because an asteroid will then secretly be a ship when its molten
            ///which isn't totally insane but it is a bit odd
            ///that said... we should definitely be able to fire asteroids out of a cannon so what do i know

            missile.store(mship);
        }
    }

    std::cout << "num missiles " << missile.stored.size() << std::endl;

    //coolant_cold.scale(50);
    //coolant_hot.scale(10);

    test_ship->add(thruster);
    test_ship->add(warp);
    test_ship->add(shields);
    test_ship->add(missile);
    test_ship->add(laser);
    test_ship->add(sensor);
    test_ship->add(comms);
    //test_ship.add(sysrepair);
    test_ship->add(armour);
    test_ship->add(ls);
    test_ship->add(radiator);
    test_ship->add(power_generator);
    test_ship->add(crew);
    test_ship->add(coolant_cold);
    test_ship->add(coolant_hot);
    test_ship->add(destruct);

    ///HACKY SCALE UNTIL FIX STUFF
    for(component& c : test_ship->components)
    {
        for(material& m : c.composition)
        {
            m.dynamic_desc.volume *= 10;
        }
    }

    storage_pipe rpipe;
    rpipe.id_1 = coolant_cold._pid;
    rpipe.id_2 = coolant_hot._pid;
    rpipe.max_flow_rate = 1;

    test_ship->add_pipe(rpipe);

    storage_pipe space_pipe;
    space_pipe.id_1 = coolant_hot._pid;
    space_pipe.goes_to_space = true;
    space_pipe.max_flow_rate = 0.5;

    test_ship->add_pipe(space_pipe);
    #endif // 0

    test_ship->r.position = {400, 400};

    test_ship->r.position = {498.336609, 529.024292};

    test_ship->r.position = {323.986694, 242.469727};

    //test_ship->r.position = {585, 400};

    ship* test_ship2 = entities.make_new<ship>(*test_ship);
    //test_ship2->data_track.pid = get_next_persistent_id();
    test_ship2->new_network_copy();

    //std::cout << "TS2 " << test_ship2->data_track.pid << std::endl;
    test_ship2->network_owner = 1;
    test_ship2->r.network_owner = 1;

    test_ship2->r.position = {600, 400};

    std::minstd_rand rng;
    rng.seed(0);

    /*test_ship->take_damage(80);

    for(int i=0; i < 10; i++)
        test_ship->take_damage(2);*/

    data_model_manager<ship*> data_manage;

    player_research default_research;
    /*default_research.components.push_back(thruster);
    default_research.components.push_back(warp);
    default_research.components.push_back(shields);
    default_research.components.push_back(missile);
    default_research.components.push_back(laser);
    default_research.components.push_back(sensor);
    default_research.components.push_back(comms);
    default_research.components.push_back(armour);
    default_research.components.push_back(ls);
    default_research.components.push_back(radiator);
    default_research.components.push_back(power_generator);
    default_research.components.push_back(crew);
    default_research.components.push_back(destruct);*/

    //player_model_manager player_manage;

    /*int num_asteroids = 10;

    for(int i=0; i < num_asteroids; i++)
    {
        float ffrac = (float)i / num_asteroids;

        float fangle = ffrac * 2 * M_PI;

        vec2f rpos = rand_det(rng, (vec2f){100, 100}, (vec2f){800, 600});

        asteroid* a = entities.make_new<asteroid>();
        //a->r.position = (vec2f){300, 0}.rot(fangle) + rpos;

        a->r.position = rpos;
    }*/

    solar_system sys(entities, radar);

    double frametime_dt = 1;

    sf::Clock clk;

    sf::Clock frame_pacing_clock;
    double time_between_ticks_ms = 16;

    std::map<uint64_t, sf::Clock> control_elapsed;
    std::map<uint64_t, vec2f> last_mouse_pos;

    sf::Mouse mouse;
    sf::Keyboard key;

    uint32_t iterations = 0;

    bool render = true;

    sf::Clock read_clock;

    #ifdef SERVER_VIEW
    camera cam({debug.getSize().x, debug.getSize().y});
    cam.position = {400, 400};
    #endif // SERVER_VIEW

    /*for(component& c : test_ship->components)
    {
        std::cout << "cpid server " << c._pid << std::endl;
    }*/

    //std::cout << "server ship " << test_ship->_pid << std::endl;

    //entities.use_aggregates = false;

    int stagger_id = 0;

    while(1)
    {
        sf::Clock tickclock;

        double used_frametime_dt = clamp(frametime_dt, 14 / 1000., 18 / 1000.);

        entities.tick(used_frametime_dt);

        double tclock_time = tickclock.getElapsedTime().asMicroseconds() / 1000.;
        //std::cout << "tclock " << tclock_time << std::endl;

        for(auto& i : data_manage.backup)
        {
            player_model* mod = &i.second.networked_model;

            if(mod->controlled_ship == nullptr)
                continue;

            if(mod->controlled_ship->cleanup)
                mod->controlled_ship = nullptr;
        }

        for(auto& m : data_manage.backup)
        {
            for(int i=0; i < (int)m.second.ships.size(); i++)
            {
                if(m.second.ships[i]->cleanup)
                {
                    m.second.ships.erase(m.second.ships.begin() + i);
                    i--;
                    continue;
                }
            }
        }

        entities.cleanup();

        frametime_dt = (clk.restart().asMicroseconds() / 1000.) / 1000.;

        alt_frequency_packet alt_pack;
        alt_pack.intensity = 50000;
        alt_pack.frequency = 1000;

        #ifdef SERVER_VIEW
        /*vec2f mpos = {mouse.getPosition(debug).x, mouse.getPosition(debug).y};

        for(entity* e : entities.entities)
        {
            heatable_entity* heat = dynamic_cast<heatable_entity*>(e);

            if(!heat)
                continue;

            if(heat->point_within(mpos))
            {
                printf("heat %f\n", heat->latent_heat);
            }
        }*/
        #endif // SERVER_VIEW

        //if(key.isKeyPressed(sf::Keyboard::K))
        #ifdef SERVER_VIEW
        if(ONCE_MACRO(sf::Keyboard::K))
        {
            radar.add_packet(alt_pack, {mouse.getPosition(debug).x, mouse.getPosition(debug).y});
        }
        #endif // SERVER_VIEW

        if(ONCE_MACRO(sf::Keyboard::L))
        {
            for(int i=0; i < 100; i++)
            {
                radar.add_packet(alt_pack, {i + 200,200});
            }
        }

        if(ONCE_MACRO(sf::Keyboard::M))
        {
            render = !render;
        }

        radar.tick(used_frametime_dt);
        //player_manage.tick(frametime_dt);

        for(auto& i : data_manage.data)
        {
            i.second.networked_model.tick(used_frametime_dt);
        }

        iterations++;

        #ifdef SERVER_VIEW
        sf::Event event;

        while(debug.pollEvent(event))
        {

        }
        #endif // SERVER_VIEW

        while(conn.has_new_client())
        {
            uint32_t pid = conn.has_new_client().value();

            data_model<ship*>& data = data_manage.fetch_by_id(pid);

            player_model& fmodel = data.networked_model;

            std::vector<ship*> ships = entities.fetch<ship>();

            for(ship* s : ships)
            {
                if(s->network_owner == pid)
                {
                    s->model = &fmodel;

                    fmodel.controlled_ship = s;
                }
            }

            fmodel.research = default_research;
            fmodel.research._pid = get_next_persistent_id();

            get_next_persistent_id();

            if(std::ifstream("temp.blueprint").good())
                fmodel.blueprint_manage.load("temp.blueprint");

            bool has_missile = false;

            for(blueprint& p : fmodel.blueprint_manage.blueprints)
            {
                if(p.name == default_missile.name)
                    has_missile = true;
            }

            /*for(blueprint& p : fmodel.blueprint_manage.blueprints)
            {
                for(blueprint_node& node : p.nodes)
                {
                    std::cout << "node pid " << node.original._pid << std::endl;
                }
            }*/

            if(!has_missile)
                fmodel.blueprint_manage.blueprints.push_back(default_missile);

            if(fmodel.blueprint_manage.blueprints.size() == 0)
                fmodel.blueprint_manage.create_blueprint();

            conn.pop_new_client();
        }

        while(conn.has_read())
        {
            client_input read_data;
            uint64_t read_id = conn.reads_from<client_input>(read_data);

            last_mouse_pos[read_id] = read_data.mouse_world_pos;

            data_model<ship*>& data = data_manage.fetch_by_id(read_id);

            player_model& mod = data.networked_model;

            //std::optional<player_model*> mod_opt = player_manage.fetch_by_network_id(read.id);

            ship* s = dynamic_cast<ship*>(mod.controlled_ship);

            if(s && s->network_owner == read_id)
            {
                ///acceleration etc
                {
                    double time = (control_elapsed[read_id].restart().asMicroseconds() / 1000.) / 1000.;

                    s->apply_force(read_data.direction * time);
                    s->apply_rotation_force(read_data.rotation * time);

                    double thruster_active_percent = read_data.direction.length() + fabs(read_data.rotation);

                    thruster_active_percent = clamp(thruster_active_percent, 0, 1);

                    s->set_thrusters_active(thruster_active_percent);

                    if(read_data.fired.size() > 0)
                    {
                        s->fire(read_data.fired);
                    }

                    if(read_data.ping)
                    {
                        s->ping();
                    }
                }

                {
                    serialise_context ctx;
                    ctx.inf = read_data.rpcs;
                    ctx.exec_rpcs = true;

                    //s->serialise(ctx, ctx.faux);
                    do_recurse(ctx, s);
                }

                {
                    serialise_context ctx;
                    ctx.inf = read_data.rpcs;
                    ctx.exec_rpcs = true;

                    do_recurse(ctx, mod);
                }
            }

            conn.pop_read(read_id);
        }

        if(key.isKeyPressed(sf::Keyboard::P))
            std::cout << "test ship " << test_ship->r.position << std::endl;

        /*for(component& c : test_ship->components)
        {
            if(c._pid == 24)
            {
                std::cout << "activation level " << c.activation_level << " " << c.long_name << " " << c._pid << std::endl;
            }
        }*/

        #define SEE_ONLY_REAL

        auto clients = conn.clients();

        for(auto& i : clients)
        {
            std::vector<ship*> ships;
            std::vector<client_renderable> renderables;

            for(entity* e : entities.entities)
            {
                ship* s = dynamic_cast<ship*>(e);

                if(s)
                {
                    #ifdef SEE_ONLY_REAL
                    if(s->network_owner != i)
                        continue;
                    #endif // SEE_ONLY_REAL

                    ships.push_back(s);
                    renderables.push_back(s->r);

                    if(s->network_owner != i)
                        continue;

                    #ifdef MOUSE_TRACK
                    vec2f mpos = last_mouse_pos[i];

                    vec2f to_mouse = mpos - s->r.position;
                    vec2f front_dir = (vec2f){1, 0}.rot(s->r.rotation);

                    for(component& c : s->components)
                    {
                        if(c.max_use_angle == 0)
                            continue;

                        vec2f clamped = clamp_angle(to_mouse, front_dir, c.max_use_angle);
                        //vec2f clamped = to_mouse;

                        float flen = clamped.length()/4;

                        //flen = clamp(flen, 0, 10);

                        flen = 10;

                        client_renderable init;
                        init.init_rectangular({flen, 0.0});

                        for(auto& i : init.vert_cols)
                        {
                            i.w() = 0.1;
                        }

                        init.rotation = clamped.angle();
                        init.position = s->r.position + clamped.norm() * flen * 2;

                        renderables.push_back(init);
                    }
                    #endif // MOUSE_TRACK

                    #ifdef FIXED_HORN
                    float angle_dist = 10;

                    for(component& c : s->components)
                    {
                        if(c.max_use_angle == 0)
                            continue;

                        client_renderable init;
                        init.init_rectangular({angle_dist, 0.5});
                        init.rotation = s->r.rotation - c.max_use_angle;

                        for(auto& i : init.vert_cols)
                        {
                            i.w() = 0.1;
                        }

                        vec2f svector = (vec2f){1, 0}.rot(s->r.rotation - c.max_use_angle);
                        init.position = s->r.position + svector * angle_dist * 2;

                        renderables.push_back(init);

                        init.rotation = s->r.rotation + c.max_use_angle;
                        svector = (vec2f){1, 0}.rot(s->r.rotation + c.max_use_angle);
                        init.position = s->r.position + svector * angle_dist * 2;

                        renderables.push_back(init);
                    }
                    #endif // FIXED_HORN
                }
                else
                {
                    #ifndef SEE_ONLY_REAL
                    renderables.push_back(e->r);
                    #endif // SEE_ONLY_REAL
                }
            }

            data_model<ship*>& data = data_manage.fetch_by_id(i);

            data.client_network_id = i;
            data.ships = ships;
            data.renderables = renderables;

            player_model* player_model = &data.networked_model;

            //std::cout << "sending blupe " << player_model->blueprint_manage._pid << std::endl;

            if(player_model)
            {
                ship* s = dynamic_cast<ship*>(player_model->controlled_ship);

                if(s)
                {
                    data.sample = radar.sample_for(s->r.position, *s, entities, player_model, s->get_radar_strength());
                }
            }
            else
            {
                data.sample = decltype(data.sample)();
            }

            if(data_manage.backup.find(i) != data_manage.backup.end())
            {
                sf::Clock total_encode;

                nlohmann::json ret = serialise_against(data, data_manage.backup[i], true, stagger_id);

                double partial_time = total_encode.getElapsedTime().asMicroseconds() / 1000.;

                //std::cout << "partial time " << partial_time << std::endl;

                std::vector<uint8_t> cb = nlohmann::json::to_cbor(ret);

                write_data dat;
                dat.id = i;
                dat.data = std::string(cb.begin(), cb.end());

                conn.write_to(dat);

                //std::cout << "partial data " << cb.size() << std::endl;

                //std::cout << ret.dump() << std::endl;

                /*for(auto& i : cb)
                    std::cout << i;

                std::cout << std::endl;*/

                sf::Clock clone_clock;

                ///basically clones model, by applying the current diff to last model
                ///LEAKS MEMORY ON POINTERS
                deserialise(ret, data_manage.backup[i]);

                //double ftime = total_encode.getElapsedTime().asMicroseconds() / 1000.;
                //std::cout << "total " << ftime << std::endl;
            }
            else
            {
                nlohmann::json ret = serialise(data);

                std::vector<uint8_t> cb = nlohmann::json::to_cbor(ret);

                write_data dat;
                dat.id = i;
                dat.data = std::string(cb.begin(), cb.end());

                conn.write_to(dat);

                //std::cout << "full data " << cb.size() << std::endl;

                data_manage.backup[i] = data_model<ship*>();
                serialisable_clone(data, data_manage.backup[i]);

                //conn.writes_to(model, i);
            }

            if(player_model)
            {
                for(auto it = player_model->renderables.begin(); it != player_model->renderables.end();)
                {
                    if(it->second.r.transient)
                    {
                        it = player_model->renderables.erase(it);
                    }
                    else
                    {
                        it++;
                    }
                }
            }
        }

        stagger_id++;

        #ifdef SERVER_VIEW

        if(render)
        {
            entities.render(cam, debug);
            entities.debug_aggregates(cam, debug);

            radar.render(cam, debug);
        }

        debug.display();
        debug.clear();

        #endif // SERVER_VIEW

        //std::cout << "FULL FRAME " << tickclock.restart().asMicroseconds()/1000. << std::endl;

        double elapsed_ms = frame_pacing_clock.getElapsedTime().asMicroseconds() / 1000.;

        double to_sleep = clamp(time_between_ticks_ms - elapsed_ms, 0, time_between_ticks_ms);

        sf::Clock sleep_clock;

        int slept = 0;

        for(int i=0; i < round(to_sleep); i++)
        {
            sf::sleep(sf::milliseconds(1));

            slept++;

            if(sleep_clock.getElapsedTime().asMicroseconds() / 1000. >= round(to_sleep))
                break;
        }

        frame_pacing_clock.restart();

        //std::cout << frametime_dt * 1000 << " slept " << to_sleep << " real slept " << slept << std::endl;

        //Sleep(10);

        fixed_clock::increment();

        if(should_term)
            return;
    }
}

int main()
{
    serialise_tests();

    std::atomic_bool term{0};

    #define WITH_SERVER
    #ifdef WITH_SERVER
    std::thread(server_thread, std::ref(term)).detach();
    #endif // WITH_SERVER

    sf::ContextSettings sett;
    sett.antialiasingLevel = 8;
    sett.sRgbCapable = true;

    sf::RenderWindow window(sf::VideoMode(1600, 900), "hi", sf::Style::Default, sett);

    camera cam({window.getSize().x, window.getSize().y});

    cam.position = {400, 400};

    sf::Texture font_atlas;

    ImGui::SFML::Init(window, false);

    ImGuiIO& io = ImGui::GetIO();
    //io.Fonts->AddFontDefault();
    ImFont* font = io.Fonts->AddFontFromFileTTF("VeraMono.ttf", 14.f);

    ImFontAtlas* atlas = ImGui::SFML::GetFontAtlas();

    auto write_data =  [](unsigned char* data, void* tex_id, int width, int height)
    {
        sf::Texture* tex = (sf::Texture*)tex_id;

        tex->create(width, height);
        tex->update((const unsigned char*)data);
    };

    ImGuiFreeType::BuildFontAtlas(atlas, ImGuiFreeType::ForceAutoHint, ImGuiFreeType::LEGACY);

    write_data((unsigned char*)atlas->TexPixelsNewRGBA32, (void*)&font_atlas, atlas->TexWidth, atlas->TexHeight);

    atlas->TexID = (void*)font_atlas.getNativeHandle();

    ImGuiStyle& style = ImGui::GetStyle();

    style.FrameRounding = 0;
    style.WindowRounding = 0;
    //style.ChildRounding = 0;
    //style.ChildBorderSize = 0;
    style.FrameBorderSize = 0;
    //style.PopupBorderSize = 0;
    //style.WindowBorderSize = 0;

    ImGui::SetStyleLinearColor(sett.sRgbCapable);

    //assert(font != nullptr);

    connection conn;
    conn.connect("192.168.0.54", 11000);
    //conn.connect("77.97.17.179", 11000);

    data_model<ship> model;
    client_entities renderables;

    sf::Clock imgui_delta;
    sf::Clock frametime_delta;

    sf::Keyboard key;
    sf::Mouse mouse;

    entity_manager entities;
    entities.use_aggregates = false;

    entity* ship_proxy = entities.make_new<entity>();

    stardust_manager star_manage(cam, entities);

    alt_radar_sample sample;
    entity_manager transients;
    transients.use_aggregates = false;

    sf::Clock read_clock;

    design_editor design;
    design.open = true;

    while(window.isOpen())
    {
        double frametime_dt = (frametime_delta.restart().asMicroseconds() / 1000.) / 1000.;

        sf::Event event;

        float mouse_delta = 0;

        while(window.pollEvent(event))
        {
            ImGui::SFML::ProcessEvent(event);

            if(event.type == sf::Event::Closed)
            {
                exit(0);

                window.close();
                term = 1;
            }

            if(event.type == sf::Event::Resized)
            {
                window.create(sf::VideoMode(event.size.width, event.size.height), "hi", sf::Style::Default, sett);
            }

            if(event.type == sf::Event::MouseWheelScrolled)
            {
                mouse_delta += event.mouseWheelScroll.delta;
            }
        }

        if(ImGui::IsAnyWindowHovered())
        {
            mouse_delta = 0;
        }

        cam.screen_size = {window.getSize().x, window.getSize().y};
        cam.add_linear_zoom(mouse_delta);

        while(conn.has_read())
        {
            sf::Clock rtime;

            //model.cleanup();
            //std::cout << conn.read() << std::endl;
            uint64_t rdata_id = conn.reads_from<data_model<ship>>(model);
            //renderables = conn.reads_from<client_entities>().data;
            conn.pop_read(rdata_id);


            if(model.sample.fresh)
            {
                sample = model.sample;
            }
            else
            {
                sample.intensities = model.sample.intensities;
                sample.frequencies = model.sample.frequencies;
            }

            //std::cout << "crtime " << rtime.getElapsedTime().asMicroseconds() / 1000. << std::endl;
            //std::cout << (*(model.ships[0].data_track))[component_info::SHIELDS].vsat.size() << std::endl;

            //std::cout << "pid " << model.ships[0].data_track.pid << std::endl;


            sf::Clock copy_time;
            design.research = std::move(model.networked_model.research);
            //std::cout << "copytime " << copy_time.getElapsedTime().asMicroseconds() / 1000. << std::endl;
            design.server_blueprint_manage = model.networked_model.blueprint_manage;

            //std::cout << "RSBM " << design.server_blueprint_manage._pid << " serv " << model.networked_model.blueprint_manage._pid << std::endl;

            renderables.entities = model.renderables;
        }

        for(client_renderable& r : model.renderables)
        {
            if(r.network_owner == model.client_network_id)
            {
                ship_proxy->r.position = r.position;

                cam.position = r.position;
            }
        }

        entities.cleanup();
        transients.cleanup();
        tick_radar_data(transients, sample, ship_proxy);
        entities.tick(frametime_dt);
        transients.tick(frametime_dt);
        design.tick(frametime_dt);

        vec2f mpos = {mouse.getPosition(window).x, mouse.getPosition(window).y};
        //vec2f mfrac = mpos / (vec2f){window.getSize().x, window.getSize().y};

        ImGui::SFML::Update(window,  imgui_delta.restart());

        renderables.render(cam, window);

        client_input cinput;

        float forward_vel = 0;

        forward_vel += key.isKeyPressed(sf::Keyboard::W);
        forward_vel -= key.isKeyPressed(sf::Keyboard::S);

        float angular_vel = 0;

        angular_vel += key.isKeyPressed(sf::Keyboard::D);
        angular_vel -= key.isKeyPressed(sf::Keyboard::A);

        cinput.direction = (vec2f){forward_vel, 0};
        cinput.rotation = angular_vel;

        if(ONCE_MACRO(sf::Keyboard::Space))
        {
            client_fire fire;
            fire.weapon_offset = 0;

            cinput.fired.push_back(fire);

            //cinput.fired = true;
        }

        if(ONCE_MACRO(sf::Keyboard::Q))
        {
            cinput.ping = true;
        }

        if(ONCE_MACRO(sf::Keyboard::E))
        {
            client_fire fire;
            fire.weapon_offset = 1;

            vec2f relative_pos = cam.screen_to_world(mpos) - ship_proxy->r.position;

            fire.fire_angle = relative_pos.angle();

            cinput.fired.push_back(fire);
        }

        if(ONCE_MACRO(sf::Keyboard::F1))
        {
            design.open = !design.open;
        }

        cinput.mouse_world_pos = cam.screen_to_world(mpos);
        cinput.rpcs = get_global_serialise_info();

        get_global_serialise_info().all_rpcs.clear();

        conn.writes_to(cinput, -1);

        //sf::Clock showtime;

        for(ship& s : model.ships)
        {
            /*ImGui::Begin(std::to_string(s.network_owner).c_str());

            ImGui::Text(s.show_resources().c_str());

            ImGui::End();*/


            //printf("fpid %i\n", s._pid);

            s.show_resources();

            s.advanced_ship_display();

            s.show_power();
        }

        //std::cout << "showtime " << showtime.getElapsedTime().asMicroseconds() / 1000. << std::endl;

        if(model.ships.size() > 0)
        {
            /*for(component& c : model.ships[0].components)
            {
                std::cout << "cpid client " << c._pid << std::endl;
            }*/

            //std::cout << "client ship " << model.ships[0]._pid << std::endl;

            //std::cout << "mnum " << model.ships[0].components.size() << std::endl;
        }

        render_radar_data(window, sample);

        entities.render(cam, window);
        transients.render(cam, window);
        design.render(window);

        sf::Clock render_clock;

        ImGui::SFML::Render(window);

        //std::cout << "rclock " << render_clock.getElapsedTime().asMicroseconds() / 1000. << std::endl;

        window.display();
        window.clear();

        sf::sleep(sf::milliseconds(4));

        std::cout << "frametime " << frametime_dt << std::endl;
    }

    return 0;
}
