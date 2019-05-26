#ifndef UI_UTIL_HPP_INCLUDED
#define UI_UTIL_HPP_INCLUDED

#include <string>
#include <imgui/imgui.h>
#include <vec/vec.hpp>

namespace ImGuiX
{
    ImVec4 GetStyleCol(ImGuiCol name);

    bool OutlineHoverText(const std::string& txt, vec3f col, vec3f text_col, bool hover = true, vec2f dim_extra = {0,0}, int thickness = 1, bool force_hover = false, vec3f hover_col = {-1, -1, -1}, int force_hover_thickness = 0);

    void ToggleTextButton(const std::string& txt, vec3f highlight_col, vec3f col, bool is_active);

    void SolidToggleTextButton(const std::string& txt, vec3f highlight_col, vec3f col, bool is_active);

    bool OutlineHoverTextAuto(const std::string& txt, vec3f text_col, bool hover = true, vec2f dim_extra = {0,0}, int thickness = 1, bool force_hover = false);

    /*inline
    void TextColored(const std::string& str, vec3f col)
    {
        TextColored(ImVec4(col.x(), col.y(), col.z(), 1), str.c_str());
    }*/

    void Text(const std::string& str);

    bool SimpleButton(const std::string& str);
    bool SimpleButtonColored(ImVec4 col, const std::string& str);
}

#endif // UI_UTIL_HPP_INCLUDED
