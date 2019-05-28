#ifndef CODE_EDITOR_HPP_INCLUDED
#define CODE_EDITOR_HPP_INCLUDED

#include <ImGuiColorTextEdit/TextEditor.h>

struct code_editor
{
    TextEditor text_edit;

    void render();
};

#endif // CODE_EDITOR_HPP_INCLUDED
