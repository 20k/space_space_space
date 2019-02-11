#include <iostream>
#include <SFML/Graphics.hpp>
#include <imgui/imgui.h>
#include <imgui-sfml/imgui-SFML.h>
#include <imgui/misc/freetype/imgui_freetype.h>
#include "ship_components.hpp"

int main()
{
    sf::RenderWindow window(sf::VideoMode(1200, 800), "hi");

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

    ship test_ship;

    component thruster, warp, shields, laser, sensor, comms, armour, ls, coolant, power_generator;

    thruster.add(component_info::POWER, -1);
    thruster.add(component_info::THRUST, 1);
    thruster.add(component_info::HP, 0, 1);

    warp.add(component_info::POWER, -1);
    warp.add(component_info::WARP, 0.5, 10);
    warp.add(component_info::HP, 0, 5);

    shields.add(component_info::SHIELDS, 0.5, 10);
    shields.add(component_info::POWER, -1);
    shields.add(component_info::HP, 0, 5);

    laser.add(component_info::POWER, -1);
    laser.add(component_info::WEAPONS, 1);
    laser.add(component_info::HP, 0, 2);

    sensor.add(component_info::POWER, -1);
    sensor.add(component_info::SENSORS, 1);
    sensor.add(component_info::HP, 0, 1);

    comms.add(component_info::POWER, -0.5);
    comms.add(component_info::COMMS, 1);
    comms.add(component_info::HP, 0, 1);

    /*sysrepair.add(component_info::POWER, -1);
    sysrepair.add(component_info::SYSTEM, 1);
    sysrepair.add(component_info::HP, 0.1, 1);*/

    armour.add(component_info::ARMOUR, 1, 10);
    armour.add(component_info::POWER, -0.5);
    armour.add(component_info::HP, 0, 10);

    ls.add(component_info::POWER, -1);
    ls.add(component_info::LIFE_SUPPORT, 1, 5);
    ls.add(component_info::HP, 0.1, 5);

    coolant.add(component_info::COOLANT, 1, 20);
    coolant.add(component_info::HP, 0, 1);

    power_generator.add(component_info::POWER, 20, 50);
    power_generator.add(component_info::HP, 0, 10);

    power_generator.info[1].held = 0;

    test_ship.add(thruster);
    test_ship.add(warp);
    test_ship.add(shields);
    test_ship.add(laser);
    test_ship.add(sensor);
    test_ship.add(comms);
    //test_ship.add(sysrepair);
    test_ship.add(armour);
    test_ship.add(ls);
    test_ship.add(coolant);
    test_ship.add(power_generator);

    sf::Clock imgui_delta;
    sf::Clock frametime_delta;

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

        test_ship.tick(frametime_dt);

        ImGui::Begin("Hi");

        ImGui::Text("Test");

        ImGui::End();

        ImGui::Begin("Test Ship");

        //std::string str = test_ship.show_components();

        //ImGui::Text(str.c_str());

        ImGui::Text(test_ship.show_resources().c_str());

        ImGui::End();

        ImGui::SFML::Render(window);
        window.display();
        window.clear();
    }

    return 0;
}
