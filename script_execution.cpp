#include "script_execution.hpp"
#include <assert.h>
#include <iostream>

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

            return;
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
            return;
        }
        else if(str[0] == 'T')
        {
            set_reg(registers::TEST);
            return;
        }
        else if(isalpha(str[0]))
        {
            throw std::runtime_error("Bad register " + str);
        }

        if(str[0] == '>' || str[0] == '<' || str[0] == '=')
        {
            set_symbol(str);
            return;
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

            return;
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

register_value& restrict_n(register_value& in)
{
    if(!in.is_int())
        throw std::runtime_error("Expected value, got " + in.as_string());

    return in;
}

register_value& restrict_rn(register_value& in)
{
    if(!in.is_reg() && !in.is_int())
        throw std::runtime_error("Expected register or integer, got " + in.as_string());

    return in;
}

register_value& restrict_l(register_value& in)
{
    if(!in.is_label())
        throw std::runtime_error("Expected label, got " + in.as_string());

    return in;
}

register_value& restrict_s(register_value& in)
{
    if(!in.is_symbol())
        throw std::runtime_error("Expected symbol, got " + in.as_string());

    return in;
}

register_value& pseudo_symbol(register_value& in)
{
    ///BIT HACKY
    if(!in.is_label())
        throw std::runtime_error("Expected <, > or =, got " + in.as_string());

    return in;
}

register_value& restrict_all(register_value& in)
{
    //if(!in.is_reg() && !in.is_int())
    //    throw std::runtime_error("Expected register or integer, got " + in.as_string());

    return in;
}

#define R(x) restrict_r(x).decode(*this) ///register only
#define RN(x) restrict_rn(x).decode(*this) ///register or number
#define E(x) x.decode(*this) ///everything
#define L(x) restrict_l(x).decode(*this)

#define SYM(x) restrict_s(x)

#define NUM(x) restrict_n(x)

#define A1(x) x(next[0])
#define A2(x, y) x(next[0]), y(next[1])
#define A3(x, y, z) x(next[0]), y(next[1]), z(next[2])

#define CALL1(x, y) x(y(next[0]))
#define CALL2(x, y, z) x(y(next[0]), z(next[1]))
#define CALL3(x, y, z, w) x(y(next[0]), z(next[1]), w(next[2]))

#include "instructions.hpp"

void cpu_state::step()
{
    if(inst.size() == 0)
        return;

    pc %= (int)inst.size();

    if(pc < 0 || pc >= (int)inst.size())
        throw std::runtime_error("Bad pc somehow");

    using namespace instructions;

    instruction& next = inst[pc];

    if(next.type == instructions::COUNT)
        throw std::runtime_error("Bad instruction at runtime?");

    std::cout << "NEXT " << instructions::rnames[(int)next.type] << std::endl;

    switch(next.type)
    {
    case COPY:
        CALL2(icopy, E, R);
        break;
    case ADDI:
        CALL3(iaddi, RN, RN, R);
        break;
    case SUBI:
        CALL3(isubi, RN, RN, R);
        break;
    case MULI:
        CALL3(imuli, RN, RN, R);
        break;
    case DIVI:
        CALL3(idivi, RN, RN, R);
        break;
    case MODI:
        CALL3(imodi, RN, RN, R);
        break;
    case MARK:///pseudo instruction
        pc++;
        step();
        return;
        break;
    case SWIZ:
        throw std::runtime_error("Unimplemented SWIZ");
    case JUMP:
        pc = label_to_pc(L(next[0]).label);
        return;
        break;
    case TJMP:
        if(fetch(registers::TEST).is_int() && fetch(registers::TEST).value != 0)
        {
            pc = label_to_pc(L(next[0]).label);
            return;
        }

        break;
    case FJMP:
        if(fetch(registers::TEST).is_int() && fetch(registers::TEST).value == 0)
        {
            pc = label_to_pc(L(next[0]).label);
            return;
        }

        break;

    case TEST:
        itest(RN(next[0]), SYM(next[1]), RN(next[2]), fetch(registers::TEST));
        break;
    case HALT:
        throw std::runtime_error("Received HALT");
        break;
    case HOST:
        ///get name of area
        throw std::runtime_error("Unimplemented HOST");
        break;
    case NOOP:
        break;
    case NOTE:
        pc++;
        step();
        return;
        break;
    case RAND:
        CALL3(irandi, RN, RN, R);
        break;
    case AT_REP:
        throw std::runtime_error("Should never be executed [rep]");
    case AT_END:
        throw std::runtime_error("Should never be executed [end]");
    case AT_N_M:
        throw std::runtime_error("Should never be executed [n_m]");
    case DATA:
        throw std::runtime_error("Unimpl");
    case COUNT:
        throw std::runtime_error("Unreachable?");
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

void cpu_state::add(const std::vector<std::string>& raw)
{
    instruction i1;
    i1.make(raw);

    inst.push_back(i1);
}

int cpu_state::label_to_pc(const std::string& label)
{
    for(int i=0; i < (int)inst.size(); i++)
    {
        const instruction& instr = inst[i];

        if(instr.type != instructions::MARK)
            continue;

        if(instr.args.size() == 0)
            continue;

        if(!instr.args[0].is_label())
            continue;

        if(instr.args[0].label == label)
            return i;
    }

    throw std::runtime_error("Attempted to jump to non existent label: " + label);
}

void cpu_tests()
{
    assert(instructions::rnames.size() == instructions::COUNT);

    {
        cpu_state test;

        test.add({"COPY", "5", "X"});
        test.add({"COPY", "7", "T"});
        test.add({"ADDI", "X", "T", "X"});

        test.step();
        test.step();
        test.step();

        test.debug_state();

        test.add({"COPY", "0", "X"});
        test.add({"COPY", "0", "T"});

        test.step();
        test.step();

        test.add({"MARK", "MY_LAB"});
        test.add({"ADDI", "X", "1", "X"});
        test.add({"JUMP", "MY_LAB"});

        test.step();
        test.step();
        test.step();
        test.step();

        test.debug_state();
    }

    {
        cpu_state test;

        test.add({"COPY", "53", "X"});
        test.add({"TEST", "54", ">", "X"});

        test.step();
        test.step();
        test.debug_state();
    }

    {
        cpu_state test;

        test.add({"COPY", "53", "X"});
        test.add({"TEST", "54", "=", "X"});

        test.step();
        test.step();
        test.debug_state();
    }

    {
        cpu_state test;
        test.add({"RAND", "0", "15", "X"});

        test.step();

        test.debug_state();
    }

    exit(0);
}
