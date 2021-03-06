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

    bool SliderFloat(const std::string& label, float* v, float v_min, float v_max, std::string display_format = "%.3f");
    bool VSliderFloat(const std::string& label, float* v, float v_min, float v_max, std::string display_format = "%.3f");

    void ProgressBarPseudo(float fraction, const ImVec2& size_arg, const std::string& overlay, ImVec4 col);
}

#endif // IMGUIX_H_INCLUDED
