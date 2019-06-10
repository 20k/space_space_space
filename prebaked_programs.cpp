#include "prebaked_programs.hpp"

prebaked_program_manager::prebaked_program_manager()
{
    {
        prebaked_program prog;
        prog.name = "missile";

        ///so
        ///need to go through radar data and look for targets, aka pick random high heat target?
        ///once a target has been picked, need to spam KEEP TARGET -100, and activate self destruct if the distance is too low
        ///remember to blacklist any targets in the NOHIT file
        prog.program =
        R"(
@DEF MTARGET reg skip
GRAB RADAR_DATA
COPY -1 X0 ; init best match
COPY 0 reg ; init result
SEEK 512 ; seek counter
SEEK 1 ; seek first id

MARK RADAR_LABEL
SEEK 5 ; ID to INTENSITY

TEST F > X0 ; advance pointer to next id
FJMP SKIPPY ; didn't do better than current
SEEK -6 ; move pointer back to id
TEST F = skip ; shouldn't hit this target
TJMP SKIPPY
SEEK -1 ; move pointer back to id
COPY F reg ; copy id into reg
SEEK 4 ; move back to intensity
COPY F X0 ; copy intensity into x0, on id

MARK SKIPPY
TEST EOF
FJMP RADAR_LABEL
DROP
@END

@DEF SLEEP
TIME X0
ADDI X0 1000 X0

MARK TLOOP
TIME T
TEST T < X0
TJMP TLOOP

@END

COPY -1 T

SLEEP

FASK SKIP
FJMP NEXT
COPY F T
DROP

MARK NEXT
MTARGET X0 T ; T is discarded, X0 is target

TDST X0 T
TEST T = 0
TJMP BAD

MARK KILL
TDST X0 T
TEST T < 2
TJMP EXPLODE
KEEP -100 X0
JUMP KILL

MARK EXPLODE
GRAB DESTRUCT_0_HW
COPY 1 [0x8]
DROP
MARK BAD
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
