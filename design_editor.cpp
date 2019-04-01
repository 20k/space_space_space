#include "design_editor.hpp"
#include <SFML/Graphics.hpp>
#include <ImGui/ImGui.h>

void render_component_simple(const component& c)
{
    ImGui::Button(c.long_name.c_str(), ImVec2(190, 0));
}

void player_research::render(design_editor& edit, vec2f upper_size)
{
    float fixed_width = 200;

    ImGui::BeginChild("Components", ImVec2(fixed_width, upper_size.y() - 50), true);

    //ImGui::BeginChild("Hello there");

    //ImGui::Text("Hello");

    for(component& c : components)
    {
        std::string txt = c.long_name;

        render_component_simple(c);

        if((ImGui::IsItemHovered() || ImGui::IsItemClicked()) && ImGui::IsMouseDown(0) && ImGui::IsMouseDragging(0) && !edit.dragging)
        {
            /*ImGui::BeginTooltip();

            render_component_simple(c);

            ImGui::EndTooltip();*/

            edit.dragging = true;
            edit.dragging_id = c.id;
        }

        //ImGui::Text(txt.c_str());
    }

    ImGui::EndChild();
}

void blueprint_node::render()
{
    auto old_pos = ImGui::GetCursorPos();

    ImGui::SetCursorPos(ImVec2(pos.x(), pos.y()));

    //ImGui::Text(my_comp.long_name.c_str());

    render_component_simple(my_comp);

    ImGui::SetCursorPos(old_pos);
}

void blueprint::render()
{

}

void design_editor::tick(double dt_s)
{

}

void design_editor::render(sf::RenderWindow& win)
{
    if(!open)
        return;

    ImGui::SetNextWindowPos(ImVec2(300, 50), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(1000, 800), ImGuiCond_Appearing);

    ImGui::SetNextWindowFocus();

    ImGui::Begin("Blueprint Designer", &open);

    auto main_dim = ImGui::GetWindowSize();

    research.render(*this, {main_dim.x, main_dim.y});

    cur.render();

    if(!ImGui::IsMouseDown(0))
    {
        dragging = false;
    }

    if(dragging)
    {
        for(component& c : research.components)
        {
            if(c.id != dragging_id)
                continue;

            ImGui::BeginTooltip();

            render_component_simple(c);

            ImGui::EndTooltip();
        }
    }

    ImGui::End();
}
