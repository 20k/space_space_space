#include "prebaked_programs.hpp"

prebaked_program_manager::prebaked_program_manager()
{
    {
        prebaked_program prog;
        prog.name = "Missile";

        ///so
        ///need to go through radar data and look for targets, aka pick random high heat target?
        ///once a target has been picked, need to spam KEEP TARGET -100, and activate self destruct if the distance is too low
        ///remember to blacklist any targets in the NOHIT file
        prog.program =
        R"(

        )";

        programs.push_back(prog);
    }
}

prebaked_program_manager& get_programs()
{
    static prebaked_program_manager manage;

    return manage;
}

std::optional<prebaked_program> fetch_program(const std::string& name)
{
    prebaked_program_manager& programs = get_programs();

    for(auto& i : programs.programs)
    {
        if(i.name == name)
            return i;
    }

    return std::nullopt;
}
