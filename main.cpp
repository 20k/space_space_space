#include <iostream>
#include <SFML/Graphics.hpp>
#include <imgui/imgui.h>
#include <imgui-sfml/imgui-SFML.h>
#include <imgui/misc/freetype/imgui_freetype.h>
#include "ship_components.hpp"
#include "entity.hpp"
#include <windows.h>
#include <networking/networking.hpp>

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

void server_thread()
{
    connection conn;
    conn.host("192.168.0.54", 11000);

    data_model model;

    entity_manager entities;

    ship* test_ship = entities.make_new<ship>();

    component thruster, warp, shields, laser, sensor, comms, armour, ls, coolant, power_generator, crew;

    thruster.add(component_info::POWER, -1);
    thruster.add(component_info::THRUST, 1);
    thruster.add(component_info::HP, 0, 1);

    warp.add(component_info::POWER, -1);
    warp.add(component_info::WARP, 0.5, 10);
    warp.add(component_info::WARP, -0.1);
    warp.add(component_info::WARP, 0.1);
    warp.add(component_info::HP, 0, 5);

    shields.add(component_info::SHIELDS, 0.5, 10);
    shields.add(component_info::POWER, -1);
    shields.add(component_info::HP, 0, 5);

    laser.add(component_info::POWER, -1);
    laser.add(component_info::WEAPONS, 1);
    laser.add(component_info::HP, 0, 2);
    laser.add(component_info::CAPACITOR, 1, 20);

    laser.add_on_use(component_info::CAPACITOR, -10);

    //laser.info[3].held = 0;

    sensor.add(component_info::POWER, -1);
    sensor.add(component_info::SENSORS, 1);
    sensor.add(component_info::HP, 0, 1);

    comms.add(component_info::POWER, -0.5);
    comms.add(component_info::COMMS, 1);
    comms.add(component_info::HP, 0, 1);

    /*sysrepair.add(component_info::POWER, -1);
    sysrepair.add(component_info::SYSTEM, 1);
    sysrepair.add(component_info::HP, 0.1, 1);*/

    armour.add(component_info::ARMOUR, 0.1, 10);
    armour.add(component_info::POWER, -0.5);
    armour.add(component_info::HP, 0, 10);

    ls.add(component_info::POWER, -1);
    ls.add(component_info::LIFE_SUPPORT, 1, 5);
    ls.add(component_info::HP, 0.01, 5);

    coolant.add(component_info::COOLANT, 1, 20);
    coolant.add(component_info::HP, 0, 1);

    power_generator.add(component_info::POWER, 10, 50);
    power_generator.add(component_info::HP, 0, 10);

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


    test_ship->add(thruster);
    test_ship->add(warp);
    test_ship->add(shields);
    test_ship->add(laser);
    test_ship->add(sensor);
    test_ship->add(comms);
    //test_ship.add(sysrepair);
    test_ship->add(armour);
    test_ship->add(ls);
    test_ship->add(coolant);
    test_ship->add(power_generator);
    test_ship->add(crew);

    test_ship->r.position = {400, 400};

    ship* test_ship2 = entities.make_new<ship>(*test_ship);
    test_ship2->network_owner = 1;

    test_ship2->r.position = {600, 400};

    double frametime_dt = 1;

    sf::Clock clk;

    std::map<uint64_t, sf::Clock> control_elapsed;

    while(1)
    {
        entities.tick(frametime_dt);
        entities.cleanup();

        frametime_dt = (clk.restart().asMicroseconds() / 1000.) / 1000.;

        /*nlohmann::json dat;
        entities.serialise(dat, true);*/

        /*auto clients = conn.clients();

        for(auto& i : clients)
        {
            conn.writes_to(entities, i);
        }*/

        while(conn.has_read())
        {
            writes_data<client_input> read = conn.reads_from<client_input>();

            for(entity* e : entities.entities)
            {
                ship* s = dynamic_cast<ship*>(e);

                if(s && s->network_owner == read.id)
                {
                    double time = (control_elapsed[read.id].restart().asMicroseconds() / 1000.) / 1000.;

                    s->apply_force(read.data.direction * time);
                    s->apply_rotation_force(read.data.rotation * time);

                    if(read.data.fired)
                    {
                        s->fire();
                    }
                }
            }

            conn.pop_read();
        }

        std::vector<ship> ships;
        std::vector<client_renderable> renderables;

        for(entity* e : entities.entities)
        {
            ship* s = dynamic_cast<ship*>(e);

            if(s)
            {
                ships.push_back(*s);
            }

            renderables.push_back(e->r);
        }


        model.ships = ships;
        model.renderables = renderables;

        auto clients = conn.clients();

        for(auto& i : clients)
        {
            conn.writes_to(model, i);
        }

        Sleep(10);
    }
}

