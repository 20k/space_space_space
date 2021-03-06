#include "design_editor.hpp"
#include <SFML/Graphics.hpp>
#include <ImGui/ImGui.h>
#include <iostream>
#include "imgui/imgui_internal.h"
#include "format.hpp"
#include "colours.hpp"
#include "material_info.hpp"

void render_component_simple(const component& c)
{
    ImGui::Button(c.long_name.c_str(), ImVec2(190, 0));
}

///in star ruler info is displayed in a stack above it
void render_component_compact(const component& c, int id, float rad_mult, float real_size)
{
    ImDrawList* lst = ImGui::GetWindowDrawList();

    std::string name = c.short_name;

    float radius = 35 * rad_mult;

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


    std::string object_size = to_string_with(real_size, 2, false);

    auto size_dim = ImGui::CalcTextSize(object_size.c_str());

    float y_pad = 10;

    lst->AddText(ImVec2(cursor_pos.x() - size_dim.x/2, cursor_pos.y() - size_dim.y / 2 - radius - y_pad), IM_COL32(255, 255, 255, 255), object_size.c_str());
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
            //edit.dragging_id = c._pid;
            edit.dragged = c;
            edit.dragging_size = 1;
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

void player_research::merge_into_me(player_research& other)
{
    for(component& c : other.components)
    {
        bool has = false;

        for(component& oc : components)
        {
            if(oc.base_id == c.base_id)
            {
                oc = c;
                has = true;
                break;
            }
        }

        if(!has)
        {
            components.push_back(c);
        }
    }
}

void blueprint_node::render(design_editor& edit, blueprint& parent)
{
    auto old_pos = ImGui::GetCursorPos();

    ImGui::SetCursorPos(ImVec2(pos.x(), pos.y()));

    float rad_mult = sqrt(size);

    my_comp = original;
    my_comp.scale(size * parent.get_overall_size());

    render_component_compact(my_comp, original._pid, rad_mult, size);

    if(((ImGui::IsItemHovered() && ImGui::IsMouseDown(0) && ImGui::IsMouseDragging(0)) || ImGui::IsItemClicked(0)) && !edit.dragging)
    {
        edit.dragging = true;
        //edit.dragging_id = original._pid;
        edit.dragged = original;
        edit.dragging_size = size;

        cleanup = true;
    }

    ImGuiIO& io = ImGui::GetIO();

    if(ImGui::IsItemHovered() && fabs(io.MouseWheel) > 0)
    {
        if(io.MouseWheel > 0)
        {
            size -= 0.25;
        }
        else
        {
            size += 0.25;
        }

        size = round(clamp(size * 4, 0.25*4, 4*4)) / 4;
    }

    if(ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();

        my_comp.render_inline_stats();

        ImGui::EndTooltip();
    }

    ImGui::SetCursorPos(old_pos);
}

void blueprint::add_component_at(const component& c, vec2f pos, float size)
{
    blueprint_node node;
    node.original = c;
    node.size = size;
    node.pos = pos;

    node.original._pid = get_next_persistent_id();

    nodes.push_back(node);
}

blueprint_render_state blueprint::render(design_editor& edit, vec2f upper_size)
{
    ImGui::BeginChild("Test", ImVec2(500, upper_size.y() - 50), true);

    for(blueprint_node& node : nodes)
    {
        node.render(edit, *this);
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

ship blueprint::to_ship() const
{
    ship nship;

    for(blueprint_node c : nodes)
    {
        c.my_comp = c.original;
        c.my_comp.scale(c.size * get_overall_size());

        nship.add(c.my_comp);
    }

    nship.my_size = get_overall_size();
    nship.is_ship = true;
    nship.blueprint_name = name;
    nship.blueprint_tags = tags;
    nship.original_blueprint = std::make_shared<blueprint>(*this);
    nship.new_network_copy();

    return nship;
}

float blueprint::get_build_time_s(float build_power)
{
    return ::get_build_time_s(to_ship(), build_power);
}

void get_ship_cost(const ship& s, std::vector<std::vector<material>>& out)
{
    for(const component& c : s.components)
    {
        bool found = false;

        for(auto& mats : out)
        {
            if(is_equivalent_material(mats, c.composition))
            {
                assert(mats.size() == c.composition.size());

                material_merge(mats, c.composition);
                found = true;
                break;
            }
        }

        if(!found)
        {
            out.push_back(c.composition);
        }

        for(const ship& cs : c.stored)
        {
            get_ship_cost(cs, out);
        }
    }

    for(auto& i : out)
    {
        auto sorter =
        [](const material& one, const material& two)
        {
            return one.type < two.type;
        };

        std::sort(i.begin(), i.end(), sorter);
    }
}

bool shares_blueprint(const ship& s1, const ship& s2)
{
    return s1.blueprint_name == s2.blueprint_name;
}

float get_build_time_s(const ship& s, float build_power)
{
    if(build_power <= 0.000001)
        return 999999999;

    float total_volume = get_build_work(s);

    ///100s for a level 1 factory to build a level 1 ship?
    return SIZE_TO_TIME * total_volume / build_power;
}

float get_build_work(const ship& s)
{
    std::vector<std::vector<material>> cost;
    get_ship_cost(s, cost);

    float total_volume = 0;

    for(auto& m : cost)
    {
        total_volume += material_volume(m);
    }

    return total_volume;
}

std::vector<std::vector<material>> blueprint::get_cost() const
{
    std::vector<std::vector<material>> ret;

    ship mship = to_ship();

    get_ship_cost(mship, ret);

    return ret;
}

void blueprint::add_tag(const std::string& tg)
{
    tags.push_back(tg);
}

void design_editor::tick(double dt_s)
{

}

std::optional<component*> design_editor::fetch(uint32_t id)
{
    for(component& c : research.components)
    {
        if(c._pid != id)
            continue;

        return &c;
    }

    for(blueprint& p : blueprint_manage.blueprints)
    {
        for(blueprint_node& node : p.nodes)
        {
            if(node.original._pid == id)
                return &node.original;
        }
    }

    return std::nullopt;
}

void render_ship_cost(const ship& s, const std::vector<std::vector<material>>& mats)
{
    std::vector<std::vector<material>> cost;
    get_ship_cost(s, cost);

    ImGui::Text("Ship Cost:");

    for(std::vector<material>& mat : cost)
    {
        bool is_sat = material_satisfies({mat}, mats);

        for(int i=0; i < (int)mat.size(); i++)
        {
            material& m = mat[i];

            ImGui::Text(material_info::long_names[m.type].c_str());

            ImGui::SameLine();

            std::string amount = to_string_with_variable_prec(m.dynamic_desc.volume);

            ImVec4 col;

            if(is_sat)
                col = colours::pastel_green;
            else
                col = colours::pastel_red;

            ImGui::TextColored(col, amount.c_str());

            if(i != (int)mat.size() - 1)
            {
                ImGui::SameLine();

                ImGui::Text("/");

                ImGui::SameLine();
            }
        }
    }
}

char make_acceptable(char in)
{
    if(in == ' ')
        return '_';

    in = tolower(in);

    if(!isalpha(in))
        return '_';

    return in;
}

int filter_callback(ImGuiTextEditCallbackData* data)
{
    data->EventChar = make_acceptable(data->EventChar);
    return 0;
}

template<int max_len>
void InputFixedText(const std::string& name, std::string& buffer)
{
    std::array<char, max_len+1> ftag = {};

    for(int kk=0; kk < (int)buffer.size() && kk < max_len; kk++)
    {
        ftag[kk] = buffer[kk];
    }

    int num = buffer.size() + 1;

    if(num < 3)
        num = 3;

    float width = ImGui::CalcTextSize(" ").x * num;

    ImGui::PushItemWidth(width);

    ImGui::InputText(name.c_str(), &ftag[0], max_len, ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_NoHorizontalScroll, filter_callback);

    ImGui::PopItemWidth();

    buffer.resize(strlen(&ftag[0]));

    for(int kk=0; kk < (int)strlen(&ftag[0]); kk++)
    {
        assert(kk < max_len + 1 && kk < (int)buffer.size());

        buffer[kk] = ftag[kk];
    }
}

void clean_tag(std::string& in)
{
    for(int i=0; i < (int)in.size(); i++)
    {
        in[i] = make_acceptable(in[i]);
    }
}

void clean_blueprint_name(std::string& in)
{
    if(in.starts_with("UNFINISHED_"))
    {
        ///yeah heh so ok whatever this works
        in.erase(in.begin());
    }
}

void size_dropdown(int& in)
{
    assert(blueprint_info::names.size() == blueprint_info::sizes.size());

    std::string current_item = blueprint_info::names[in];

    if (ImGui::BeginCombo("##combo", current_item.c_str()))
    {
        for (int n = 0; n < blueprint_info::names.size(); n++)
        {
            bool is_selected = (in == n);
            if (ImGui::Selectable(blueprint_info::names[n].c_str(), is_selected))
                in = n;
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

void design_editor::render(sf::RenderWindow& win)
{
    if(!open)
        return;

    sf::Mouse mouse;

    vec2f mpos = {mouse.getPosition(win).x, mouse.getPosition(win).y};

    ImGui::SetNextWindowPos(ImVec2(300, 50), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(1000, 800), ImGuiCond_Appearing);

    //ImGui::SetNextWindowFocus();

    ImGui::Begin("Blueprint Designer", &open, ImGuiWindowFlags_MenuBar);

    if(ImGui::BeginMenuBar())
    {
        if(ImGui::BeginMenu("File"))
        {
            //ImGui::MenuItem("Hello");

            if(ImGui::BeginMenu("Blueprints (local)"))
            {
                for(int i=0; i < blueprint_manage.blueprints.size(); i++)
                {
                    std::string id = blueprint_manage.blueprints[i].name + "##" + std::to_string(blueprint_manage.blueprints[i]._pid);

                    if(blueprint_manage.blueprints[i].name == "")
                    {
                        id = std::to_string(blueprint_manage.blueprints[i]._pid) + "##" + std::to_string(blueprint_manage.blueprints[i]._pid);
                    }

                    if(ImGui::MenuItem(id.c_str()))
                    {
                        currently_selected = blueprint_manage.blueprints[i]._pid;
                    }
                }

                ImGui::EndMenu();
            }

            if(ImGui::MenuItem("New Blueprint"))
            {
                server_blueprint_manage.create_blueprint_rpc();
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    ///integrates server blueprints into client blueprints
    for(blueprint& p1 : server_blueprint_manage.blueprints)
    {
        bool found = false;

        for(blueprint& p2 : blueprint_manage.blueprints)
        {
            if(p2._pid == p1._pid)
            {
                found = true;
                break;
            }
        }

        if(!found)
        {
            blueprint_manage.blueprints.push_back(p1);
        }
    }

    /*if(blueprint_manage.blueprints.size() == 0 && !rpcd_default_blueprint)
    {
        server_blueprint_manage.create_blueprint_rpc();
        rpcd_default_blueprint = true;

        ImGui::End();
        return;
    }*/

    if(blueprint_manage.blueprints.size() == 0)
    {
        ImGui::End();
        return;
    }

    blueprint* current_blueprint = nullptr;

    for(blueprint& print : blueprint_manage.blueprints)
    {
        if(print._pid == currently_selected)
        {
            current_blueprint = &print;
        }
    }

    if(current_blueprint == nullptr)
    {
        current_blueprint = &blueprint_manage.blueprints.front();
    }

    blueprint& cur = *current_blueprint;

    std::string sname = cur.name;
    sname.resize(100);

    ImGui::PushItemWidth(160);

    ImGui::InputText("Name", &sname[0], sname.size());

    cur.name.clear();

    for(int i=0; i < (int)sname.size() && sname[i] != 0; i++)
    {
        cur.name.push_back(sname[i]);
    }

    clean_blueprint_name(cur.name);

    /*ImGui::InputFloat("Scale", &cur.overall_size, 0.1, 1, "%.2f");

    cur.overall_size = clamp(cur.overall_size, 0.01, 100);*/

    size_dropdown(cur.size_offset);

    ImGui::PopItemWidth();

    ImGui::SameLine();

    if(ImGui::Button("Upload"))
    {
        printf("ClientUpload\n");

        server_blueprint_manage.upload_blueprint_rpc(cur);
    }

    ImGui::BeginGroup();

    ImGui::Text("Tags:");

    constexpr int max_len = 20;

    for(int i=0; i < (int)cur.tags.size(); i++)
    {
        ImGui::SameLine();

        InputFixedText<max_len>("##" + std::to_string(i), cur.tags[i]);
        clean_tag(cur.tags[i]);

        ImGui::SameLine();

        if(ImGui::Button("-"))
        {
            cur.tags.erase(cur.tags.begin() + i);
            i--;
            continue;
        }
    }

    ImGui::SameLine();

    InputFixedText<max_len>("##-1", cur.unbaked_tag);
    clean_tag(cur.unbaked_tag);

    ImGui::SameLine();

    if(ImGui::Button("+") && cur.unbaked_tag.size() > 0)
    {
        cur.add_tag(cur.unbaked_tag);
        cur.unbaked_tag = "";
    }

    ImGui::EndGroup();

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

    ImGui::NewLine();

    render_ship_cost(s, std::vector<std::vector<material>>());

    ImGui::EndChild();

    bool main_hovered = brender.is_hovered;

    if(!ImGui::IsMouseDown(0) && dragging)
    {
        if(main_hovered)
        {
            //std::optional<component*> comp = fetch(dragging_id);

            //if(comp)
            {
                //component& c = *comp.value();
                component& c = dragged;

                vec2f tlpos = mpos - brender.pos;

                blueprint_node node;
                node.original = c;
                node.my_comp = c;
                node.name = c.long_name;
                node.pos = tlpos;
                node.size = dragging_size;

                //node.my_comp.scale(node.size);

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
        //std::optional<component*> comp = fetch(dragging_id);

        //if(comp)
        {
            //component& c = *comp.value();
            component& c = dragged;

            ImGui::BeginTooltip();

            render_component_simple(c);

            ImGui::EndTooltip();
        }
        /*else
        {
            std::cout << "COMP " << dragging_id << std::endl;
        }*/
    }

    ImGui::End();
}
