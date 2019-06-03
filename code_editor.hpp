#ifndef CODE_EDITOR_HPP_INCLUDED
#define CODE_EDITOR_HPP_INCLUDED

#include <ImGuiColorTextEdit/TextEditor.h>
#include "file_editor.hpp"

struct cpu_state;

struct code_editor
{
    TextEditor text_edit;
    file_editor file_edit;

    bool show_file = true;
    float audio_volume = 1;

    void render(cpu_state& cpu);
};

#endif // CODE_EDITOR_HPP_INCLUDED
