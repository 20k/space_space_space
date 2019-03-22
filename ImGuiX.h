#ifndef IMGUIX_H_INCLUDED
#define IMGUIX_H_INCLUDED

#include <vector>
#include <string>
#include <imgui/imgui.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui/imgui_internal.h>

namespace ImGuiX
{
    void PlotMultiEx(
    ImGuiPlotType plot_type,
    const char* label,
    const std::vector<std::string>& names,
    const std::vector<ImColor>& colors,
    const std::vector<std::vector<float>>& data,
    float scale_min,
    float scale_max,
    ImVec2 graph_size);

    bool SliderFloat(const std::string& label, float* v, float v_min, float v_max);
}

#endif // IMGUIX_H_INCLUDED
