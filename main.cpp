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

void server_thread()
{
    fixed_clock::init = true;

    connection conn;
    conn.host("192.168.0.54", 11000);

    data_model<ship*> model;

    #define SERVER_VIEW
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

    component thruster, warp, shields, missile, laser, sensor, comms, armour, ls, coolant, power_generator, crew, coolant_cold, coolant_hot;

    thruster.add(component_info::POWER, -1);
    thruster.add(component_info::THRUST, 1);
    thruster.add(component_info::HP, 0, 1);
    thruster.add_composition(material_info::COPPER, 10);
    thruster.set_heat(10);

    warp.add(component_info::POWER, -1);
    warp.add(component_info::WARP, 0.5, 10);
    warp.add(component_info::WARP, -0.1);
    warp.add(component_info::WARP, 0.1);
    warp.add(component_info::HP, 0, 5);
    warp.add_composition(material_info::COPPER, 10);
    warp.add_composition(material_info::IRON, 10);
    warp.set_no_drain_on_full_production();
    warp.set_heat(100);

    shields.add(component_info::SHIELDS, 0.5, 50);
    shields.add(component_info::POWER, -3);
    shields.add(component_info::HP, 0, 5);
    shields.add_composition(material_info::IRON, 10);
    shields.add_composition(material_info::COPPER, 5);
    shields.set_no_drain_on_full_production();
    shields.set_heat(20);

    missile.add(component_info::POWER, -1);
    missile.add(component_info::WEAPONS, 1);
    missile.add(component_info::HP, 0, 2);
    missile.add(component_info::MISSILE_STORE, 0.01, 10);
    missile.add_composition(material_info::IRON, 1);
    missile.set_no_drain_on_full_production();
    missile.set_heat(5);

    missile.add_on_use(component_info::MISSILE_STORE, -1, 1);
    missile.subtype = "missile";

    laser.add(component_info::POWER, -1);
    laser.add(component_info::WEAPONS, 1);
    laser.add(component_info::HP, 0, 2);
    laser.add(component_info::CAPACITOR, 1.5, 20);
    laser.add_composition(material_info::COPPER, 5);
    laser.set_no_drain_on_full_production();
    laser.set_heat(5);

    laser.add_on_use(component_info::CAPACITOR, -10, 1);
    laser.subtype = "laser";

    laser.max_use_angle = M_PI/8;

    //laser.info[3].held = 0;

    sensor.add(component_info::POWER, -1);
    sensor.add(component_info::SENSORS, 1);
    sensor.add(component_info::HP, 0, 1);
    sensor.add_composition(material_info::COPPER, 2);

    sensor.add_on_use(component_info::POWER, -35, 1);

    sensor.set_heat(2);

    comms.add(component_info::POWER, -0.5);
    comms.add(component_info::COMMS, 1);
    comms.add(component_info::HP, 0, 1);
    comms.add_composition(material_info::COPPER, 1);
    comms.add_composition(material_info::IRON, 1);
    comms.set_heat(1);

    /*sysrepair.add(component_info::POWER, -1);
    sysrepair.add(component_info::SYSTEM, 1);
    sysrepair.add(component_info::HP, 0.1, 1);*/

    armour.add(component_info::ARMOUR, 0.01, 30);
    armour.add(component_info::POWER, -0.5);
    armour.add(component_info::HP, 0, 10);
    armour.add_composition(material_info::IRON, 30);
    armour.set_no_drain_on_full_production();
    armour.set_heat(5);

    ls.add(component_info::POWER, -1);
    ls.add(component_info::LIFE_SUPPORT, 1, 20);
    ls.add(component_info::HP, 0.01, 5);
    ls.add_composition(material_info::IRON, 2);
    ls.add_composition(material_info::COPPER, 2);
    //ls.set_no_drain_on_full_production();
    ls.set_heat(10);

    coolant.add(component_info::COOLANT, 10, 200);
    coolant.add(component_info::HP, 0, 1);
    coolant.add(component_info::POWER, -1);
    coolant.add_composition(material_info::IRON, 5);
    coolant.set_heat(5);

    power_generator.add(component_info::POWER, 8, 50);
    power_generator.add(component_info::HP, 0, 10);
    power_generator.add_composition(material_info::IRON, 30);
    power_generator.add_composition(material_info::COPPER, 20);
    power_generator.set_heat(8 * 20);
    power_generator.set_heat_scales_by_production(true, component_info::POWER);

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

    coolant_cold.add(component_info::HP, 0, 10);
    coolant_hot.add(component_info::HP, 0, 10);

    coolant_cold.internal_volume = 50;
    coolant_hot.internal_volume = 5;

    coolant_cold.add_composition(material_info::IRON, 55);
    coolant_hot.add_composition(material_info::IRON, 10);

    coolant_hot.heat_sink = true;

    component coolant_material;
    ///????
    coolant_material.add(component_info::HP, 0, 1);
    coolant_material.add_composition(material_info::HYDROGEN, 50);
    coolant_material.long_name = "Fluid";
    coolant_material.flows = true;

    component coolant_material2;
    ///????
    coolant_material2.add(component_info::HP, 0, 1);
    coolant_material2.add_composition(material_info::HYDROGEN, 5);
    coolant_material2.long_name = "Fluid";
    coolant_material2.flows = true;

    assert(coolant_cold.can_store(coolant_material));
    coolant_cold.store(coolant_material);
    coolant_cold.add_heat_to_stored(500);

    coolant_hot.store(coolant_material2);
    coolant_hot.add_heat_to_stored(500);

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
    test_ship->add(coolant);
    test_ship->add(power_generator);
    test_ship->add(crew);
    test_ship->add(coolant_cold);
    test_ship->add(coolant_hot);

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

    test_ship->r.position = {400, 400};

    test_ship->r.position = {498.336609, 529.024292};

    test_ship->r.position = {323.986694, 242.469727};

    ship* test_ship2 = entities.make_new<ship>(*test_ship);
    //test_ship2->data_track.pid = get_next_persistent_id();
    test_ship2->new_network_copy();

    //std::cout << "TS2 " << test_ship2->data_track.pid << std::endl;
    test_ship2->network_owner = 1;
    test_ship2->r.network_owner = 1;

    test_ship2->r.position = {600, 400};

    std::minstd_rand rng;
    rng.seed(0);

    player_model_manager player_manage;

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

    while(1)
    {
        sf::Clock tickclock;
        entities.tick(frametime_dt);

        double tclock_time = tickclock.getElapsedTime().asMicroseconds() / 1000.;
        std::cout << "tclock " << tclock_time << std::endl;

        entities.cleanup();

        frametime_dt = (clk.restart().asMicroseconds() / 1000.) / 1000.;

        /*nlohmann::json dat;
        entities.serialise(dat, true);*/

        /*auto clients = conn.clients();

        for(auto& i : clients)
        {
            conn.writes_to(entities, i);
        }*/

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

        /*for(entity* e : entities.entities)
        {
            asteroid* a = dynamic_cast<asteroid*>(e);

            if(a)
            {
                alt_frequency_packet heat;
                heat.frequency = HEAT_FREQ;
                heat.intensity = a->permanent_heat;

                radar.emit(heat, e->r.position, e->id);
            }
        }*/

        radar.tick(frametime_dt);
        player_manage.tick(frametime_dt);

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

            player_model* fmodel = player_manage.make_new(pid);

            std::vector<ship*> ships = entities.fetch<ship>();

            for(ship* s : ships)
            {
                if(s->network_owner == pid)
                {
                    s->model = fmodel;
                }
            }

            conn.pop_new_client();
        }

        while(conn.has_read())
        {
            writes_data<client_input> read = conn.reads_from<client_input>();

            last_mouse_pos[read.id] = read.data.mouse_world_pos;

            for(entity* e : entities.entities)
            {
                ship* s = dynamic_cast<ship*>(e);

                if(s && s->network_owner == read.id)
                {
                    ///acceleration etc
                    {
                        double time = (control_elapsed[read.id].restart().asMicroseconds() / 1000.) / 1000.;

                        s->apply_force(read.data.direction * time);
                        s->apply_rotation_force(read.data.rotation * time);

                        double thruster_active_percent = read.data.direction.length() + fabs(read.data.rotation);

                        thruster_active_percent = clamp(thruster_active_percent, 0, 1);

                        s->set_thrusters_active(thruster_active_percent);

                        if(read.data.fired.size() > 0)
                        {
                            s->fire(read.data.fired);
                        }

                        if(read.data.ping)
                        {
                            s->ping();
                        }
                    }

                    {
                        serialise_context ctx;
                        ctx.inf = read.data.rpcs;
                        ctx.exec_rpcs = true;

                        //s->serialise(ctx, ctx.faux);
                        do_recurse(ctx, s);
                    }
                }
            }

            conn.pop_read();
        }

        if(key.isKeyPressed(sf::Keyboard::P))
            std::cout << "test ship " << test_ship->r.position << std::endl;

        #define SEE_ONLY_REAL

        std::map<uint32_t, ship*> network_ships;

        for(entity* e : entities.entities)
        {
            ship* s = dynamic_cast<ship*>(e);

            if(s)
            {
                network_ships[s->network_owner] = s;
            }
        }

        auto clients = conn.clients();

        for(auto& i : clients)
        {
            //model.sample = radar.sample()

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

                    vec2f mpos = last_mouse_pos[i];

                    vec2f to_mouse = mpos - s->r.position;

                    vec2f front_dir = (vec2f){1, 0}.rot(s->r.rotation);

                    #ifdef MOUSE_TRACK
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

            model.client_network_id = i;
            model.ships = ships;
            model.renderables = renderables;

            if(network_ships[i] != nullptr)
            {
                model.sample = radar.sample_for(network_ships[i]->r.position, network_ships[i]->id, entities, player_manage.fetch_by_network_id(i));
            }
            else
            {
                model.sample = decltype(model.sample)();
            }

            //std::cout << "srv write " << read_clock.restart().asMicroseconds() / 1000. << std::endl;

            conn.writes_to(model, i);
        }

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

        //Sleep(100);
    }
}

