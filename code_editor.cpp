#include "code_editor.hpp"
#include "script_execution.hpp"
#include "format.hpp"
#include "ui_util.hpp"
#include <iostream>

void code_editor::render(cpu_state& cpu)
{
    ImGui::Begin("Code Editor");

    int pc_loop = cpu.pc + 1;

    if(cpu.inst.size() == 0)
        pc_loop = 0;

    if(pc_loop > (int)cpu.inst.size())
        pc_loop %= (int)cpu.inst.size();

    text_edit.SetBreakpoints({pc_loop});

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

    if(ImGuiX::SimpleButton("(Reset)"))
    {
        cpu.reset_rpc();
    }

    ImGui::TextWrapped(cpu.last_error.c_str());

    int max_data = 128*128;

    ///so
    ///this basic editor is nice but its not quite suitable for what we need
    ///firstly: needs variable width columns, where we display whole values in a column instead of
    ///a fixed representation
    ///secondly, needs to support strings and other datatypes
    ///editor is not too long so should be pretty easy to retrofit
    std::vector<char> test_data;

    if(cpu.held_file != -1 && cpu.held_file >= 0 && cpu.held_file < (int)cpu.files.size())
    {
        cpu_file& fle = cpu.files[cpu.held_file];

        for(int i=0; i < (int)fle.len() && i < max_data; i++)
        {
            register_value& val = fle.data[i];

            if(val.is_int())
            {
                test_data.push_back(val.value);
            }
            else
            {
                test_data.push_back(0);
            }
        }

        file_edit.render(fle);
    }

    if(test_data.size() == 0)
        test_data.resize(1);

    //mem_edit.DrawWindow("MemEdit", &test_data[0], test_data.size());

    ImGui::EndChild();

    ImGui::End();
}
