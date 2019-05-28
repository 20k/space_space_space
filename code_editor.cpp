#include "code_editor.hpp"

void code_editor::render()
{
    ImGui::Begin("Code Editor");
    text_edit.Render("Code");

    ImGui::End();
}
