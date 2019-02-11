#include <iostream>
#include <SFML/Graphics.hpp>
#include <imgui/imgui.h>
#include <imgui-sfml/imgui-SFML.h>
#include <imgui/misc/freetype/imgui_freetype.h>

int main()
{
    sf::RenderWindow window(sf::VideoMode(800, 600), "hi");

    sf::Texture font_atlas;

    ImGui::SFML::Init(window, false);

    ImGuiIO& io = ImGui::GetIO();
    //io.Fonts->AddFontDefault();
    ImFont* font = io.Fonts->AddFontFromFileTTF("VeraMono.ttf", 14.f);

    ImGuiFreeType::BuildFontAtlas(font_atlas, io.Fonts, 0, 1);

    assert(font != nullptr);

    //assert(font->ConfigData.size() != 0);

    sf::Clock imgui_delta;

    while(1)
    {
        sf::Event event;

        while(window.pollEvent(event))
        {
            ImGui::SFML::ProcessEvent(event);
        }

        ImGui::SFML::Update(window,  imgui_delta.restart());

        ImGui::Begin("Hi");

        ImGui::Text("Test");

        ImGui::End();

        ImGui::SFML::Render(window);
        window.display();
        window.clear();
    }

    return 0;
}