int main()
{
    #define WITH_SERVER
    #ifdef WITH_SERVER
    std::thread(server_thread).detach();
    #endif // WITH_SERVER


    sf::ContextSettings sett;
    sett.antialiasingLevel = 8;

    sf::RenderWindow window(sf::VideoMode(1200, 800), "hi", sf::Style::Default, sett);

    camera cam({window.getSize().x, window.getSize().y});

    cam.position = {400, 400};

    sf::Texture font_atlas;

    ImGui::SFML::Init(window, false);

    ImGuiIO& io = ImGui::GetIO();
    //io.Fonts->AddFontDefault();
    ImFont* font = io.Fonts->AddFontFromFileTTF("VeraMono.ttf", 14.f);

    ImGuiFreeType::BuildFontAtlas(font_atlas, io.Fonts, 0, 1);

    ImGuiStyle& style = ImGui::GetStyle();

    style.FrameRounding = 0;
    style.WindowRounding = 0;
    style.ChildRounding = 0;
    style.ChildBorderSize = 0;
    style.FrameBorderSize = 0;
    //style.PopupBorderSize = 0;
    //style.WindowBorderSize = 0;

    assert(font != nullptr);

    connection conn;
    conn.connect("192.168.0.54", 11000);
    //conn.connect("77.97.17.179", 11000);

    data_model<ship> model;
    client_entities renderables;

    sf::Clock imgui_delta;
    sf::Clock frametime_delta;

    sf::Keyboard key;
    sf::Mouse mouse;

    std::map<uint64_t, ship> network_ships;

    entity_manager entities;
    entities.use_aggregates = false;

    entity* ship_proxy = entities.make_new<entity>();

    stardust_manager star_manage(cam, entities);

    alt_radar_sample sample;
    entity_manager transients;
    transients.use_aggregates = false;

    sf::Clock read_clock;

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
                window.close();
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

        cam.screen_size = {window.getSize().x, window.getSize().y};
        cam.add_linear_zoom(mouse_delta);

        while(conn.has_read())
        {
            //model.cleanup();
            //std::cout << conn.read() << std::endl;
            conn.reads_from<data_model<ship>>(model);
            //renderables = conn.reads_from<client_entities>().data;
            conn.pop_read();

            if(model.sample.fresh)
            {
                sample = model.sample;
            }
            else
            {
                sample.intensities = model.sample.intensities;
                sample.frequencies = model.sample.frequencies;
            }

            //std::cout << (*(model.ships[0].data_track))[component_info::SHIELDS].vsat.size() << std::endl;

            //std::cout << "pid " << model.ships[0].data_track.pid << std::endl;

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

        vec2f mpos = {mouse.getPosition(window).x, mouse.getPosition(window).y};
        vec2f mfrac = mpos / (vec2f){window.getSize().x, window.getSize().y};

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

        cinput.mouse_world_pos = cam.screen_to_world(mpos);
        cinput.rpcs = get_global_serialise_info();

        get_global_serialise_info().all_rpcs.clear();

        conn.writes_to(cinput, -1);

        for(ship& s : model.ships)
        {
            /*ImGui::Begin(std::to_string(s.network_owner).c_str());

            ImGui::Text(s.show_resources().c_str());

            ImGui::End();*/


            //printf("fpid %i\n", s._pid);

            s.show_resources();

            s.advanced_ship_display();
        }

        render_radar_data(window, sample);

        entities.render(cam, window);
        transients.render(cam, window);

        ImGui::SFML::Render(window);
        window.display();
        window.clear();

        sf::sleep(sf::milliseconds(4));
    }

    return 0;
}
