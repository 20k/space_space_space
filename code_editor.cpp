#include "code_editor.hpp"
#include "script_execution.hpp"
#include "format.hpp"
#include "ui_util.hpp"
#include <iostream>

void code_editor::render(cpu_state& cpu)
{
    ImGui::Begin("Code Editor");

    int pc_loop = cpu.context.pc + 1;

    if(cpu.inst.size() == 0)
        pc_loop = 0;

    if(pc_loop > (int)cpu.inst.size())
        pc_loop %= (int)cpu.inst.size();

    text_edit.SetBreakpoints({pc_loop});

    text_edit.Render("Code", ImVec2(400, 0));

    ImGui::SameLine();

    ImGui::BeginChild("Registers");

    for(int i=0; i < (int)registers::COUNT; i++)
    {
        std::string name = registers::rnames[i];

        register_value& val = cpu.context.register_states[i];

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

    if(ImGuiX::SimpleButton("(Reset)"))
    {
        cpu.reset_rpc();
    }

    ImGui::TextWrapped(cpu.last_error.c_str());

    ImGui::Checkbox("Show File?", &show_file);

    int max_data = 128*128;

    ///so
    ///this basic editor is nice but its not quite suitable for what we need
    ///firstly: needs variable width columns, where we display whole values in a column instead of
    ///a fixed representation
    ///secondly, needs to support strings and other datatypes
    ///editor is not too long so should be pretty easy to retrofit
    std::vector<char> test_data;

    if(show_file)
    {
        if(cpu.context.held_file_id != -1)
        {
            auto it = cpu.files.find(cpu.context.held_file_id);

            if(it != cpu.files.end())
            {
                cpu_file& fle = it->second;

                //ImGui::Begin((fle.name.as_string() + "###Files").c_str());

                file_edit.render(fle);
            }
        }
        else
        {
            //ImGui::Begin("Files###Files");
        }

        //ImGui::End();
    }

    ImGui::EndChild();

    ImGui::End();
}
