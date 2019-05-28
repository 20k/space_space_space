#include "script_execution.hpp"
#include <assert.h>

///need to parse 0xFF to 255

bool all_numeric(const std::string& str)
{
    for(auto i : str)
    {
        if(!isdigit(i))
            return false;
    }

    return true;
}

void register_value::make(const std::string& str)
{
    if(str.size() == 0)
        throw std::runtime_error("Bad register value, 0 length");

    if(all_numeric(str))
    {
        try
        {
            int val = std::stoi(str);

            set_int(val);
        }
        catch(...)
        {
            throw std::runtime_error("Not a valid integer " + str);
        }
    }

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
        else if(isalpha(str[0]))
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

register_value& instruction::fetch(int idx)
{
    if(idx < 0 || idx >= (int)args.size())
        throw std::runtime_error("Bad instruction idx " + std::to_string(idx));

    return args[idx];
}

std::string register_value::as_string()
{
    if(is_reg())
    {
        return registers::rnames[(int)reg];
    }

    if(is_int())
    {
        return std::to_string(value);
    }

    if(is_symbol())
    {
        return + "\"" + symbol + "\"";
    }

    if(is_label())
    {
        return label;
    }

    throw std::runtime_error("Bad register val?");
}

register_value& register_value::decode(cpu_state& state)
{
    if(is_reg())
    {
        return state.fetch(reg);
    }

    return *this;
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

    type = found;

    for(int i=1; i < (int)raw.size(); i++)
    {
        register_value val;
        val.make(raw[i]);

        args.push_back(val);
    }
}

cpu_state::cpu_state()
{
    for(int i=0; i < registers::COUNT; i++)
    {
        register_value val;
        val.set_int(0);

        register_states[(registers::type)i] = val;
    }
}

/*struct value_r
{
    register_value& in;

    value_r(register_value& val) : in(val)
    {
        if(!)
    }
};*/

register_value& restrict_r(register_value& in)
{
    if(!in.is_reg())
        throw std::runtime_error("Expected register, got " + in.as_string());

    return in;
}

register_value& restrict_all(register_value& in)
{
    //if(!in.is_reg() && !in.is_int())
    //    throw std::runtime_error("Expected register or integer, got " + in.as_string());

    return in;
}

#define R(x) restrict_r(x)
#define RN(x) restrict_rn(x)
#define E(x) x

void icopy(register_value& one, register_value& two)
{
    two = one;
}

void cpu_state::step()
{
    if(inst.size() == 0)
        return;

    pc %= (int)inst.size();

    if(pc < 0 || pc >= (int)inst.size())
        throw std::runtime_error("Bad pc somehow");

    using namespace instructions;

    instruction& next = inst[pc];

    switch(next.type)
    {
    case COPY:
        printf("COPY\n");
        icopy(E(next.fetch(0)).decode(*this), R(next.fetch(1)).decode(*this));
        break;
    }

    pc++;
}

void cpu_state::debug_state()
{
    printf("PC %i\n", pc);

    for(auto& i : register_states)
    {
        std::string name = registers::rnames[(int)i.first];
        std::string val = i.second.as_string();

        printf("%s: %s\n", name.c_str(), val.c_str());
    }
}

register_value& cpu_state::fetch(registers::type type)
{
    auto it = register_states.find(type);

    if(it == register_states.end())
        throw std::runtime_error("No such register " + std::to_string((int)type));

    return it->second;
}

void cpu_tests()
{
    assert(instructions::rnames.size() == instructions::COUNT);

    cpu_state test;

    instruction i1;
    //i1.make({"ADDI", "1", "2", "X"});

    i1.make({"COPY", "1", "X"});

    test.inst.push_back(i1);

    test.step();

    test.debug_state();
}
