#ifndef CODE_EDITOR_HPP_INCLUDED
#define CODE_EDITOR_HPP_INCLUDED

#include <ImGuiColorTextEdit/TextEditor.h>

struct cpu_state;

struct code_editor
{
    TextEditor text_edit;

    void render(cpu_state& cpu);
};

#endif // CODE_EDITOR_HPP_INCLUDED
