#include "file_editor.hpp"
#include "script_execution.hpp"
#include <imgui/imgui.h>
#include <stdio.h>      // sprintf, scanf
#include <stdint.h>     // uint8_t, etc.

#define _PRISizeT   "z"
#define ImSnprintf  snprintf

struct Sizes
{
    int     AddrDigitsCount;
    float   LineHeight;
    float   GlyphWidth;
    float   HexCellWidth;
    float   SpacingBetweenMidCols;
    float   PosHexStart;
    float   PosHexEnd;
    float   PosAsciiStart;
    float   PosAsciiEnd;
    float   WindowWidth;
};

void CalcSizes(file_editor& edit, Sizes& s, size_t mem_size, size_t base_display_addr)
{
    ImGuiStyle& style = ImGui::GetStyle();
    s.AddrDigitsCount = edit.OptAddrDigitsCount;
    if (s.AddrDigitsCount == 0)
        for (size_t n = base_display_addr + mem_size - 1; n > 0; n >>= 4)
            s.AddrDigitsCount++;
    s.LineHeight = ImGui::GetTextLineHeight();
    s.GlyphWidth = ImGui::CalcTextSize("F").x + 1;                  // We assume the font is mono-space
    s.HexCellWidth = (float)(int)(s.GlyphWidth * 2.5f);             // "FF " we include trailing space in the width to easily catch clicks everywhere
    s.SpacingBetweenMidCols = (float)(int)(s.HexCellWidth * 0.25f); // Every OptMidColsCount columns we add a bit of extra spacing
    s.PosHexStart = (s.AddrDigitsCount + 2) * s.GlyphWidth;
    s.PosHexEnd = s.PosHexStart + (s.HexCellWidth * edit.Cols);
    s.PosAsciiStart = s.PosAsciiEnd = s.PosHexEnd;
    if (edit.OptShowAscii)
    {
        s.PosAsciiStart = s.PosHexEnd + s.GlyphWidth * 1;
        if (edit.OptMidColsCount > 0)
            s.PosAsciiStart += (float)((edit.Cols + edit.OptMidColsCount - 1) / edit.OptMidColsCount) * s.SpacingBetweenMidCols;
        s.PosAsciiEnd = s.PosAsciiStart + edit.Cols * s.GlyphWidth;
    }
    s.WindowWidth = s.PosAsciiEnd + style.ScrollbarSize + style.WindowPadding.x * 2 + s.GlyphWidth;
}

std::string address_to_str(int in)
{
    return std::to_string(in);
}

