#include "script_execution.hpp"
#include <assert.h>
#include <iostream>
#include <networking/serialisable.hpp>
#include <sstream>
#include <string>
#include "time.hpp"

void strip_whitespace(std::string& in)
{
    while(in.size() > 0 && isspace(in[0]))
        in.erase(in.begin());

    while(in.size() > 0 && isspace(in.back()))
        in.pop_back();
}

bool all_whitespace(const std::string& in)
{
    for(int i=0; i < (int)in.size(); i++)
    {
        if(!isspace(in[i]))
            return false;
    }

    return true;
}

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

void register_value::serialise(serialise_context& ctx, nlohmann::json& data, self_t* other)
{
    DO_SERIALISE(reg);
    DO_SERIALISE(value);
    DO_SERIALISE(symbol);
    DO_SERIALISE(label);
    DO_SERIALISE(address);
    DO_SERIALISE(reg_address);
    DO_SERIALISE(file_eof);
    DO_SERIALISE(which);
    DO_SERIALISE(help);
}

void instruction::serialise(serialise_context& ctx, nlohmann::json& data, self_t* other)
{
    DO_SERIALISE(type);
    DO_SERIALISE(args);
}

void cpu_stash::serialise(serialise_context& ctx, nlohmann::json& data, self_t* other)
{
    DO_SERIALISE(held_file);
    DO_SERIALISE(register_states);
    DO_SERIALISE(pc);
    DO_SERIALISE(called_with);
    DO_SERIALISE(my_argument_names);
}

spair::spair()
{

}

spair::spair(register_value f1, int s) : first(f1), second(s){}

void spair::serialise(serialise_context& ctx, nlohmann::json& data, self_t* other)
{
    DO_SERIALISE(first);
    DO_SERIALISE(second);
}

void cpu_xfer::serialise(serialise_context& ctx, nlohmann::json& data, self_t* other)
{
    DO_SERIALISE(from);
    DO_SERIALISE(to);
    DO_SERIALISE(fraction);
    DO_SERIALISE(is_fractiony);
    DO_SERIALISE(held_file);
}

void cpu_state::serialise(serialise_context& ctx, nlohmann::json& data, self_t* other)
{
    DO_SERIALISE(all_stash);
    DO_SERIALISE(files);
    DO_SERIALISE(inst);
    DO_SERIALISE(context);

    DO_SERIALISE(free_running);
    DO_SERIALISE(last_error);
    DO_SERIALISE(ports);
    DO_SERIALISE(xfers);
    DO_SERIALISE(blocking_status);
    DO_SERIALISE(waiting_for_hardware_feedback);
    DO_SERIALISE(saved_program);
    DO_SERIALISE(tx_pending);
    DO_SERIALISE(had_tx_pending);

    DO_RPC(inc_pc);
    DO_RPC(set_program);
    DO_RPC(reset);
}

void cpu_file::serialise(serialise_context& ctx, nlohmann::json& data, self_t* other)
{
    DO_SERIALISE(name);
    DO_SERIALISE(data);
    DO_SERIALISE(file_pointer);
    DO_SERIALISE(was_xferred);
    DO_SERIALISE(owner);
}

bool all_numeric(const std::string& str)
{
    try
    {
        std::stoi(str, nullptr, 0);
        return true;
    }
    catch(...)
    {
        return false;
    }
}

