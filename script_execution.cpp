#include "script_execution.hpp"
#include <assert.h>

void register_value::make(const std::string& str)
{
    if(str.size() == 0)
        throw std::runtime_error("Bad register value, 0 length");

    if(str.size() == 1)
    {
        if(str[0] == 'G' || str[0] == 'X')
        {
            set_reg(registers::GENERAL_PURPOSE0);
        }
        else if(str[0] == 'T')
        {
            set_reg(registers::TEST);
        }
        else
        {
            throw std::runtime_error("Bad register " + str);
        }
    }

    if(str.size() == 2)
    {
        if(str[0] == 'X' || str[0] == 'G')
        {
            int num = str[1] - '0';

            if(num == 0)
            {
                set_reg(registers::GENERAL_PURPOSE0);
            }
            else
            {
                throw std::runtime_error("No G/X registers available > 0");
            }
        }
    }

    if(str.size() >= 2)
    {
        if(str.front() == '\"' && str.back() == '\"')
        {
            std::string stripped = str;
            stripped.erase(stripped.begin());
            stripped.pop_back();

            set_symbol(stripped);
            return;
        }

        if(str.front() == '\"' && str.back() != '\"')
            throw std::runtime_error("String not terminated");

        set_label(str);
    }
}

instructions::type instructions::fetch(const std::string& name)
{
    for(int i=0; i < (int)instructions::rnames.size(); i++)
    {
        if(instructions::rnames[i] == name)
            return (instructions::type)i;
    }

    return instructions::COUNT;
}

void instruction::make(const std::vector<std::string>& raw)
{
    if(raw.size() == 0)
        throw std::runtime_error("Bad instr");

    instructions::type found = instructions::fetch(raw[0]);

    if(found == instructions::COUNT)
    {
        throw std::runtime_error("Bad instruction " + raw[0]);
    }

    for(int i=1; i < (int)raw.size(); i++)
    {
        register_value val;
        val.make(raw[i]);

        args.push_back(val);
    }
}

void cpu_state::step()
{

}

void cpu_state::debug_state()
{

}

void cpu_tests()
{
    assert(instructions::rnames.size() == instructions::COUNT);

    cpu_state test;

    instruction i1;
    i1.make({"ADDI", "1", "2", "X"});

    test.inst.push_back(i1);

    test.step();

    test.debug_state();
}
