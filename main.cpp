#include <iostream>
#include <SFML/Graphics.hpp>
#include <imgui/imgui.h>
#include <imgui-sfml/imgui-SFML.h>
#include <imgui/misc/freetype/imgui_freetype.h>
#include "ship_components.hpp"
#include "entity.hpp"
#include <windows.h>
#include <networking/networking.hpp>
#include <networking/netinterpolate.hpp>
#include "radar_field.hpp"
#include "camera.hpp"
#include "stardust.hpp"
#include "player.hpp"
#include "fixed_clock.hpp"
#include "aoe_damage.hpp"
#include "design_editor.hpp"
#include <fstream>
#include <nauth/auth.hpp>
#include <nauth/steam_auth.hpp>
#include <nauth/steam_api.hpp>
#include "playspace.hpp"
#include "format.hpp"
#include "ui_util.hpp"
#include "colours.hpp"
#include "script_execution.hpp"
#include "code_editor.hpp"
#include "audio.hpp"
#include "prebaked_programs.hpp"

bool skip_keyboard_input(bool has_focus)
{
    if(!has_focus)
        return true;

    if(ImGui::GetCurrentContext() == nullptr)
        return false;

    bool skip = ImGui::GetIO().WantCaptureKeyboard;

    return skip;
}

template<int c>
bool once(sf::Keyboard::Key k, bool has_focus)
{
    static std::map<sf::Keyboard::Key, bool> last;

    if(!has_focus)
    {
        for(auto& i : last)
            i.second = false;

        return false;
    }

    bool skip = skip_keyboard_input(has_focus);

    sf::Keyboard key;

    if(key.isKeyPressed(k) && !skip && !last[k])
    {
        last[k] = true;

        return true;
    }

    if(!key.isKeyPressed(k))
    {
        last[k] = false;
    }

    return false;
}

template<int c>
bool once(sf::Mouse::Button b, bool has_focus)
{
    static std::map<sf::Mouse::Button, bool> last;

    if(!has_focus)
    {
        for(auto& i : last)
            i.second = false;

        return false;
    }

    sf::Mouse mouse;

    if(mouse.isButtonPressed(b) && !last[b])
    {
        last[b] = true;

        return true;
    }

    if(!mouse.isButtonPressed(b))
    {
        last[b] = false;
    }

    return false;
}

#define ONCE_MACRO(x, y) once<__COUNTER__>(x, y)

void db_pid_saver(size_t cur, size_t requested, void* udata)
{
    assert(udata);

    db_backend& db = *(db_backend*)udata;

    db_read_write tx(db, DB_PERSIST_ID);

    std::string data;
    data.resize(sizeof(size_t));

    size_t to_write = cur + requested;

    memcpy(&data[0], &to_write, sizeof(size_t));

    tx.write("pid", data);
}

