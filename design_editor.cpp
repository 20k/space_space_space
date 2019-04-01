#include "design_editor.hpp"
#include <SFML/Graphics.hpp>
#include <ImGui/ImGui.h>

void player_research::render(vec2f upper_size)
{
    float fixed_width = 200;

    ImGui::BeginChild("Components", ImVec2(fixed_width, upper_size.y() - 50), true);

    //ImGui::BeginChild("Hello there");

    //ImGui::Text("Hello");

    for(component& c : components)
    {
        std::string txt = c.long_name;

        ImGui::Text(txt.c_str());
    }

    ImGui::EndChild();
}

void blueprint_node::render()
{
    auto old_pos = ImGui::GetCursorPos();

    ImGui::SetCursorPos(ImVec2(pos.x(), pos.y()));

    ImGui::Text(my_comp.long_name.c_str());

    ImGui::SetCursorPos(old_pos);
}

void design_editor::tick(double dt_s)
{

}

void design_editor::render(sf::RenderWindow& win)
{
    if(!open)
        return;

    ImGui::SetNextWindowPos(ImVec2(400, 50), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(800, 800), ImGuiCond_Appearing);

    ImGui::SetNextWindowFocus();

    ImGui::Begin("Blueprint Designer", &open);

    auto main_dim = ImGui::GetWindowSize();

    research.render({main_dim.x, main_dim.y});

    ImGui::End();
}
