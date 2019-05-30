#ifndef CODE_EDITOR_HPP_INCLUDED
#define CODE_EDITOR_HPP_INCLUDED

#include <ImGuiColorTextEdit/TextEditor.h>
#include <imgui_club/imgui_memory_editor/imgui_memory_editor.h>

struct cpu_state;

struct code_editor
{
    TextEditor text_edit;
    MemoryEditor mem_edit;

    void render(cpu_state& cpu);
};

#endif // CODE_EDITOR_HPP_INCLUDED