int main()
{
    std::thread(server_thread).detach();

    sf::ContextSettings sett;
    sett.antialiasingLevel = 8;

    sf::RenderWindow window(sf::VideoMode(1200, 800), "hi", sf::Style::Default, sett);

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

    #if 0
    entity_manager entities;

    ship* test_ship = entities.make_new<ship>();

    component thruster, warp, shields, laser, sensor, comms, armour, ls, coolant, power_generator, crew;

    thruster.add(component_info::POWER, -1);
    thruster.add(component_info::THRUST, 1);
    thruster.add(component_info::HP, 0, 1);

    warp.add(component_info::POWER, -1);
    warp.add(component_info::WARP, 0.5, 10);
    warp.add(component_info::WARP, -0.1);
    warp.add(component_info::WARP, 0.1);
    warp.add(component_info::HP, 0, 5);

    shields.add(component_info::SHIELDS, 0.5, 10);
    shields.add(component_info::POWER, -1);
    shields.add(component_info::HP, 0, 5);

    laser.add(component_info::POWER, -1);
    laser.add(component_info::WEAPONS, 1);
    laser.add(component_info::HP, 0, 2);
    laser.add(component_info::CAPACITOR, 1, 20);

    laser.add_on_use(component_info::CAPACITOR, -10);

    //laser.info[3].held = 0;

    sensor.add(component_info::POWER, -1);
    sensor.add(component_info::SENSORS, 1);
    sensor.add(component_info::HP, 0, 1);

    comms.add(component_info::POWER, -0.5);
    comms.add(component_info::COMMS, 1);
    comms.add(component_info::HP, 0, 1);

    /*sysrepair.add(component_info::POWER, -1);
    sysrepair.add(component_info::SYSTEM, 1);
    sysrepair.add(component_info::HP, 0.1, 1);*/

    armour.add(component_info::ARMOUR, 0.1, 10);
    armour.add(component_info::POWER, -0.5);
    armour.add(component_info::HP, 0, 10);

    ls.add(component_info::POWER, -1);
    ls.add(component_info::LIFE_SUPPORT, 1, 5);
    ls.add(component_info::HP, 0.01, 5);

    coolant.add(component_info::COOLANT, 1, 20);
    coolant.add(component_info::HP, 0, 1);

    power_generator.add(component_info::POWER, 10, 50);
    power_generator.add(component_info::HP, 0, 10);

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


    test_ship->add(thruster);
    test_ship->add(warp);
    test_ship->add(shields);
    test_ship->add(laser);
    test_ship->add(sensor);
    test_ship->add(comms);
    //test_ship.add(sysrepair);
    test_ship->add(armour);
    test_ship->add(ls);
    test_ship->add(coolant);
    test_ship->add(power_generator);
    test_ship->add(crew);

    test_ship->position = {400, 400};

    ship* test_ship2 = entities.make_new<ship>(*test_ship);

    test_ship2->position = {600, 400};
    #endif // 0

    connection conn;
    conn.connect("192.168.0.54", 11000);

    data_model model;
    client_entities renderables;

    sf::Clock imgui_delta;
    sf::Clock frametime_delta;

    sf::Keyboard key;

    while(window.isOpen())
    {
        double frametime_dt = (frametime_delta.restart().asMicroseconds() / 1000.) / 1000.;

        sf::Event event;

        while(window.pollEvent(event))
        {
            ImGui::SFML::ProcessEvent(event);

            if(event.type == sf::Event::Closed)
            {
                window.close();
            }

            if(event.type == sf::Event::Resized)
            {
                window.create(sf::VideoMode(event.size.width, event.size.height), "hi");
            }
        }

        ImGui::SFML::Update(window,  imgui_delta.restart());

        while(conn.has_read())
        {
            //std::cout << conn.read() << std::endl;
            model = conn.reads_from<data_model>().data;
            //renderables = conn.reads_from<client_entities>().data;
            conn.pop_read();

            renderables.entities = model.renderables;
        }

        renderables.render(window);

        client_input cinput;

        float forward_vel = 0;

        forward_vel += key.isKeyPressed(sf::Keyboard::W);
        forward_vel -= key.isKeyPressed(sf::Keyboard::S);

        float angular_vel = 0;

        angular_vel += key.isKeyPressed(sf::Keyboard::D);
        angular_vel -= key.isKeyPressed(sf::Keyboard::A);

        cinput.direction = (vec2f){0, forward_vel};
        cinput.rotation = angular_vel;

        if(ONCE_MACRO(sf::Keyboard::Space))
        {
            cinput.fired = true;
        }

        //std::cout << cinput.direction << std::endl;

        conn.writes_to(cinput, -1);

        //nlohmann::json testdat = serialise(cinput);

        //std::cout << "writing " << testdat.dump() << std::endl;

        //conn.write_to({(uint64_t)-1, testdat.dump()});

        #if 0
        float forward_vel = 0;

        forward_vel += key.isKeyPressed(sf::Keyboard::W);
        forward_vel -= key.isKeyPressed(sf::Keyboard::S);

        float angular_vel = 0;

        angular_vel += key.isKeyPressed(sf::Keyboard::D);
        angular_vel -= key.isKeyPressed(sf::Keyboard::A);

        test_ship->apply_force({0, forward_vel});
        test_ship->apply_rotation_force(angular_vel);

        //test_ship.tick(frametime_dt);
        //test_ship.tick_move(frametime_dt);
        //test_ship.render(window);

        entities.tick(frametime_dt);
        entities.render(window);
        entities.cleanup();

        ImGui::Begin("Hi");

        ImGui::Text("Test");

        ImGui::End();

        ImGui::Begin("Test Ship");

        //std::string str = test_ship.show_components();

        //ImGui::Text(str.c_str());

        ImGui::Text(test_ship->show_resources().c_str());

        ImGui::End();

        ImGui::Begin("Test Ship2");

        //std::string str = test_ship.show_components();

        //ImGui::Text(str.c_str());

        ImGui::Text(test_ship2->show_resources().c_str());

        ImGui::End();

        if(ONCE_MACRO(sf::Keyboard::Space))
        {
            test_ship->fire();
        }

        test_ship->advanced_ship_display();
        #endif // 0

        for(ship& s : model.ships)
        {
            ImGui::Begin(std::to_string(s.network_owner).c_str());

            ImGui::Text(s.show_resources().c_str());

            ImGui::End();

            s.advanced_ship_display();
        }

        ImGui::SFML::Render(window);
        window.display();
        window.clear();

        Sleep(10);
    }

    return 0;
}