void server_thread(std::atomic_bool& should_term)
{
    set_db_location("./db");
    set_num_dbs(3);

    fixed_clock::init = true;

    connection conn;
    conn.host("192.168.0.54", 11000);
    set_pid_callback(db_pid_saver);
    set_pid_udata((void*)&get_db());

    {
        size_t persist_id_saved = 0;

        db_read_write tx(get_db(), DB_PERSIST_ID);

        std::optional<db_data> opt = tx.read("pid");

        if(opt)
        {
            db_data& dat = opt.value();

            if(dat.data.size() > 0)
            {
                assert(dat.data.size() == sizeof(size_t));

                memcpy(&persist_id_saved, &dat.data[0], sizeof(size_t));

                set_next_persistent_id(persist_id_saved);

                std::cout << "loaded pid " << persist_id_saved << std::endl;
            }
        }

        if(persist_id_saved < 1024)
            persist_id_saved = 1024;
    }

    cpu_tests();

    //#define SERVER_VIEW
    #ifdef SERVER_VIEW

    sf::RenderWindow debug(sf::VideoMode(1200, 1200), "debug");

    #endif // SERVER_VIEW

    //alt_radar_field radar({800, 800});


    playspace_manager playspace_manage;

    playspace* sys_1 = playspace_manage.make_new();
    sys_1->init_default(0);
    sys_1->name = "P-C";

    sys_1->position = {100, 0};

    playspace* sys_2 = playspace_manage.make_new();
    sys_2->init_default(10000);
    sys_2->name = "A-C";

    playspace* sys_3 = playspace_manage.make_new();
    sys_3->init_default(20000);
    sys_3->name = "C-C";

    playspace* sys_4 = playspace_manage.make_new();
    sys_4->init_default(30000);
    sys_4->name = "C-D";

    playspace_connect(sys_1, sys_2);
    playspace_connect(sys_2, sys_3);
    playspace_connect(sys_3, sys_4);


    //entity_manager entities;
    //alt_radar_field& radar = get_radar_field();

    ship* test_ship;

    {
        entity_manager entities;

        test_ship = entities.make_new<ship>();
        test_ship->network_owner = 0;
        test_ship->r.network_owner = 0;

        sys_1->add(test_ship);
    }

    component component_launch = get_component_default(component_type::COMPONENT_LAUNCHER, 1);
    component test_power = get_component_default(component_type::POWER_GENERATOR, 1);
    test_power.scale(2);

    test_ship->add(get_component_default(component_type::THRUSTER, 1));
    test_ship->add(get_component_default(component_type::S_DRIVE, 1));
    test_ship->add(get_component_default(component_type::W_DRIVE, 1));
    test_ship->add(get_component_default(component_type::SHIELDS, 1));
    test_ship->add(component_launch);
    test_ship->add(get_component_default(component_type::LASER, 1));
    test_ship->add(get_component_default(component_type::SENSOR, 1));
    //test_ship->add(get_component_default(component_type::COMMS, 1));
    test_ship->add(get_component_default(component_type::ARMOUR, 1));
    //test_ship->add(get_component_default(component_type::LIFE_SUPPORT, 1));
    test_ship->add(get_component_default(component_type::RADIATOR, 1));
    test_ship->add(test_power);
    //test_ship->add(get_component_default(component_type::CREW, 1));
    test_ship->add(get_component_default(component_type::MINING_LASER, 1));
    test_ship->add(get_component_default(component_type::REFINERY, 1));
    test_ship->add(get_component_default(component_type::FACTORY, 1));
    test_ship->add(get_component_default(component_type::CARGO_STORAGE, 1));
    test_ship->add(get_component_default(component_type::RADAR, 1));
    test_ship->add(get_component_default(component_type::CPU, 1));

    #if 0
    {
        storage_pipe rpipe;

        int cnum = test_ship->components.size();

        //size_t id_1 = -1;
        size_t id_2 = -1;
        size_t id_3 = -1;

        for(component& c : test_ship->components)
        {
            /*if(c.base_id == component_type::MINING_LASER)
                id_1 = c._pid;*/

            if(c.base_id == component_type::REFINERY)
                id_2 = c._pid;

            if(c.base_id == component_type::FACTORY)
                id_3 = c._pid;
        }

        //rpipe.id_1 = id_1;
        //rpipe.id_2 = id_2;
        //rpipe.max_flow_rate = 1;

        //test_ship->add_pipe(rpipe);


        storage_pipe rpipe2;
        rpipe2.id_1 = id_2;
        rpipe2.id_2 = id_3;
        rpipe2.max_flow_rate = 1;

        test_ship->add_pipe(rpipe2);
    }
    #endif // 0

    blueprint default_missile;

    default_missile.overall_size = (1/15.);
    default_missile.add_component_at(get_component_default(component_type::THRUSTER, 1), {50, 50}, 2);
    default_missile.add_component_at(get_component_default(component_type::SENSOR, 4), {100, 100}, 1);
    default_missile.add_component_at(get_component_default(component_type::RADAR, 4), {100, 100}, 1);
    default_missile.add_component_at(get_component_default(component_type::POWER_GENERATOR, 1), {150, 150}, 3);
    default_missile.add_component_at(get_component_default(component_type::DESTRUCT, 1), {200, 200}, 1);
    //default_missile.add_component_at(get_component_default(component_type::MISSILE_CORE, 1), {250, 250}, 1);
    default_missile.add_component_at(get_component_default(component_type::HEAT_BLOCK, 1), {300, 300}, 2);
    default_missile.add_component_at(get_component_default(component_type::RADIATOR, 1), {350, 350}, 2);
    default_missile.add_component_at(get_component_default(component_type::CARGO_STORAGE, 1), {400, 400}, 4);
    default_missile.add_component_at(get_component_default(component_type::CPU, 1), {400, 400}, 1);
    default_missile.name = "DEFAULT_MISSILE";
    default_missile.add_tag("missile");

    const component_fixed_properties& cold_fixed = get_component_fixed_props(component_type::STORAGE_TANK, 1);
    const component_fixed_properties& hot_fixed = get_component_fixed_props(component_type::STORAGE_TANK_HS, 1);
    //const component_fixed_properties& destruct_fixed = get_component_fixed_props(component_type::DESTRUCT, 1);


    component destruct = get_component_default(component_type::DESTRUCT, 1);
    component cold_tank = get_component_default(component_type::STORAGE_TANK, 1);
    component hot_tank = get_component_default(component_type::STORAGE_TANK_HS, 1);

    /*component explosives = get_component_default(component_type::MATERIAL, 1);
    explosives.add_composition(material_info::HTX, destruct_fixed.get_internal_volume(destruct.current_scale));

    destruct.store(explosives);*/

    component coolant_material = get_component_default(component_type::MATERIAL, 1);
    coolant_material.add_composition(material_info::HYDROGEN, cold_fixed.get_internal_volume(cold_tank.current_scale));

    component coolant_material2 = get_component_default(component_type::MATERIAL, 1);
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
        ship mship = default_missile.to_ship();

        if(c.has_tag(tag_info::TAG_EJECTOR))
        {
            for(component& c : mship.components)
            {
                if(c.base_id == component_type::CPU)
                {
                    c.cpu_core.set_program(fetch_program("missile").value().program);
                }
            }

            for(component& c : mship.components)
            {
                if(!c.has(component_info::THRUST))
                    continue;

                std::cout << "THR " << c.get_fixed(component_info::THRUST).recharge << std::endl;
            }

            for(component& lc : mship.components)
            {
                //if(!lc.has(component_info::CARGO_STORAGE))
                //    continue;

                if(lc.base_id != component_type::CARGO_STORAGE)
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

                mship.new_network_copy();
                c.store(mship);
            }
        }

        if(c.base_id == component_type::CARGO_STORAGE)
        {
            component mat_1 = get_component_default(component_type::MATERIAL, 1);
            component mat_2 = get_component_default(component_type::MATERIAL, 1);
            component mat_3 = get_component_default(component_type::MATERIAL, 1);
            component mat_4 = get_component_default(component_type::MATERIAL, 1);
            mat_1.add_composition(material_info::IRON, 1);
            mat_2.add_composition(material_info::COPPER, 1);
            mat_3.add_composition(material_info::LITHIUM, 1);
            mat_4.add_composition(material_info::HTX, 1);

            c.store(mat_1);
            c.store(mat_2);
            c.store(mat_3);
            c.store(mat_4);

            mship.new_network_copy();
            c.store(mship);
        }
    }

    test_ship->r.position = {400, 400};

    test_ship->r.position = {498.336609, 529.024292};

    //test_ship->r.position = {323.986694, 242.469727};

    test_ship->r.position = {175, 250};

    //test_ship->r.position = {585, 400};

    /*ship* test_ship2 = entities.make_new<ship>(*test_ship);
    //test_ship2->data_track.pid = get_next_persistent_id();
    test_ship2->new_network_copy();

    //std::cout << "TS2 " << test_ship2->data_track.pid << std::endl;
    test_ship2->network_owner = 1;
    test_ship2->r.network_owner = 1;

    test_ship2->r.position = {600, 400};*/

    {
        entity_manager entities;

        ship* ts2 = entities.make_new<ship>(*test_ship);
        ts2->new_network_copy();
        ts2->network_owner = 1;
        ts2->r.network_owner = 1;

        sys_1->add(ts2);

        ts2->r.position = {600, 400};
    }

    std::minstd_rand rng;
    rng.seed(0);

    /*test_ship->take_damage(80);

    for(int i=0; i < 10; i++)
        test_ship->take_damage(2);*/

    data_model_manager<ship*> data_manage;

    player_research default_research;

    for(int i=0; i < (int)component_type::COUNT; i++)
    {
        default_research.components.push_back(get_component_default((component_type::type)i, 1));
    }

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

    //solar_system sys(entities, radar);

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
    cam.position = {-cam.screen_size.x()/2, -cam.screen_size.y()/2};
    #endif // SERVER_VIEW

    /*for(component& c : test_ship->components)
    {
        std::cout << "cpid server " << c._pid << std::endl;
    }*/

    //std::cout << "server ship " << test_ship->_pid << std::endl;

    //entities.use_aggregates = false;

    int stagger_id = 0;

    auth_manager<auth_data> auth_manage;

    std::string secret_key = "secret/akey.ect";
    uint64_t net_code_appid = 814820;

    set_app_description({secret_key, net_code_appid});

    while(1)
    {
        sf::Clock whole_frametime;

        sf::Clock tickclock;

        double used_frametime_dt = clamp(frametime_dt, 14 / 1000., 18 / 1000.);

        //entities.tick(used_frametime_dt);

        playspace_manage.tick(used_frametime_dt);

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

        #define CCC
        #ifdef CCC
        #ifdef SERVER_VIEW
        cam.position.x() += key.isKeyPressed(sf::Keyboard::Numpad6)*10;
        cam.position.x() -= key.isKeyPressed(sf::Keyboard::Numpad4)*10;
        cam.position.y() += key.isKeyPressed(sf::Keyboard::Numpad2)*10;
        cam.position.y() -= key.isKeyPressed(sf::Keyboard::Numpad8)*10;
        #endif // SERVER_VIEW
        #endif // CCC

        //entities.cleanup();

        frametime_dt = (clk.restart().asMicroseconds() / 1000.) / 1000.;

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


        /*if(ONCE_MACRO(sf::Keyboard::L, true))
        {
            for(int i=0; i < 100; i++)
            {
                radar.add_packet(alt_pack, {i + 200,200});
            }
        }*/

        #ifdef SERVER_VIEW
        if(ONCE_MACRO(sf::Keyboard::M, true))
        {
            render = !render;
        }
        #endif // SERVER_VIEW

        //radar.tick(entities, used_frametime_dt);
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
            /*
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
            }*/

            conn.pop_new_client();
        }

        while(conn.has_read())
        {
            nlohmann::json network_json;
            uint64_t read_id = -1;

            std::optional<auth<auth_data>*> found_auth;

            {
                network_protocol proto;

                read_id = conn.reads_from(proto);

                if(proto.type == network_mode::STEAM_AUTH)
                {
                    std::string hex = proto.data;

                    auth_manage.handle_steam_auth(read_id, hex, get_db());
                }

                if(!auth_manage.authenticated(read_id))
                {
                    printf("DENIED\n");

                    conn.pop_read(read_id);
                    continue;
                }

                found_auth = auth_manage.fetch(read_id);

                if(found_auth.has_value() && proto.type == network_mode::STEAM_AUTH)
                {
                    data_model<ship*>& data = data_manage.fetch_by_id(read_id);

                    player_model& fmodel = data.networked_model;

                    {
                        db_read tx(get_db(), DB_USER_ID);

                        data.persistent_data.load(found_auth.value()->user_id, tx);
                    }

                    data.persistent_data.research.merge_into_me(default_research);

                    std::vector<ship*> s1;

                    for(playspace* space : playspace_manage.spaces)
                    {
                        auto s2 = space->entity_manage->fetch<ship>();

                        for(auto& i : s2)
                            s1.push_back(i);

                        for(room* r : space->rooms)
                        {
                            auto s3 = r->entity_manage->fetch<ship>();

                            for(auto& i : s3)
                                s1.push_back(i);
                        }
                    }

                    for(ship* s : s1)
                    {
                        if(s->network_owner == read_id)
                        {
                            s->persistent_data = &data.persistent_data;

                            fmodel.controlled_ship = s;
                        }
                    }
                }

                if(found_auth.has_value() && !found_auth.value()->data.default_init)
                {
                    uint32_t pid = read_id;
                    data_model<ship*>& data = data_manage.fetch_by_id(pid);

                    persistent_user_data& user_data = data.persistent_data;

                    user_data.research = default_research;
                    user_data.research._pid = get_next_persistent_id();

                    user_data.blueprint_manage.blueprints.push_back(default_missile);

                    found_auth.value()->data.default_init = true;

                    {
                        db_read_write tx(get_db(), AUTH_DB_ID);

                        found_auth.value()->save(tx);
                    }

                    {
                        db_read_write tx(get_db(), DB_USER_ID);

                        data.persistent_data.save(tx);
                    }
                }

                if(proto.type == network_mode::DATA)
                {
                    network_json = std::move(proto.data);
                }

                conn.pop_read(read_id);
            }

            /*client_input read_data;
            uint64_t read_id = conn.reads_from<client_input>(read_data);*/

            client_input read_data = deserialise<client_input>(network_json);

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

                    if(s->room_type == space_type::REAL_SPACE)
                    {
                        s->apply_force(read_data.direction * time);
                        s->apply_rotation_force(read_data.rotation * time);

                        double thruster_active_percent = read_data.direction.length() + fabs(read_data.rotation);

                        thruster_active_percent = clamp(thruster_active_percent, 0, 1);

                        if(!s->travelling_in_realspace)
                            s->set_thrusters_active(thruster_active_percent);
                    }

                    if(read_data.fired.size() > 0)
                    {
                        s->fire(read_data.fired);
                    }

                    if(read_data.ping)
                    {
                        s->ping();
                    }
                }

                if(read_data.rpcs.all_rpcs.size() > 0)
                {
                    serialise_context ctx;
                    ctx.inf = read_data.rpcs;
                    ctx.exec_rpcs = true;

                    do_recurse(ctx, s);
                }

                /*{
                    serialise_context ctx;
                    ctx.inf = read_data.rpcs;
                    ctx.exec_rpcs = true;

                    do_recurse(ctx, mod);
                }*/

                /*if(read_data.to_fsd_space)
                {
                    playspace_manage.exit_room(s);
                }

                if(read_data.to_poi_space)
                {
                    auto opt = playspace_manage.get_nearby_room(s);

                    if(opt)
                    {
                        playspace_manage.enter_room(s, opt.value());
                    }
                }*/

                if(read_data.to_fsd_space)
                    s->move_up = true;

                if(read_data.to_poi_space)
                    s->move_down = true;

                if(read_data.warp.should_warp)
                {
                    playspace_manage.start_warp_travel(*s, read_data.warp.sys_pid);
                }

                if(read_data.travel.should_travel)
                {
                    playspace_manage.start_room_travel(*s, read_data.travel.poi_pid);
                }
            }

            if(read_data.rpcs.all_rpcs.size() > 0)
            {
                serialise_context ctx;
                ctx.inf = read_data.rpcs;
                ctx.exec_rpcs = true;

                do_recurse(ctx, data.persistent_data);
            }

            if(data.persistent_data.blueprint_manage.dirty)
            {
                db_read_write tx(get_db(), DB_USER_ID);

                data.persistent_data.save(tx);

                data.persistent_data.blueprint_manage.dirty = false;
            }
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

        ///migrate radar ticking into playspace
        ///come up with solution to radar locality (aka globalness)
        for(auto& i : clients)
        {
            data_model<ship*>& data = data_manage.fetch_by_id(i);

            ship* s = dynamic_cast<ship*>(data.networked_model.controlled_ship);

            if(s)
            {
                data.sample = s->last_sample;
                data.controlled_ship_id = s->_pid;

                //data.sample = radar.sample_for(s->r.position, *s, entities, true, s->get_radar_strength());
            }

            ship_network_data network_ships = playspace_manage.get_network_data_for(s, i);

            data.client_network_id = i;
            data.ships = std::move(network_ships.ships);
            data.renderables = std::move(network_ships.renderables);

            data.labels.clear();
            data.connected_systems.clear();

            if(s)
            {
                auto connected_systems = playspace_manage.get_connected_systems_for(s);

                for(playspace* p : connected_systems)
                {
                    system_descriptor desc;
                    desc.name = p->name;
                    desc.position = p->position;
                    desc.sys_pid = p->_pid;

                    data.connected_systems.push_back(desc);
                }

                auto [play, found_room] = playspace_manage.get_location_for(s);

                if(play && found_room)
                {
                    data.room_position = found_room->position;
                }
            }

            data.labels = network_ships.pois;

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


            ///SO THIS IS KIND OF HACKY

            if(s)
            {
                for(component& c : s->components)
                {
                    c.cpu_core.audio = shared_audio();
                }
            }

            /*if(player_model)
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
            }*/
        }

        stagger_id++;

        #ifdef SERVER_VIEW

        if(render)
        {
            /*entities.render(cam, debug);
            entities.debug_aggregates(cam, debug);

            radar.render(cam, debug);*/

            for(playspace* play : playspace_manage.spaces)
            {
                for(room* r : play->rooms)
                {
                    if(r->entity_manage->contains(test_ship))
                    {
                        r->entity_manage->render(cam, debug);
                        r->field->render(cam, debug);
                        r->entity_manage->debug_aggregates(cam, debug);
                        break;
                    }
                }

                if(play->entity_manage->contains(test_ship))
                {
                    play->entity_manage->render(cam, debug);
                    play->field->render(cam, debug);
                    play->entity_manage->debug_aggregates(cam, debug);
                    break;
                }
            }
        }

        debug.display();
        debug.clear();

        #endif // SERVER_VIEW

        std::cout << "Whole frametime " << whole_frametime.getElapsedTime().asMicroseconds() / 1000. << std::endl;

        //std::cout << "FULL FRAME " << tickclock.restart().asMicroseconds()/1000. << std::endl;

        double elapsed_ms = frame_pacing_clock.getElapsedTime().asMicroseconds() / 1000.;

        double to_sleep = clamp(time_between_ticks_ms - elapsed_ms, 0, time_between_ticks_ms);

        #ifndef PERF_TESTING
        sf::Clock sleep_clock;

        int slept = 0;

        for(int i=0; i < round(to_sleep); i++)
        {
            sf::sleep(sf::milliseconds(1));

            slept++;

            if(sleep_clock.getElapsedTime().asMicroseconds() / 1000. >= round(to_sleep))
                break;
        }
        #endif // PERF_TESTING

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

    #ifndef SERVER_ONLY
    #ifndef CLIENT_ONLY
    #define WITH_SERVER
    #ifdef WITH_SERVER
    std::thread(server_thread, std::ref(term)).detach();
    #endif // WITH_SERVER
    #endif // CLIENT_ONLY
    #endif // SERVER_ONLY

    #ifdef SERVER_ONLY
    server_thread(term);
    exit(0);
    #endif // SERVER_ONLY

    sf::ContextSettings sett;
    sett.antialiasingLevel = 8;
    sett.sRgbCapable = true;
    sett.majorVersion = 3;
    sett.minorVersion = 3;

    sf::RenderWindow window(sf::VideoMode(1600, 900), "hi", sf::Style::Default, sett);

    camera poi_cam({window.getSize().x, window.getSize().y});
    camera sspace_cam({window.getSize().x, window.getSize().y});

    poi_cam.position = {400, 400};
    sspace_cam.position = {400, 400};

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
    #ifndef CLIENT_ONLY
    conn.connect("192.168.0.54", 11000);
    #else
    conn.connect("77.97.17.179", 11000);
    #endif // CLIENT_ONLY

    data_model<ship> model;
    client_entities renderables;

    sf::Clock imgui_delta;
    sf::Clock frametime_delta;

    sf::Keyboard key;
    sf::Mouse mouse;

    entity_manager entities;
    entities.use_aggregates = false;

    entity_manager drawables;
    drawables.use_aggregates = false;

    entity* ship_proxy = entities.make_new<entity>();

    stardust_manager star_manage(poi_cam, entities);

    alt_radar_sample sample;
    entity_manager transients;
    transients.use_aggregates = false;

    sf::Clock read_clock;

    design_editor design(model.persistent_data.research);
    design.open = true;

    steamapi api;

    if(!api.enabled)
        throw std::runtime_error("No steam");

    {
        api.request_auth_token("");

        while(!api.auth_success())
        {
            api.pump_callbacks();
        }

        std::vector<uint8_t> vec = api.get_encrypted_token();

        std::string str(vec.begin(), vec.end());

        network_protocol proto;
        proto.type = network_mode::STEAM_AUTH;
        proto.data = binary_to_hex(str, false);

        conn.writes_to(proto, -1);
    }

    bool focus = true;
    vec2f last_s_size = {0,0};

    std::map<size_t, code_editor> editors;

    shared_audio audio;

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

            if(event.type == sf::Event::LostFocus)
            {
                focus = false;
            }

            if(event.type == sf::Event::GainedFocus)
            {
                focus = true;
            }
        }

        if(ImGui::IsAnyWindowHovered())
        {
            mouse_delta = 0;
        }

        poi_cam.screen_size = {window.getSize().x, window.getSize().y};
        poi_cam.add_linear_zoom(mouse_delta);

        sspace_cam.screen_size = {window.getSize().x, window.getSize().y};
        sspace_cam.add_linear_zoom(mouse_delta);

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
            //design.research = std::move(model.persistent_data.research);
            //std::cout << "copytime " << copy_time.getElapsedTime().asMicroseconds() / 1000. << std::endl;
            design.server_blueprint_manage = model.persistent_data.blueprint_manage;

            //std::cout << "RSBM " << design.server_blueprint_manage._pid << " serv " << model.networked_model.blueprint_manage._pid << std::endl;

            renderables.entities = model.renderables;

            update_interpolated_variables(model);

            for(ship& s : model.ships)
            {
                if(s._pid == !model.controlled_ship_id)
                    continue;

                for(component& c : s.components)
                {
                    if(c.base_id != component_type::CPU)
                        continue;

                    audio.relative_amplitudes = c.cpu_core.audio.relative_amplitudes;
                    audio.frequencies = c.cpu_core.audio.frequencies;
                    audio.types = c.cpu_core.audio.types;

                    for(auto& i : audio.relative_amplitudes)
                    {
                        i *= editors[c._pid].audio_volume;
                    }

                    audio.play_all();
                }

                break;
            }
        }

        for(client_renderable& r : model.renderables)
        {
            if(r.network_owner == model.client_network_id)
            {
                ship_proxy->r.position = r.position;

                poi_cam.position = r.position;
                sspace_cam.position = r.position;
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

        int render_mode = RENDER_LAYER_SSPACE;

        size_t current_room_pid = -1;
        ship* my_ship = nullptr;
        vec2f ship_current_position;

        for(ship& s : model.ships)
        {
            if(s._pid == model.controlled_ship_id)
            {
                my_ship = &s;

                current_room_pid = s.current_room_pid;

                if(s.room_type == space_type::REAL_SPACE)
                {
                    ship_proxy->r.render_layer = RENDER_LAYER_REALSPACE;

                    if(poi_cam.get_linear_zoom() <= -3)
                    {
                        render_mode = RENDER_LAYER_SSPACE;

                        ship_current_position = (ship_proxy->r.position * ROOM_POI_SCALE) + model.room_position;

                        sspace_cam.position = (poi_cam.position * ROOM_POI_SCALE) + model.room_position;
                        sspace_cam.zoom = poi_cam.zoom;
                        sspace_cam.add_linear_zoom(4);
                    }
                    else
                    {
                        render_mode = RENDER_LAYER_REALSPACE;
                        ship_current_position = (ship_proxy->r.position * ROOM_POI_SCALE) + model.room_position;
                        sspace_cam.zoom = 1;
                    }
                }
                else
                {
                    ship_proxy->r.render_layer = RENDER_LAYER_SSPACE;

                    render_mode = RENDER_LAYER_SSPACE;
                    ship_current_position = ship_proxy->r.position;
                    poi_cam.zoom = 1;
                }
            }
            else
            {
                /*for(component& c : s.components)
                {
                    if(c.has_tag(tag_info::TAG_CPU))
                    {
                        editors[c._pid].render(c.cpu_core);
                    }
                }*/
            }
        }

        if(my_ship)
        {
            for(component& c : my_ship->components)
            {
                if(c.has_tag(tag_info::TAG_CPU))
                {
                    editors[c._pid].render(c.cpu_core);
                }
            }
        }

        camera cam(sspace_cam.screen_size);

        if(render_mode == RENDER_LAYER_SSPACE)
        {
            cam = sspace_cam;
        }
        else
        {
            cam = poi_cam;
        }

        //for(clientside_label& lab : model.labels)

        renderables.render_layer(cam, window, render_mode);

        network_protocol nproto;
        nproto.type = network_mode::DATA;

        client_input cinput;

        float forward_vel = 0;

        forward_vel += key.isKeyPressed(sf::Keyboard::W) && !skip_keyboard_input(focus);
        forward_vel -= key.isKeyPressed(sf::Keyboard::S) && !skip_keyboard_input(focus);

        float angular_vel = 0;

        angular_vel += key.isKeyPressed(sf::Keyboard::D) && !skip_keyboard_input(focus);
        angular_vel -= key.isKeyPressed(sf::Keyboard::A) && !skip_keyboard_input(focus);

        cinput.direction = (vec2f){forward_vel, 0};
        cinput.rotation = angular_vel;

        if(ONCE_MACRO(sf::Keyboard::Space, focus))
        {
            client_fire fire;
            fire.weapon_offset = 0;

            cinput.fired.push_back(fire);

            //cinput.fired = true;
        }

        if(ONCE_MACRO(sf::Keyboard::Q, focus))
        {
            cinput.ping = true;
        }

        if(ONCE_MACRO(sf::Keyboard::J, focus))
        {
            cinput.to_poi_space = true;
        }

        if(ONCE_MACRO(sf::Keyboard::H, focus))
        {
            cinput.to_fsd_space = true;
        }

        vec2f mouse_relative_pos = cam.screen_to_world(mpos) - ship_proxy->r.position;

        if(ONCE_MACRO(sf::Keyboard::E, focus))
        {
            client_fire fire;
            fire.weapon_offset = 1;

            fire.fire_angle = mouse_relative_pos.angle();

            cinput.fired.push_back(fire);
        }

        for(int num = sf::Keyboard::Num1; num <= sf::Keyboard::Num9; num++)
        {
            //if(ONCE_MACRO((sf::Keyboard::Key)num))

            if(key.isKeyPressed((sf::Keyboard::Key)num) && !skip_keyboard_input(focus))
            {
                client_fire fire;
                fire.weapon_offset = (int)num - (int)sf::Keyboard::Num1;

                fire.fire_angle = mouse_relative_pos.angle();

                cinput.fired.push_back(fire);
            }
        }

        if(ONCE_MACRO(sf::Keyboard::F1, focus))
        {
            design.open = !design.open;
        }

        cinput.mouse_world_pos = cam.screen_to_world(mpos);
        cinput.rpcs = get_global_serialise_info();

        {
            ImGui::Begin("Star Systems", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

            std::vector<std::string> names{"Name"};
            std::vector<std::string> positions{"Position"};
            std::vector<std::string> misc{"Type"};
            std::vector<std::string> pids{"ID"};

            for(system_descriptor& desc : model.connected_systems)
            {
                std::string spos = to_string_with(desc.position.x(), 1) + " " + to_string_with(desc.position.y());

                names.push_back(desc.name);
                positions.push_back(spos);
                misc.push_back("Star");
                pids.push_back(std::to_string(desc.sys_pid));
            }

            for(int i=0; i < (int)names.size(); i++)
            {
                std::string formatted = format(names[i], names) + " | " + format(positions[i], positions) + " | " + format(misc[i], misc) + " | " + format(pids[i], pids);

                ImGui::Text(formatted.c_str());

                if(i == 0)
                    continue;

                bool skip = false;

                for(ship& s : model.ships)
                {
                    if(s._pid == model.controlled_ship_id)
                    {
                        if(s.room_type == space_type::S_SPACE)
                            skip = true;

                        break;
                    }
                }

                if(skip)
                    continue;

                if(my_ship == nullptr)
                    continue;

                int model_idx = i - 1;

                ImGui::SameLine();

                ImGui::Text("|");

                ImGui::SameLine();

                std::string fstr = "(Warp)";

                ImVec4 col = colours::pastel_red;

                if(my_ship->has_w_power)
                    col = colours::pastel_green;

                if(ImGuiX::SimpleButtonColored(col, fstr))
                {
                    warp_info inf;
                    inf.sys_pid = model.connected_systems[model_idx].sys_pid;
                    inf.should_warp = true;

                    cinput.warp = inf;
                }
            }

            ImGui::End();
        }

        if(my_ship && my_ship->room_type == space_type::REAL_SPACE)
        {
            ImGui::Begin("Local Objects");

            std::vector<std::string> positions{"Position"};
            std::vector<std::string> distances{"Distance"};
            std::vector<std::string> pids{"ID"};
            std::vector<int> old_index;

            for(int i=0; i < (int)model.sample.renderables.size(); i++)
            {
                client_renderable& ren = model.sample.renderables[i].property.r;
                uint32_t object_pid = model.sample.renderables[i].uid;

                if(ren.render_layer != RENDER_LAYER_REALSPACE)
                    continue;

                std::string spos = to_string_with(ren.position.x(), 1) + " " + to_string_with(ren.position.y());

                std::string dist = to_string_with((ren.position - ship_proxy->r.position).length(), 1);

                positions.push_back(spos);
                distances.push_back(dist);
                pids.push_back(std::to_string(object_pid));
                old_index.push_back(i);
            }

            for(int i=0; i < (int)positions.size(); i++)
            {
                std::string rstr = format(positions[i], positions) + " | " + format(distances[i], distances) + " | " + format(pids[i], pids);

                ImGui::Text(rstr.c_str());

                if(i == 0)
                    continue;
            }

            ImGui::End();
        }

        std::map<size_t, bool> highlighted_pois;

        {
            ///remember off by 1
            std::vector<std::string> names{"Name"};
            std::vector<std::string> positions{"Position"};
            std::vector<std::string> distances{"Distance"};
            std::vector<std::string> types{"Type"};
            std::vector<std::string> offsets{"Offset"};
            std::vector<std::string> pids{"ID"};

            ImGui::Begin("Points of Interest", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

            for(int i=0; i < (int)model.labels.size(); i++)
            {
                client_poi_data& lab = model.labels[i];

                std::string spos = to_string_with(lab.position.x(), 1) + " " + to_string_with(lab.position.y());

                std::string dist = to_string_with((lab.position - ship_current_position).length(), 1);

                names.push_back(lab.name);
                positions.push_back(spos);
                distances.push_back(dist);
                types.push_back(lab.type);
                offsets.push_back(std::to_string(lab.offset));
                pids.push_back(std::to_string(model.labels[i].poi_pid));
            }

            for(int i=0; i < (int)names.size(); i++)
            {
                int real_idx = i - 1;

                std::string rstr = format(names[i], names) + " | " + format(types[i], types) + " | " + format(offsets[i], offsets) + " | " + format(positions[i], positions) + " | " + format(distances[i], distances) + " | " + format(pids[i], pids);

                ImGui::Text(rstr.c_str());

                if(ImGui::IsItemHovered() && i != 0)
                {
                    highlighted_pois[model.labels[real_idx].poi_pid] = true;
                }

                if(i == 0)
                    continue;

                if(my_ship == nullptr)
                    continue;

                if(my_ship->room_type == space_type::S_SPACE)
                    continue;

                size_t pid = model.labels[real_idx].poi_pid;

                ImGui::SameLine();

                ImGui::Text("|");

                ImGui::SameLine();

                if(pid == current_room_pid)
                {
                    ImGuiX::SimpleButtonColored(colours::pastel_red, "[Here]");
                }
                else
                {
                    if(my_ship->has_s_power)
                    {
                        if(ImGuiX::SimpleButtonColored(colours::pastel_green, "(Warp)"))
                        {
                            poi_travel_info info;
                            info.poi_pid = pid;
                            info.should_travel = true;

                            cinput.travel = info;
                        }
                    }
                    else
                    {
                        ImGuiX::SimpleButtonColored(colours::pastel_red, "(Warp)");
                    }
                }

                if(ImGui::IsItemHovered())
                {
                    highlighted_pois[pid] = true;
                }
            }

            ImGui::End();
        }

        if(render_mode == RENDER_LAYER_SSPACE)
        {
            /*for(int i=0; i < (int)model.labels.size(); i++)
            {
                client_poi_data& lab = model.labels[i];

                vec2f sspace = cam.world_to_screen(lab.position);

                //if((mpos - sspace).length() > 20)
                //    continue;

                ImGui::SetNextWindowPos(ImVec2(sspace.x(), sspace.y()));

                ImGuiWindowFlags flags = ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_NoBringToFrontOnFocus;

                ImGui::Begin(("##" + std::to_string(i)).c_str(), nullptr, flags);

                std::string str = lab.name.c_str();
                ImVec4 col = ImVec4(1,1,1,1);

                if(highlighted_pois[lab.poi_pid])
                {
                    col = colours::pastel_green;
                }

                ImGui::TextColored(col, str.c_str());

                ImGui::End();
            }*/

            for(client_poi_data& lab : model.labels)
            {
                if(!highlighted_pois[lab.poi_pid])
                    continue;

                if(!my_ship)
                    continue;

                vec2f to_poi = lab.position - ship_current_position;

                float to_len = to_poi.length()/4 - 13;

                if(to_len > 0)
                {
                    client_renderable ren;
                    ren.init_rectangular({to_len, 1});
                    ren.rotation = to_poi.angle();

                    ren.position = (to_poi / 2.f) + ship_current_position;
                    ren.render(cam, window);
                }
            }
        }

        get_global_serialise_info().all_rpcs.clear();

        nproto.data = serialise(cinput);

        conn.writes_to(nproto, -1);

        //sf::Clock showtime;

        for(ship& s : model.ships)
        {
            /*ImGui::Begin(std::to_string(s.network_owner).c_str());

            ImGui::Text(s.show_resources().c_str());

            ImGui::End();*/


            //printf("fpid %i\n", s._pid);

            s.show_resources();

            s.show_power();

            s.show_manufacturing_windows(design.server_blueprint_manage);

            /*if(s._pid == model.controlled_ship_id && s.has_s_power && s.room_type == space_type::REAL_SPACE)
            {
                ImGui::SetNextWindowPos(ImVec2(window.getSize().x/2 - last_s_size.x()/2, 6*window.getSize().y/7));

                ImGuiWindowFlags flags = ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysAutoResize;

                ImGui::Begin("##active", nullptr, flags);

                ImGui::Text("S-Drive Ready");

                last_s_size = {ImGui::GetWindowWidth(), ImGui::GetWindowHeight()};

                ImGui::End();
            }*/
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

        std::vector<pending_transfer>& xfers = client_pending_transfers();

        for(int i=0; i < (int)xfers.size(); i++)
        {
            bool open = true;

            ImGui::Begin("Xfer", &open, ImGuiWindowFlags_AlwaysAutoResize);

            ImGui::SliderFloat("Xfer %", &xfers[i].fraction, 0, 1);

            if(ImGui::Button("Accept"))
            {
                open = false;
                ///rpc

                owned* ocomp = find_by_id(model, xfers[i].pid_component);

                if(ocomp)
                {
                    component* c = dynamic_cast<component*>(ocomp);

                    if(c)
                    {
                        c->transfer_stored_from_to_frac_rpc(xfers[i].pid_ship, xfers[i].pid_component, xfers[i].fraction);
                    }
                }
            }

            ImGui::SameLine();

            if(ImGui::Button("Cancel"))
            {
                open = false;
            }

            ImGui::End();

            if(!open)
            {
                xfers.erase(xfers.begin() + i);
                i--;
                continue;
            }
        }

        render_radar_data(window, sample);

        entities.render_layer(cam, window, render_mode);
        transients.render_layer(cam, window, render_mode);
        design.render(window);

        sf::Clock render_clock;

        ImGui::SFML::Render(window);

        //std::cout << "rclock " << render_clock.getElapsedTime().asMicroseconds() / 1000. << std::endl;

        window.display();
        window.clear();

        sf::sleep(sf::milliseconds(4));
    }

    return 0;
}
