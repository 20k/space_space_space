#include "design_editor.hpp"
#include <SFML/Graphics.hpp>
#include <ImGui/ImGui.h>
#include <iostream>

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

        if(((ImGui::IsItemHovered() && ImGui::IsMouseDown(0) && ImGui::IsMouseDragging(0)) || ImGui::IsItemClicked(0)) && !edit.dragging)
        {
            /*ImGui::BeginTooltip();

            render_component_simple(c);

            ImGui::EndTooltip();*/

            edit.dragging = true;
            edit.dragging_id = c.id;
        }

        if(ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();

            c.render_inline_stats();

            ImGui::EndTooltip();
        }

        //ImGui::Text(txt.c_str());
    }

    ImGui::EndChild();
}

void blueprint_node::render(design_editor& edit)
{
    auto old_pos = ImGui::GetCursorPos();

    ImGui::SetCursorPos(ImVec2(pos.x(), pos.y()));

    //ImGui::Text(my_comp.long_name.c_str());

    render_component_simple(my_comp);

    if(((ImGui::IsItemHovered() && ImGui::IsMouseDown(0) && ImGui::IsMouseDragging(0)) || ImGui::IsItemClicked(0)) && !edit.dragging)
    {
        edit.dragging = true;
        edit.dragging_id = my_comp.id;

        cleanup = true;
    }

    if(ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();

        my_comp.render_inline_stats();

        ImGui::EndTooltip();
    }

    ImGui::SetCursorPos(old_pos);
}

void blueprint::render(design_editor& edit)
{
    for(blueprint_node& node : nodes)
    {
        node.render(edit);
    }

    for(int i=0; i < (int)nodes.size(); i++)
    {
        if(nodes[i].cleanup)
        {
            nodes.erase(nodes.begin() + i);
            i--;
            continue;
        }
    }
}

void design_editor::tick(double dt_s)
{

}

std::optional<component*> design_editor::fetch(uint32_t id)
{
    for(component& c : research.components)
    {
        if(c.id != id)
            continue;

        return &c;
    }

    return std::nullopt;
}

void design_editor::render(sf::RenderWindow& win)
{
    if(!open)
        return;

    sf::Mouse mouse;

    vec2f mpos = {mouse.getPosition(win).x, mouse.getPosition(win).y};

    ImGui::SetNextWindowPos(ImVec2(300, 50), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(1000, 800), ImGuiCond_Appearing);

    ImGui::SetNextWindowFocus();

    ImGui::Begin("Blueprint Designer", &open);

    ImVec2 win_screen = ImGui::GetWindowPos();

    auto main_dim = ImGui::GetWindowSize();

    //auto tl_mpos = ImGui::GetCursorScreenPos();

    bool main_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

    research.render(*this, {main_dim.x, main_dim.y});

    cur.render(*this);

    if(!ImGui::IsMouseDown(0) && dragging)
    {
        if(main_hovered)
        {
            std::optional<component*> comp = fetch(dragging_id);

            if(comp)
            {
                component& c = *comp.value();

                //auto tlpos = (vec2f){mpos.x() - tl_mpos.x, mpos.y() - tl_mpos.y};

                //vec2f tlpos = mpos;

                vec2f tlpos = {mpos.x() - win_screen.x, mpos.y() - win_screen.y};

                blueprint_node node;
                node.my_comp = c;
                node.name = c.long_name;
                node.pos = tlpos;

                cur.nodes.push_back(node);
            }
        }
        else
        {
            ///do nothing
        }

        dragging = false;
    }

    if(dragging)
    {
        std::optional<component*> comp = fetch(dragging_id);

        if(comp)
        {
            component& c = *comp.value();

            ImGui::BeginTooltip();

            render_component_simple(c);

            ImGui::EndTooltip();
        }
    }

    ImGui::End();
}
