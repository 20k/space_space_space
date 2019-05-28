#include "code_editor.hpp"
#include "script_execution.hpp"
#include "format.hpp"
#include "ui_util.hpp"
#include <iostream>

void code_editor::render(cpu_state& cpu)
{
    ImGui::Begin("Code Editor");
    text_edit.Render("Code", ImVec2(200, 0));

    ImGui::SameLine();

    ImGui::BeginChild("Registers");

    for(int i=0; i < (int)registers::COUNT; i++)
    {
        std::string name = registers::rnames[i];

        register_value& val = cpu.register_states[i];

        std::string extra = val.as_string();

        ImGui::Button(format(name, registers::rnames).c_str());

        ImGui::SameLine();

        ImGui::Text(extra.c_str());
    }

    if(ImGuiX::SimpleButton("(Step)"))
    {
        cpu.inc_pc_rpc();
    }

    if(ImGuiX::SimpleButton("(Upload)"))
    {
        cpu.upload_program_rpc(text_edit.GetText());
    }

    ImGui::TextWrapped(cpu.last_error.c_str());

    ImGui::EndChild();

    ImGui::End();
}
