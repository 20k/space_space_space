#include "design_editor.hpp"
#include <SFML/Graphics.hpp>
#include <ImGui/ImGui.h>
#include <iostream>
#include "imgui/imgui_internal.h"

void render_component_simple(const component& c)
{
    ImGui::Button(c.long_name.c_str(), ImVec2(190, 0));
}

void render_component_compact(const component& c, int id)
{
    ImDrawList* lst = ImGui::GetWindowDrawList();

    std::string name = c.short_name;

    float radius = 35;

    vec2f cursor_pos = {ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y};

    //ImGui::Button(c.short_name.c_str(), ImVec2(190, 0));

    vec2f text_dim = xy_to_vec(ImGui::CalcTextSize(name.c_str()));

    lst->AddCircleFilled(ImVec2(cursor_pos.x(), cursor_pos.y()), radius, IM_COL32(128, 128, 128, 128), 80);
    lst->AddText(ImVec2(cursor_pos.x() - text_dim.x()/2, cursor_pos.y() - text_dim.y()/2), IM_COL32(255,255,255,255), name.c_str());

    ImGuiWindow* window = ImGui::GetCurrentWindow();

    std::string full_id = std::to_string(id);

    ImGuiID iguid = window->GetID(full_id.c_str());

    ImVec2 size = ImVec2(radius*2, radius*2);

    const ImRect bb(ImVec2(cursor_pos.x() - radius, cursor_pos.y() - radius), ImVec2(cursor_pos.x() + size.x - radius, cursor_pos.y() + size.y - radius));
    ImGui::ItemSize(bb, 0);

    if(!ImGui::ItemAdd(bb, iguid))
        return;

    bool hovered, held;

    bool pressed = ImGui::ButtonBehavior(bb, iguid, &hovered, &held, 0);
}

void player_research::render(design_editor& edit, vec2f upper_size)
{
    float fixed_width = 200;

    ImGui::BeginChild("Components", ImVec2(fixed_width, upper_size.y() - 50), true);

    for(component& c : components)
    {
        std::string txt = c.long_name;

        render_component_simple(c);

        if(((ImGui::IsItemHovered() && ImGui::IsMouseDown(0) && ImGui::IsMouseDragging(0)) || ImGui::IsItemClicked(0)) && !edit.dragging)
        {
            edit.dragging = true;
            edit.dragging_id = c.id;
        }

        if(ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();

            c.render_inline_stats();

            ImGui::EndTooltip();
        }
    }

    ImGui::EndChild();
}

void blueprint_node::render(design_editor& edit)
{
    auto old_pos = ImGui::GetCursorPos();

    ImGui::SetCursorPos(ImVec2(pos.x(), pos.y()));

    render_component_compact(my_comp, id);

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

blueprint_render_state blueprint::render(design_editor& edit, vec2f upper_size)
{
    ImGui::BeginChild("Test", ImVec2(500, upper_size.y() - 50), true);

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

    bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

    auto win_pos = ImGui::GetWindowPos();

    ImGui::EndChild();

    return {{win_pos.x, win_pos.y},  hovered};
}

ship blueprint::to_ship()
{
    ship nship;

    for(blueprint_node& c : nodes)
    {
        nship.add(c.my_comp);
    }

    return nship;
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

    //ImVec2 win_screen = ImGui::GetWindowPos();

    auto main_dim = ImGui::GetWindowSize();

    //auto tl_mpos = ImGui::GetCursorScreenPos();

    research.render(*this, {main_dim.x, main_dim.y});

    ImGui::SameLine();

    blueprint_render_state brender = cur.render(*this, {main_dim.x, main_dim.y});

    ImGui::SameLine();

    ImGui::BeginChild("Shippy", ImVec2(250, main_dim.y - 50), true);

    ship s = cur.to_ship();

    s.show_resources(false);

    ImGui::EndChild();

    bool main_hovered = brender.is_hovered;

    if(!ImGui::IsMouseDown(0) && dragging)
    {
        if(main_hovered)
        {
            std::optional<component*> comp = fetch(dragging_id);

            if(comp)
            {
                component& c = *comp.value();

                vec2f tlpos = mpos - brender.pos;

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