///should clip long strings optionally, display full on hover
void file_editor::render(cpu_file& file)
{
    std::vector<register_value>& vals = file.data;

    int mem_size = file.len();

    size_t base_display_addr = 0;

    Sizes s;
    CalcSizes(*this, s, mem_size, 0);

    float footer_height = 0;

    ImGui::BeginChild("##scrolling", ImVec2(0, -footer_height), false, ImGuiWindowFlags_NoMove);

    std::string cvals = "FP: " + address_to_str(file.file_pointer) + ": " + file.data[file.file_pointer].as_string();

    ImGui::Text(cvals.c_str());

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    const int line_total_count = (int)((mem_size + Cols - 1) / Cols);
    ImGuiListClipper clipper(line_total_count, s.LineHeight);
    const size_t visible_start_addr = clipper.DisplayStart * Cols;
    const size_t visible_end_addr = clipper.DisplayEnd * Cols;

    bool ReadOnly = true;

    if (ReadOnly || DataEditingAddr >= mem_size)
        DataEditingAddr = (size_t)-1;
    if (DataPreviewAddr >= mem_size)
        DataPreviewAddr = (size_t)-1;

    size_t preview_data_type_size = 0;

    size_t data_editing_addr_backup = DataEditingAddr;
    size_t data_editing_addr_next = (size_t)-1;
    if (DataEditingAddr != (size_t)-1)
    {
        // Move cursor but only apply on next frame so scrolling with be synchronized (because currently we can't change the scrolling while the window is being rendered)
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow)) && DataEditingAddr >= (size_t)Cols)          { data_editing_addr_next = DataEditingAddr - Cols; DataEditingTakeFocus = true; }
        else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow)) && DataEditingAddr < mem_size - Cols) { data_editing_addr_next = DataEditingAddr + Cols; DataEditingTakeFocus = true; }
        else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow)) && DataEditingAddr > 0)               { data_editing_addr_next = DataEditingAddr - 1; DataEditingTakeFocus = true; }
        else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow)) && DataEditingAddr < mem_size - 1)   { data_editing_addr_next = DataEditingAddr + 1; DataEditingTakeFocus = true; }
    }
    if (data_editing_addr_next != (size_t)-1 && (data_editing_addr_next / Cols) != (data_editing_addr_backup / Cols))
    {
        // Track cursor movements
        const int scroll_offset = ((int)(data_editing_addr_next / Cols) - (int)(data_editing_addr_backup / Cols));
        const bool scroll_desired = (scroll_offset < 0 && data_editing_addr_next < visible_start_addr + Cols * 2) || (scroll_offset > 0 && data_editing_addr_next > visible_end_addr - Cols * 2);
        if (scroll_desired)
            ImGui::SetScrollY(ImGui::GetScrollY() + scroll_offset * s.LineHeight);
    }

    // Draw vertical separator
    ImVec2 window_pos = ImGui::GetWindowPos();
    if (OptShowAscii)
        draw_list->AddLine(ImVec2(window_pos.x + s.PosAsciiStart - s.GlyphWidth, window_pos.y), ImVec2(window_pos.x + s.PosAsciiStart - s.GlyphWidth, window_pos.y + 9999), ImGui::GetColorU32(ImGuiCol_Border));

    const ImU32 color_text = ImGui::GetColorU32(ImGuiCol_Text);
    const ImU32 color_disabled = OptGreyOutZeroes ? ImGui::GetColorU32(ImGuiCol_TextDisabled) : color_text;

    const char* format_address = OptUpperCaseHex ? "%0*" _PRISizeT "X: " : "%0*" _PRISizeT "x: ";
    const char* format_data = OptUpperCaseHex ? "%0*" _PRISizeT "X" : "%0*" _PRISizeT "x";
    const char* format_range = OptUpperCaseHex ? "Range %0*" _PRISizeT "X..%0*" _PRISizeT "X" : "Range %0*" _PRISizeT "x..%0*" _PRISizeT "x";
    const char* format_byte = OptUpperCaseHex ? "%02X" : "%02x";
    const char* format_byte_space = OptUpperCaseHex ? "%02X " : "%02x ";

    for (int line_i = clipper.DisplayStart; line_i < clipper.DisplayEnd; line_i++) // display only visible lines
    {
        size_t addr = (size_t)(line_i * Cols);
        ImGui::Text(format_address, s.AddrDigitsCount, base_display_addr + addr);

        // Draw Hexadecimal
        for (int n = 0; n < Cols && addr < mem_size; n++, addr++)
        {
            float byte_pos_x = s.PosHexStart + s.HexCellWidth * n;
            if (OptMidColsCount > 0)
                byte_pos_x += (float)(n / OptMidColsCount) * s.SpacingBetweenMidCols;
            ImGui::SameLine(byte_pos_x);

            // Draw highlight
            bool is_highlight_from_user_range = (addr >= HighlightMin && addr < HighlightMax);
            bool is_highlight_from_user_func = false;
            bool is_highlight_from_preview = (addr >= DataPreviewAddr && addr < DataPreviewAddr + preview_data_type_size);
            if (is_highlight_from_user_range || is_highlight_from_user_func || is_highlight_from_preview)
            {
                ImVec2 pos = ImGui::GetCursorScreenPos();
                float highlight_width = s.GlyphWidth * 2;
                bool is_next_byte_highlighted =  (addr + 1 < mem_size) && ((HighlightMax != (size_t)-1 && addr + 1 < HighlightMax) || false);
                if (is_next_byte_highlighted || (n + 1 == Cols))
                {
                    highlight_width = s.HexCellWidth;
                    if (OptMidColsCount > 0 && n > 0 && (n + 1) < Cols && ((n + 1) % OptMidColsCount) == 0)
                        highlight_width += s.SpacingBetweenMidCols;
                }
                draw_list->AddRectFilled(pos, ImVec2(pos.x + highlight_width, pos.y + s.LineHeight), HighlightColor);
            }

            {
                register_value& val = vals[addr];

                #if 0
                uint8_t b = 0;

                if(val.is_int())
                {
                    b = val.value;
                }

                /*if (OptShowHexII)
                {
                    if ((b >= 32 && b < 128))
                        ImGui::Text(".%c ", b);
                    else if (b == 0xFF && OptGreyOutZeroes)
                        ImGui::TextDisabled("## ");
                    else if (b == 0x00)
                        ImGui::Text("   ");
                    else
                        ImGui::Text(format_byte_space, b);
                }
                else*/
                {
                    if (b == 0 && OptGreyOutZeroes)
                        ImGui::TextDisabled("00 ");
                    else
                        ImGui::Text(format_byte_space, b);
                }
                #endif // 0

                if(val.is_int())
                {
                    uint8_t b = val.value;

                    if (b == 0 && OptGreyOutZeroes)
                        ImGui::TextDisabled("00 ");
                    else
                        ImGui::Text(format_byte_space, b);
                }

                if(val.is_symbol())
                {
                    ImGui::Text("\"\" ");
                    //ImGui::Text(val.symbol.c_str());
                }

                if(val.is_label())
                {
                    ImGui::Text("@@");
                }

                if(ImGui::IsItemHovered())
                {
                    std::string str = address_to_str(addr) + ": " + val.as_string();

                    ImGui::SetTooltip(str.c_str());
                }

                if (!ReadOnly && ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
                {
                    DataEditingTakeFocus = true;
                    data_editing_addr_next = addr;
                }
            }
        }
    }
    clipper.End();
    ImGui::PopStyleVar(2);
    ImGui::EndChild();
}