void register_value::make(const std::string& str)
{
    if(str.size() == 0)
        throw std::runtime_error("Bad register value, 0 length");

    if(all_numeric(str))
    {
        try
        {
            int val = std::stoi(str, nullptr, 0);

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
        else
        {
            for(int i=0; i < (int)registers::COUNT; i++)
            {
                if(str == registers::rnames[i])
                {
                    set_reg((registers::type)i);
                    return;
                }
            }

            if(isalpha(str[0]))
            {
                throw std::runtime_error("Bad register " + str);
            }
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

        if(str.front() == '[' && str.back() == ']')
        {
            std::string stripped = str;
            stripped.erase(stripped.begin());
            stripped.pop_back();

            for(int kk=0; kk < (int)stripped.size(); kk++)
            {
                if(isspace(stripped[kk]))
                {
                    stripped.erase(stripped.begin() + kk);
                    kk--;
                    continue;
                }
            }

            register_value fval;

            try
            {
                fval.make(stripped);
            }
            catch(std::runtime_error& err)
            {
                std::string what = err.what();

                what = "Error in address operator: " + what;

                throw std::runtime_error(what);
            }

            if(!fval.is_int() && !fval.is_reg())
                throw std::runtime_error("Element in address must be integer or register, got " + fval.as_string());

            if(fval.is_int())
                set_address(fval.value);

            if(fval.is_reg())
                set_reg_address(fval.reg);

            return;
        }

        if(str.front() == '\"' && str.back() != '\"')
            throw std::runtime_error("String not terminated");

        if(str.front() == '[' && str.back() != ']')
            throw std::runtime_error("Unterminated address, missing ]");

        set_label(str);
    }
}

register_value& instruction::fetch(int idx)
{
    if(idx < 0 || idx >= (int)args.size())
        throw std::runtime_error("Bad instruction idx " + std::to_string(idx));

    return args[idx];
}

cpu_file::cpu_file()
{
    ensure_eof();
}

int cpu_file::len()
{
    return (int)data.size() - 1;
}

int cpu_file::len_with_eof()
{
    return data.size();
}

bool cpu_file::ensure_eof()
{
    if((int)data.size() == 0 || !data.back().is_eof())
    {
        register_value fake;
        fake.set_eof();

        data.push_back(fake);

        if(file_pointer >= (int)data.size())
        {
            ///resets to EOF
            file_pointer = len();
        }

        return true;
    }

    if(file_pointer >= (int)data.size())
    {
        ///resets to EOF
        file_pointer = len();
    }

    return false;
}

void cpu_file::set_size(int next_size)
{
    if(next_size == len())
        return;

    ///pop eof
    data.pop_back();

    if(next_size > (int)data.size())
    {
        for(int kk=0; kk < (int)next_size; kk++)
        {
            register_value next;
            next.set_int(0);

            data.push_back(next);
        }
    }

    if(next_size < (int)data.size())
    {
        data.resize(next_size);
    }

    ///push eof
    ensure_eof();
}

void cpu_file::clear()
{
    set_size(0);
}

register_value& cpu_file::operator[](int idx)
{
    if(idx < 0)
        throw std::runtime_error("IDX < 0 in cpu file operator[]");

    if(idx >= len())
        set_size(idx + 1);

    return data[idx];
}

std::string register_value::as_string() const
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

    if(which == 4)
    {
        return "[" + std::to_string(address) + "]";
    }

    if(which == 5)
    {
        return "[" + registers::rnames[(int)reg_address] + "]";
    }

    if(is_eof())
    {
        return "EOF";
    }

    throw std::runtime_error("Bad register val?");
}

register_value& register_value::decode(cpu_state& state, cpu_stash& stash)
{
    if(is_reg())
    {
        return state.fetch(stash, reg);
    }

    if(is_address())
    {
        if(stash.held_file == -1)
            throw std::runtime_error("No file held, caused by [address] " + as_string());

        cpu_file& fle = state.files[stash.held_file];

        int check_address = address;

        if(which == 5)
        {
            register_value& nval = stash.register_states[(int)reg_address];

            if(!nval.is_int())
            {
                throw std::runtime_error("Expected register in address operator to be int, got " + nval.as_string());
            }

            check_address = nval.value;
        }

        if(check_address < 0)
            throw std::runtime_error("Address < 0 " + as_string());

        if(check_address >= (int)fle.data.size())
            throw std::runtime_error("Address out of bounds " + as_string() + " in file " + fle.name.as_string());

        return fle.data[check_address];
    }

    return *this;
}

int instruction::num_args()
{
    return args.size();
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

void instruction::make(const std::vector<std::string>& raw, cpu_state& cpu)
{
    //if(raw.size() == 0)
    //    throw std::runtime_error("Bad instr");

    if(raw.size() == 0)
    {
        type = instructions::NOTE;
        return;
    }

    bool all_space = true;

    for(auto& i : raw)
    {
        if(!all_whitespace(i))
        {
            all_space = false;
        }
    }

    if(all_space)
    {
        type = instructions::NOTE;
        return;
    }

    instructions::type found = instructions::fetch(raw[0]);

    if(found == instructions::COUNT)
    {
        for(custom_instruction& cinst : cpu.custom)
        {
            if(raw[0] == cinst.name)
            {
                type = instructions::CALL;

                for(int i=0; i < (int)raw.size(); i++)
                {
                    register_value val;
                    val.make(raw[i]);

                    args.push_back(val);
                }

                return;
            }
        }

        throw std::runtime_error("Bad instruction " + raw[0]);
    }

    type = found;

    for(int i=1; i < (int)raw.size(); i++)
    {
        register_value val;
        val.make(raw[i]);

        args.push_back(val);
    }

    if(type == instructions::AT_DEF)
    {
        if(args.size() == 0)
            throw std::runtime_error("No args, use @DEF <NAME> <ARGS...>");

        if(!args[0].is_label())
            throw std::runtime_error("Invalid name, use @DEF NAME from " + args[0].as_string());

        custom_instruction cust;
        cust.name = args[0].label;

        for(int i=1; i < (int)args.size(); i++)
        {
            if(!args[i].is_label())
                throw std::runtime_error("@DEF args must be literal names, eg @DEF FUNC A0, got " + args[i].as_string());

            cust.args.push_back(args[i].label);
        }

        cpu.custom.push_back(cust);
    }
}

std::string get_next(const std::string& in, int& offset, bool& hit_comment)
{
    if(offset >= (int)in.size())
        return "";

    std::string ret;
    bool in_string = false;
    bool in_addr = false;

    bool was_addr = false;

    for(; offset < (int)in.size(); offset++)
    {
        char next = in[offset];

        if(next == '\"')
        {
            in_string = !in_string;
        }

        if(next == '[')
        {
            in_addr = true;
            was_addr = true;
        }

        if(next == ']')
        {
            in_addr = false;
        }

        if(next == ';' && !in_string && !in_addr)
        {
            hit_comment = true;
            offset++;
            return ret;
        }

        if(next == ' ' && !in_string && !in_addr)
        {
            offset++;
            return ret;
        }

        ret += next;
    }

    if(was_addr)
    {
        strip_whitespace(ret);
    }

    return ret;
}

void instruction::make(const std::string& str, cpu_state& cpu)
{
    int offset = 0;

    std::vector<std::string> vc;

    while(1)
    {
        bool hit_comment = false;

        auto it = get_next(str, offset, hit_comment);

        if(it == "")
            break;

        vc.push_back(it);

        if(hit_comment)
            break;
    }

    make(vc, cpu);
}

cpu_stash::cpu_stash()
{
    register_states.resize(registers::COUNT);

    for(int i=0; i < registers::COUNT; i++)
    {
        register_value val;
        val.set_int(0);

        register_states[i] = val;
    }
}

cpu_state::cpu_state()
{
    ports.resize(hardware::COUNT);
    blocking_status.resize(hardware::COUNT);

    for(int i=0; i < (int)hardware::COUNT; i++)
    {
        register_value val;
        val.set_int(-1);

        ports[i] = val;
    }

    update_length_register();

    files.push_back(get_master_virtual_file());
    update_master_virtual_file();
}

struct register_bundle
{
    cpu_stash& stash;
    register_value& in;

    register_bundle(cpu_stash& st, register_value& r) : stash(st), in(r){}

    register_value& decode(cpu_state& st)
    {
        return in.decode(st, stash);
    }
};

register_value& restricta(register_value& in, const std::string& types)
{
    std::map<char, std::string> rs
    {
    {'R', "register"},
    {'L', "label"},
    {'A', "address"},
    {'N', "integer"},
    {'S', "symbol"},
    {'Z', "EOF"},
    };

    bool should_throw = false;

    if(in.is_reg() && types.find('R') == std::string::npos)
        should_throw = true;
    if(in.is_label() && types.find('L') == std::string::npos)
        should_throw = true;
    if(in.is_address() && types.find('A') == std::string::npos)
        should_throw = true;
    if(in.is_int() && types.find('N') == std::string::npos)
        should_throw = true;
    if(in.is_symbol() && types.find('S') == std::string::npos)
        should_throw = true;
    if(in.is_eof() && types.find('Z') == std::string::npos)
        should_throw = true;

    if(should_throw)
    {
        std::string err = "Expected one of: ";

        for(auto i : types)
        {
            err += rs[i] + ", ";
        }

        err += "got " + in.as_string();

        throw std::runtime_error(err);
    }

    return in;
}

register_bundle restricta(const register_bundle& bun, const std::string& types)
{
    register_value& val = restricta(bun.in, types);

    return register_bundle(bun.stash, val);
}

///problem is we're not early decoding
///really need to pass through variable but need to bundle stash with it
///then decode right at the end
register_bundle check_environ(cpu_state& st, cpu_stash& stash, register_value& in)
{
    if(!in.is_label())
        return register_bundle(stash, in);

    if(stash.my_argument_names.size() != stash.called_with.size())
        throw std::runtime_error("Logic error in CPU arg name stuff [developer's fault]");

    for(int i=0; i < (int)stash.my_argument_names.size(); i++)
    {
        if(stash.my_argument_names[i] == in.label)
        {
            register_value& their_value = stash.called_with[i].first;
            int stk = stash.called_with[i].second;

            if(stk < 0 || stk >= (int)st.all_stash.size())
                throw std::runtime_error("STK OUT OF BOUNDS, STACK OVERFLOW " + std::to_string(stk));

            cpu_stash& their_stash = st.all_stash[stk];

            return check_environ(st, their_stash, their_value);
        }
    }

    return register_bundle(stash, in);
}

#define RA(x, y) restricta(x, #y)
#define CHECK(x) check_environ(*this, context, x)
#define DEC(x) x.decode(*this)

#define R(x) DEC(RA(CHECK(x), RA))
#define RN(x) RA(DEC(RA(CHECK(x), RN)), N)
#define RNS(x) RA(DEC(RA(CHECK(x), RNS)), NS)
#define RNLS(x) RA(DEC(RA(CHECK(x), RNLS)), NLS)
#define RLS(x) RA(DEC(RA(CHECK(x), RLS)), LS)
#define RS(x) RA(DEC(RA(CHECK(x), RS)), S)
#define E(x) DEC(RA(CHECK(x), RLANS))
#define L(x) DEC(RA(CHECK(x), L))

#define SYM(x) RA(x, S)
#define NUM(x) RA(x, N)

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

    int npc = s.context.pc % (int)s.inst.size();

    if(npc < 0 || npc >= (int)s.inst.size())
        throw std::runtime_error("Bad pc somehow");

    instruction& next = s.inst[npc];

    if(next.type == instructions::COUNT)
        throw std::runtime_error("Bad instruction at runtime?");

    return next.type == instructions::NOTE || next.type == instructions::MARK;
}

cpu_file cpu_state::get_master_virtual_file()
{
    cpu_file fle;

    fle.data.clear();

    for(int i=0; i < (int)files.size(); i++)
    {
        register_value fname = files[i].name;

        fle.data.push_back(fname);
    }

    fle.name.set_label("FILES");

    fle.ensure_eof();

    return fle;
}

bool cpu_state::update_master_virtual_file()
{
    register_value val;
    val.set_label("FILES");

    for(auto& i : files)
    {
        if(i.name == val)
        {
            i = get_master_virtual_file();
            return false;
        }
    }

    ///file not present
    files.push_back(get_master_virtual_file());
    update_master_virtual_file();

    return true;
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

struct eof_helper
{
    cpu_state& st;

    eof_helper(cpu_state& in) : st(in) {}

    ~eof_helper()
    {
        if(st.context.held_file != -1)
        {
            if(st.files[st.context.held_file].ensure_eof())
            {
                st.update_length_register();
            }
        }
    }
};

struct f_register_helper
{
    cpu_state& st;

    f_register_helper(cpu_state& in) : st(in) {}

    ~f_register_helper()
    {
        st.update_f_register();
    }
};

void cpu_state::step()
{
    if(waiting_for_hardware_feedback || tx_pending)
        return;

    ///shit we might double tx which makes dirty filing not work
    ///need to rename correctly
    /*if(had_tx_pending)
    {
        for(int i=0; i < (int)files.size(); i++)
        {
            if(files[i].was_xferred)
            {
                remove_file(i);
                i--;
                continue;
            }
        }

        had_tx_pending = false;
    }*/

    for(int i=0; i < (int)files.size(); i++)
    {
        if(files[i].owner != -1 && !files[i].was_updated_this_tick && !any_holds(i))
        {
            remove_file(i);
            i--;
            continue;
        }
    }

    for(int i=0; i < (int)files.size(); i++)
    {
        files[i].was_updated_this_tick = false;
    }

    if(inst.size() == 0)
        return;

    context.pc %= (int)inst.size();

    if(context.pc < 0 || context.pc >= (int)inst.size())
        throw std::runtime_error("Bad pc somehow");

    using namespace instructions;

    instruction& next = inst[context.pc];

    if(next.type == instructions::COUNT)
        throw std::runtime_error("Bad instruction at runtime?");

    ///so whenever we get a label
    ///need to run it through our list

    //std::cout << "NEXT " << instructions::rnames[(int)next.type] << std::endl;

    ///ensure that whatever happens, our file has an eof at the end
    eof_helper eof_help(*this);
    f_register_helper f_help(*this);

    switch(next.type)
    {
    case COPY:
        CALL2(icopy, E, R);
        break;
    case ADDI:
        CALL3(iaddi, RNLS, RNLS, R);
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

        CALL3(iswiz, RN, RN, R);
        //throw std::runtime_error("Unimplemented SWIZ");
        break;
    case JUMP:
        context.pc = label_to_pc(L(next[0]).label) + 1;
        return;
        break;
    case TJMP:
        if(fetch(context, registers::TEST).is_int() && fetch(context, registers::TEST).value != 0)
        {
            context.pc = label_to_pc(L(next[0]).label) + 1;
            return;
        }

        break;
    case FJMP:
        if(fetch(context, registers::TEST).is_int() && fetch(context, registers::TEST).value == 0)
        {
            context.pc = label_to_pc(L(next[0]).label) + 1;
            return;
        }

        break;

    case TEST:
        if(next.num_args() == 1 && next[0].is_label())
        {
            if(context.held_file == -1)
                throw std::runtime_error("Not holding file [TEST EOF]");

            if(next[0].label == "EOF")
            {
                cpu_file& held = files[context.held_file];

                int fptr = held.file_pointer;

                fetch(context, registers::TEST).set_int(held.data[fptr].is_eof());
                break;
            }
            else
            {
                throw std::runtime_error("TEST expects TEST VAL OP VAL or TEST EOF");
            }
        }

        itest(RNLS(next[0]), SYM(next[1]), RNLS(next[2]), fetch(context, registers::TEST));
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
    case MAKE:
        if(context.held_file != -1)
            throw std::runtime_error("Already holding file [MAKE] " + files[context.held_file].name.as_string());

        if(next.num_args() == 0)
        {
            cpu_file fle;
            fle.name.set_int(get_next_persistent_id());

            files.push_back(fle);

            context.held_file = (int)files.size() - 1;
            update_length_register();
        }
        else if(next.num_args() == 1)
        {
            register_value& name = RNLS(next[0]);

            for(auto& i : files)
            {
                if(i.name == name)
                    throw std::runtime_error("Duplicate file name [MAKE] " + name.as_string());
            }

            cpu_file fle;
            fle.name = name;

            files.push_back(fle);

            context.held_file = (int)files.size() - 1;
            update_length_register();
        }
        else
        {
            throw std::runtime_error("MAKE takes 0 or 1 args");
        }
        break;
    case RNAM:
        if(context.held_file == -1)
            throw std::runtime_error("Not holding file [RNAM]");

        {

            register_value& name = E(next[0]);

            for(auto& i : files)
            {
                if(i.name == name)
                    throw std::runtime_error("Duplicate file name [RNAM] " + name.as_string());
            }

            files[context.held_file].name = name;

            ///incase we rename the master virtual file
            update_master_virtual_file();
        }

        break;

    case RSIZ:
        if(context.held_file == -1)
            throw std::runtime_error("Not holding file [RSIZ]");

        {
            cpu_file& cur = files[context.held_file];
            int next_size = NUM(RN(next[0])).value;

            if(next_size > 1024 * 1024)
                throw std::runtime_error("Files are limited to 1KiB of storage");

            if(next_size < 0)
                throw std::runtime_error("[RSIZ] argument must be >= 0");

            cur.set_size(next_size);

            update_length_register();
        }

        break;

    ///or maybe drop should just take a destination arg
    case TXFR:
        {
            if(context.held_file == -1)
                throw std::runtime_error("Not holding file [TXFR]");

            cpu_file& file = files[context.held_file];

            cpu_xfer xf;
            xf.from = file.name.as_string();
            xf.to = E(next[0]).as_string();
            xf.held_file = context.held_file;
            xfers.push_back(xf);
            ///so
            ///when xferring stuff around we can guarantee no duplicates because the names are uniquely generated
            ///unless someone's put something there already, in which case it should get trampled? ignore that for the moment
            ///can make it illegal to name things in a certain way?
            ///ok so
            ///need to pretend that the file we're holding has gone somewhere else
            ///need to delete any files with that name already, or deliberately overwrite
            ///involes fixing all held files up, not too much of a problem
            ///although... if someone's holding it that's a problem
            ///so maybe naming scheme is way to go

            ///stalls
            tx_pending = true;
            had_tx_pending = true;
        }

        break;

    case GRAB:
    {
        if(context.held_file != -1)
            throw std::runtime_error("Already holding file " + files[context.held_file].name.as_string());

        register_value& to_grab = RNLS(next[0]);

        if(to_grab.is_label() && to_grab.label == "FILES")
        {
            update_master_virtual_file();
        }

        for(int kk=0; kk < (int)files.size(); kk++)
        {
            if(files[kk].name == to_grab)
            {
                for(int st = 0; st < (int)all_stash.size(); st++)
                {
                    if(all_stash[st].held_file == kk)
                        throw std::runtime_error("File already held by offset " + std::to_string(st) + " of " + std::to_string(all_stash.size()));
                }

                context.held_file = kk;
                break;
            }
        }

        if(context.held_file == -1)
            throw std::runtime_error("No file [GRAB] " + to_grab.as_string());

        update_length_register();

        break;
    }
    case instructions::FILE:
        if(context.held_file == -1)
            throw std::runtime_error("Not holding file [FILE]");

        R(next[0]) = files[context.held_file].name;
        break;

    case SEEK:
        if(context.held_file == -1)
            throw std::runtime_error("Not holding file [SEEK]");

        {
            int off = RN(next[0]).value;

            files[context.held_file].file_pointer += off;
            files[context.held_file].file_pointer = clamp(files[context.held_file].file_pointer, 0, files[context.held_file].len());
        }

        break;


    case VOID_FUCK_WINAPI:
        if(context.held_file == -1)
            throw std::runtime_error("Not holding file [VOID]");

        if(files[context.held_file].file_pointer < (int)files[context.held_file].len())
        {
            files[context.held_file].data.erase(files[context.held_file].data.begin() + files[context.held_file].file_pointer);
        }

        break;

    case DROP:
        if(context.held_file == -1)
            throw std::runtime_error("Not holding file [DROP]");

        drop_file();
        update_length_register();
        break;

    case WIPE:
        if(context.held_file == -1)
            throw std::runtime_error("Not holding file [WIPE]");

        files.erase(files.begin() + context.held_file);
        drop_file();
        update_length_register();
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
    case TIME:
        R(next[0]).set_int(get_time_ms());
        break;
    case AT_REP:
        throw std::runtime_error("Should never be executed [rep]");
    case AT_END:
        if(all_stash.size() == 0)
            throw std::runtime_error("Not in a function! @END");
        {
            context = all_stash.back();
            all_stash.pop_back();
            update_length_register();
        }
        break;
    case AT_N_M:
        throw std::runtime_error("Should never be executed [n_m]");
    case AT_DEF:
        //throw std::runtime_error("Should never be execited [@def]");
        {
            int defs = 1;

            context.pc++;

            for(; context.pc < (int)inst.size(); context.pc++)
            {
                if(inst[context.pc].type == instructions::AT_DEF)
                    defs++;

                if(inst[context.pc].type == instructions::AT_END)
                {
                    defs--;

                    if(defs == 0)
                        break;
                }
            }

            if(defs != 0)
                throw std::runtime_error("Unterminated @DEF, needs @END");
        }

        break;
    case CALL:
        {
            std::string name = L(next[0]).label;

            bool found = false;

            std::vector<std::string> args;

            for(custom_instruction& cst : custom)
            {
                if(cst.name == name)
                {
                    int my_args = next.num_args();

                    if(my_args - 1 != (int)cst.args.size())
                    {
                        throw std::runtime_error("Instruction " + name + " expects " + std::to_string(cst.args.size()) + " args, got " + std::to_string(my_args - 1));
                    }
                    else
                    {
                        args = cst.args;

                        found = true;
                    }
                }
            }

            if(!found)
                throw std::runtime_error("No such instruction to call, [" + name + "]");

            all_stash.push_back(context);

            cpu_stash next_stash;

            for(int i=1; i < next.num_args(); i++)
            {
                next_stash.called_with.push_back({next[i], (int)all_stash.size() - 1});
            }

            next_stash.my_argument_names = args;

            int npc = get_custom_instr_pc(name) + 1;

            context = next_stash;
            context.pc = npc;

            update_length_register();
        }

        return;
        break;
    case DATA:
        throw std::runtime_error("Unimpl");
    case WARP:
        ports[(int)hardware::W_DRIVE] = RNLS(next[0]);
        blocking_status[(int)hardware::W_DRIVE] = 1;
        waiting_for_hardware_feedback = true;
        break;
    case SLIP:
        if(next.num_args() == 1)
        {
            ports[(int)hardware::S_DRIVE] = RNS(next[0]);
        }
        else if(next.num_args() == 2)
        {
            ports[(int)hardware::S_DRIVE].set_symbol(RLS(next[0]).as_string() + RNS(next[1]).as_string());
        }
        else
        {
            throw std::runtime_error("SLIP expects 1 or 2 args");
        }

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

    context.pc++;

    int num_skip = 0;

    while(should_skip(*this) && num_skip < (int)inst.size())
    {
        context.pc++;

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

void cpu_state::reset()
{
    auto program_backup = saved_program;

    *this = cpu_state();

    saved_program = program_backup;

    set_program(saved_program);
}

void cpu_state::reset_rpc()
{
    rpc("reset", *this);
}

void cpu_state::drop_file()
{
    if(context.held_file == -1)
        throw std::runtime_error("BAD DROP FILE SHOULD NOT HAPPEN IN HERE BUT ITS OK");

    files[context.held_file].file_pointer = 0;
    context.held_file = -1;
}

void cpu_state::update_length_register()
{
    int len = -1;

    if(context.held_file != -1)
    {
        len = files[context.held_file].len();
    }

    context.register_states[(int)registers::FILE_LENGTH].set_int(len);
}

void cpu_state::update_f_register()
{
    if(context.held_file == -1)
    {
        context.register_states[(int)registers::FILE].set_int(0);
    }
    else
    {
        if(files[context.held_file].file_pointer >= (int)files[context.held_file].data.size())
            context.register_states[(int)registers::FILE].set_eof();
        else
            context.register_states[(int)registers::FILE] = files[context.held_file].data[files[context.held_file].file_pointer];
    }

}

void cpu_state::debug_state()
{
    printf("PC %i\n", context.pc);

    //for(auto& i : register_states)
    for(int i=0; i < (int)context.register_states.size(); i++)
    {
        std::string name = registers::rnames[i];
        std::string val = context.register_states[i].as_string();

        printf("%s: %s\n", name.c_str(), val.c_str());
    }

    for(int i=0; i < (int)hardware::COUNT; i++)
    {
        std::cout << "Port: " + std::to_string(i) + ": " + ports[i].as_string() << std::endl;
    }
}

register_value& cpu_state::fetch(cpu_stash& stash, registers::type type)
{
    if((int)type < 0 || (int)type >= (int)stash.register_states.size())
        throw std::runtime_error("No such register " + std::to_string((int)type));

    if(type == registers::FILE)
    {
        if(stash.held_file == -1)
            throw std::runtime_error("No file held");

        if(files[stash.held_file].file_pointer >= (int)files[stash.held_file].data.size())
            throw std::runtime_error("Invalid file pointer " + std::to_string(files[stash.held_file].file_pointer));

        int& fp = files[stash.held_file].file_pointer;

        register_value& next = files[stash.held_file].data[fp];

        fp++;

        return next;
    }

    return stash.register_states[(int)type];
}

void cpu_state::add(const std::vector<std::string>& raw)
{
    instruction i1;
    i1.make(raw, *this);

    inst.push_back(i1);
}

void cpu_state::add_line(const std::string& str)
{
    instruction i1;
    i1.make(str, *this);

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

int cpu_state::get_custom_instr_pc(const std::string& label)
{
    for(int i=0; i < (int)inst.size(); i++)
    {
        const instruction& instr = inst[i];

        if(instr.type != instructions::AT_DEF)
            continue;

        if(instr.args.size() == 0)
            continue;

        if(!instr.args[0].is_label())
            continue;

        if(instr.args[0].label == label)
            return i;
    }

    throw std::runtime_error("Attempted to jump to non existent custom instr: " + label);
}

void cpu_state::set_program(std::string str)
{
    try
    {
        *this = cpu_state();

        if(str.size() > 10000)
            return;

        saved_program = str;

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

std::optional<cpu_file*> cpu_state::get_create_capability_file(const std::string& filename, size_t owner, size_t owner_offset)
{
    for(int i=0; i < (int)files.size(); i++)
    {
        //if((files[i].name.is_symbol() && files[i].name.symbol == filename) || (files[i].name.is_label() && files[i].name.label == filename))
        if(files[i].owner == owner && files[i].owner_offset == owner_offset)
        {
            files[i].name.set_label(filename);
            files[i].was_updated_this_tick = true;

            if(context.held_file == i)
                return std::nullopt;

            return &files[i];
        }
    }

    cpu_file fle;
    fle.name.set_label(filename);
    fle.owner = owner;
    fle.owner_offset = owner_offset;

    ///cannot invalidate held file integer
    files.push_back(fle);

    return &files.back();
}

void cpu_state::remove_file(int held_id)
{
    if(held_id == -1)
        throw std::runtime_error("Held id -1 in remove_file");

    if(held_id >= (int)files.size())
        throw std::runtime_error("Should be impossible, held_id >= files.size()");

    if(context.held_file == held_id)
        throw std::runtime_error("Bad logic error, attempted to remove file in use");

    for(cpu_stash& sth : all_stash)
    {
        if(sth.held_file == held_id)
            throw std::runtime_error("Bad logic error, attempted to remove file in use in stack");
    }

    if(context.held_file > held_id)
    {
        context.held_file--;
    }

    for(cpu_stash& sth : all_stash)
    {
        if(sth.held_file > held_id)
            sth.held_file--;
    }

    files.erase(files.begin() + held_id);
}

bool cpu_state::any_holds(int held_id)
{
    if(context.held_file == held_id)
        return true;

    for(cpu_stash& sth : all_stash)
    {
        if(sth.held_file == held_id)
            return true;
    }

    return false;
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

    {
        cpu_state test;
        test.add_line("MAKE 1234");
        test.add_line("DROP");

        //test.step();
        //test.step();
    }

    {
        cpu_state test;

        /*MAKE 1234
        TEST EOF
        RSIZ 1
        TEST EOF
        COPY F X0
        TEST EOF
        WIPE*/

        printf("EOF TESTS\n");

        test.add_line("MAKE 1234");
        test.add_line("TEST EOF");
        test.add_line("RSIZ 1");
        test.add_line("TEST EOF");
        test.add_line("COPY F X0");
        test.add_line("TEST EOF");
        test.add_line("WIPE");

        test.step(); //make
        test.step(); //eof

        assert(test.context.register_states[(int)registers::TEST].value == 1);

        test.step(); //rsiz
        test.step(); //test

        assert(test.context.register_states[(int)registers::TEST].value == 0);

        test.step(); //copy
        test.step(); //test

        assert(test.context.register_states[(int)registers::TEST].value == 1);

        test.debug_state();
    }

    ///thanks https://www.reddit.com/r/exapunks/comments/96vox3/how_do_swiz/ reddit beause swiz is a bit
    ///confusing
    {
        cpu_state test;

        test.add_line("SWIZ 8567 3214 X");

        test.step();

        assert(test.context.register_states[(int)registers::GENERAL_PURPOSE0].value == 5678);

        test.add_line("SWIZ 1234 0003 X");
        test.step();

        assert(test.context.register_states[(int)registers::GENERAL_PURPOSE0].value == 2);

        test.add_line("SWIZ 5678 3 X");
        test.step();

        assert(test.context.register_states[(int)registers::GENERAL_PURPOSE0].value == 6);

        test.add_line("SWIZ 5678 32 X");
        test.step();

        assert(test.context.register_states[(int)registers::GENERAL_PURPOSE0].value == 67);


        test.add_line("SWIZ 5678 -32 X");
        test.step();

        assert(test.context.register_states[(int)registers::GENERAL_PURPOSE0].value == -67);
    }

    ///overflow
    {
        cpu_state test;
        test.add_line("ADDI " + std::to_string(std::numeric_limits<int>::max()) + " 1 X0");

        test.step();

        assert(test.context.register_states[(int)registers::GENERAL_PURPOSE0].value == std::numeric_limits<int>::max());
    }

    ///negative overflow
    {
        cpu_state test;
        test.add_line("ADDI " + std::to_string(std::numeric_limits<int>::lowest()) + " -1 X0");

        test.step();

        assert(test.context.register_states[(int)registers::GENERAL_PURPOSE0].value == std::numeric_limits<int>::lowest());
    }

    //exit(0);
}
