#include "script_execution.hpp"
#include <assert.h>
#include <iostream>
#include <networking/serialisable.hpp>
#include <sstream>

inline
std::vector<std::string>& split(const std::string &s, char delim, std::vector<std::string> &elems)
{
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

inline
std::vector<std::string> split(const std::string &s, char delim)
{
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}


///need to parse 0xFF to 255

void register_value::serialise(serialise_context& ctx, nlohmann::json& data, self_t* other)
{
    DO_SERIALISE(reg);
    DO_SERIALISE(value);
    DO_SERIALISE(symbol);
    DO_SERIALISE(label);
    DO_SERIALISE(which);
}

void instruction::serialise(serialise_context& ctx, nlohmann::json& data, self_t* other)
{
    DO_SERIALISE(type);
    DO_SERIALISE(args);
}

void cpu_state::serialise(serialise_context& ctx, nlohmann::json& data, self_t* other)
{
    DO_SERIALISE(register_states);
    DO_SERIALISE(inst);
    DO_SERIALISE(pc);
    DO_SERIALISE(ports);
    DO_SERIALISE(last_error);

    DO_RPC(inc_pc);
    DO_RPC(set_program);
}

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

std::string get_next(const std::string& in, int& offset)
{
    if(offset >= (int)in.size())
        return "";

    std::string ret;
    bool in_string = false;

    for(; offset < (int)in.size(); offset++)
    {
        char next = in[offset];

        if(next == '\"')
        {
            in_string = !in_string;
        }

        if(next == ' ' && !in_string)
        {
            offset++;
            return ret;
        }

        ret += next;
    }

    return ret;
}

void instruction::make(const std::string& str)
{
    int offset = 0;

    std::vector<std::string> vc;

    while(1)
    {
        auto it = get_next(str, offset);

        if(it == "")
            break;

        vc.push_back(it);
    }

    make(vc);
}

cpu_state::cpu_state()
{
    ports.resize(hardware::COUNT);
    blocking_status.resize(hardware::COUNT);
    register_states.resize(registers::COUNT);

    for(int i=0; i < registers::COUNT; i++)
    {
        register_value val;
        val.set_int(0);

        register_states[i] = val;
    }

    for(int i=0; i < (int)hardware::COUNT; i++)
    {
        register_value val;
        val.set_int(-1);

        ports[i] = val;
    }
}

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

register_value& restrict_rns(register_value& in)
{
    if(!in.is_reg() && !in.is_int() && !in.is_symbol())
        throw std::runtime_error("Expected register, integer or symbol, got " + in.as_string());

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
#define RNS(x) restrict_rns(x).decode(*this) ///register or number or symbol
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

bool should_skip(cpu_state& s)
{
    if(s.inst.size() == 0)
        return false;

    int npc = s.pc % (int)s.inst.size();

    if(npc < 0 || npc >= (int)s.inst.size())
        throw std::runtime_error("Bad pc somehow");

    instruction& next = s.inst[npc];

    if(next.type == instructions::COUNT)
        throw std::runtime_error("Bad instruction at runtime?");

    return next.type == instructions::NOTE || next.type == instructions::MARK;
}

bool cpu_state::any_blocked()
{
    for(int i=0; i < (int)blocking_status.size(); i++)
    {
        if(blocking_status[i])
            return true;
    }

    return false;
}

void cpu_state::step()
{
    if(waiting_for_hardware_feedback)
        return;

    if(inst.size() == 0)
        return;

    pc %= (int)inst.size();

    if(pc < 0 || pc >= (int)inst.size())
        throw std::runtime_error("Bad pc somehow");

    using namespace instructions;

    instruction& next = inst[pc];

    if(next.type == instructions::COUNT)
        throw std::runtime_error("Bad instruction at runtime?");

    //std::cout << "NEXT " << instructions::rnames[(int)next.type] << std::endl;

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
        //pc++;
        //step();
        //return;
        break;
    case SWIZ:
        throw std::runtime_error("Unimplemented SWIZ");
    case JUMP:
        pc = label_to_pc(L(next[0]).label) + 1;
        return;
        break;
    case TJMP:
        if(fetch(registers::TEST).is_int() && fetch(registers::TEST).value != 0)
        {
            pc = label_to_pc(L(next[0]).label) + 1;
            return;
        }

        break;
    case FJMP:
        if(fetch(registers::TEST).is_int() && fetch(registers::TEST).value == 0)
        {
            pc = label_to_pc(L(next[0]).label) + 1;
            return;
        }

        break;

    case TEST:
        itest(RN(next[0]), SYM(next[1]), RN(next[2]), fetch(registers::TEST));
        break;
    case HALT:
        throw std::runtime_error("Received HALT");
        break;
    case WAIT:
            if(any_blocked())
                return;
            break;
    case HOST:
        ///get name of area
        throw std::runtime_error("Unimplemented HOST");
        break;
    case NOOP:
        break;
    case NOTE:
        //pc++;
        //step();
        //return;
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
    case WARP:
        ports[(int)hardware::W_DRIVE] = RNS(next[0]);
        blocking_status[(int)hardware::W_DRIVE] = 1;
        waiting_for_hardware_feedback = true;
        break;
    case SLIP:
        ports[(int)hardware::S_DRIVE] = RNS(next[0]);
        blocking_status[(int)hardware::S_DRIVE] = 1;
        waiting_for_hardware_feedback = true;
        break;
    case TRVL:
        ports[(int)hardware::T_DRIVE] = RNS(next[0]);
        blocking_status[(int)hardware::T_DRIVE] = 1;
        waiting_for_hardware_feedback = true;
        break;
    case COUNT:
        throw std::runtime_error("Unreachable?");
    }

    pc++;

    int num_skip = 0;

    while(should_skip(*this) && num_skip < (int)inst.size())
    {
        pc++;

        num_skip++;
    }
}

void cpu_state::inc_pc()
{
    free_running = false;

    try
    {
        step();
    }
    catch(std::runtime_error& err)
    {
        last_error = err.what();
        free_running = false;
    }
}

void cpu_state::inc_pc_rpc()
{
    rpc("inc_pc", *this);
}

void cpu_state::upload_program_rpc(std::string str)
{
    rpc("set_program", *this, str);
}

void cpu_state::debug_state()
{
    printf("PC %i\n", pc);

    //for(auto& i : register_states)
    for(int i=0; i < (int)register_states.size(); i++)
    {
        std::string name = registers::rnames[i];
        std::string val = register_states[i].as_string();

        printf("%s: %s\n", name.c_str(), val.c_str());
    }

    for(int i=0; i < (int)hardware::COUNT; i++)
    {
        std::cout << "Port: " + std::to_string(i) + ": " + ports[i].as_string() << std::endl;
    }
}

register_value& cpu_state::fetch(registers::type type)
{
    if((int)type < 0 || (int)type >= register_states.size())
        throw std::runtime_error("No such register " + std::to_string((int)type));

    return register_states[(int)type];
}

void cpu_state::add(const std::vector<std::string>& raw)
{
    instruction i1;
    i1.make(raw);

    inst.push_back(i1);
}

void cpu_state::add_line(const std::string& str)
{
    instruction i1;
    i1.make(str);

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

void cpu_state::set_program(std::string str)
{
    try
    {
        if(str.size() > 10000)
            return;

        inst.clear();

        auto all = split(str, '\n');

        for(const auto& i : all)
        {
            add_line(i);
        }
    }
    catch(std::runtime_error& err)
    {
        last_error = err.what();
        free_running = false;
    }
}

void cpu_tests()
{
    assert(instructions::rnames.size() == instructions::COUNT);

    {
        cpu_state test;

        test.add_line("COPY 5 X");
        test.add_line("COPY 7 T");
        test.add_line("ADDI X T X");

        test.step();
        test.step();
        test.step();

        test.debug_state();

        test.add_line("COPY 0 X");
        test.add_line("COPY 0 T");

        test.step();
        test.step();

        test.add_line("MARK MY_LAB");
        test.add_line("ADDI X 1 X");
        test.add_line("JUMP MY_LAB");

        test.step();
        test.step();
        test.step();
        test.step();

        test.debug_state();
    }

    {
        cpu_state test;

        test.add_line("COPY 53 X");
        test.add_line("TEST 54 > X");

        test.step();
        test.step();
        test.debug_state();
    }

    {
        cpu_state test;

        test.add_line("COPY 53 X");
        test.add_line("TEST 54 = X");

        test.step();
        test.step();
        test.debug_state();
    }

    {
        cpu_state test;
        //test.add({"RAND", "0", "15", "X"});
        test.add_line("RAND 0 15 X");

        test.step();

        test.debug_state();
    }

    {
        cpu_state test;
        test.add_line("WARP 123");

        test.step();

        assert(test.ports[(int)hardware::W_DRIVE].value == 123);

        test.debug_state();
    }

    //exit(0);
}
