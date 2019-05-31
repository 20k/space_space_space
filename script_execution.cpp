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


///need to parse 0xFF to 255

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
    DO_SERIALISE(free_running);
    DO_SERIALISE(last_error);
    DO_SERIALISE(ports);
    DO_SERIALISE(files);
    DO_SERIALISE(held_file);

    DO_RPC(inc_pc);
    DO_RPC(set_program);
    DO_RPC(reset);
}

void cpu_file::serialise(serialise_context& ctx, nlohmann::json& data, self_t* other)
{
    DO_SERIALISE(name);
    DO_SERIALISE(data);
    DO_SERIALISE(file_pointer);
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

register_value& register_value::decode(cpu_state& state)
{
    if(is_reg())
    {
        return state.fetch(reg);
    }

    if(is_address())
    {
        if(state.held_file == -1)
            throw std::runtime_error("No file held, caused by [address] " + as_string());

        cpu_file& fle = state.files[state.held_file];

        int check_address = address;

        if(which == 5)
        {
            register_value& nval = state.register_states[(int)reg_address];

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

void instruction::make(const std::vector<std::string>& raw)
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

void instruction::make(const std::string& str)
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

    update_length_register();

    files.push_back(get_master_virtual_file());
    update_master_virtual_file();
}

register_value& restrict_r(register_value& in)
{
    if(!in.is_reg() && !in.is_address())
        throw std::runtime_error("Expected register or address, got " + in.as_string());

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
    if(!in.is_reg() && !in.is_address() && !in.is_int())
        throw std::runtime_error("Expected register, address, or integer, got " + in.as_string());

    return in;
}

register_value& restrict_rns(register_value& in)
{
    if(!in.is_reg() && !in.is_address() && !in.is_int() && !in.is_symbol())
        throw std::runtime_error("Expected register, address, integer, or symbol, got " + in.as_string());

    return in;
}

register_value& restrict_rnls(register_value& in)
{
    if(!in.is_reg() && !in.is_address() && !in.is_int() && !in.is_symbol() && !in.is_label())
        throw std::runtime_error("Expected register, address, integer, symbol, or label got " + in.as_string());

    return in;
}

register_value& restrict_rls(register_value& in)
{
    if(!in.is_reg() && !in.is_address() && !in.is_label() && !in.is_symbol())
        throw std::runtime_error("Expected register, address, label, or symbol, got " + in.as_string());

    return in;
}

register_value& restrict_rs(register_value& in)
{
    if(!in.is_reg() && !in.is_address() && !in.is_symbol())
        throw std::runtime_error("Expected register, address, or symbol, got " + in.as_string());

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

#define RA(x, y) restricta(x, #y)

#define R(x) restrict_r(x).decode(*this) ///register only
#define RN(x) RA(restrict_rn(x).decode(*this), N) ///register or number
#define RNS(x) RA(restrict_rns(x).decode(*this), NS) ///register or number or symbol
#define RNLS(x) RA(restrict_rnls(x).decode(*this), NLS) ///register or number or symbol
#define RLS(x) RA(restrict_rls(x).decode(*this), LS)
#define RS(x) RA(restrict_rs(x).decode(*this), S)
#define E(x) RA(RA(x, RLANS).decode(*this), RLANS) ///everything, except file pointer
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

void cpu_state::update_master_virtual_file()
{
    register_value val;
    val.set_label("FILES");

    for(auto& i : files)
    {
        if(i.name == val)
        {
            i = get_master_virtual_file();
            return;
        }
    }

    ///file not present
    files.push_back(get_master_virtual_file());
    update_master_virtual_file();
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
        if(st.held_file != -1)
        {
            if(st.files[st.held_file].ensure_eof())
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

    ///ensure that whatever happens, our file has an eof at the end
    eof_helper eof_help(*this);
    f_register_helper f_help(*this);

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
        if(next.num_args() == 1 && next[0].is_label())
        {
            if(held_file == -1)
                throw std::runtime_error("Not holding file [TEST EOF]");

            if(next[0].label == "EOF")
            {
                cpu_file& held = files[held_file];

                int fptr = held.file_pointer;

                fetch(registers::TEST).set_int(held.data[fptr].is_eof());
                break;
            }
            else
            {
                throw std::runtime_error("TEST expects TEST VAL OP VAL or TEST EOF");
            }
        }

        itest(RNLS(next[0]), SYM(next[1]), RNLS(next[2]), fetch(registers::TEST));
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
        if(held_file != -1)
            throw std::runtime_error("Already holding file [MAKE] " + files[held_file].name.as_string());

        if(next.num_args() == 0)
        {
            cpu_file fle;
            fle.name.set_int(get_next_persistent_id());

            files.push_back(fle);

            held_file = (int)files.size() - 1;
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

            held_file = (int)files.size() - 1;
            update_length_register();
        }
        else
        {
            throw std::runtime_error("MAKE takes 0 or 1 args");
        }
        break;
    case RNAM:
        if(held_file == -1)
            throw std::runtime_error("Not holding file [RNAM]");

        {

            register_value& name = E(next[0]);

            for(auto& i : files)
            {
                if(i.name == name)
                    throw std::runtime_error("Duplicate file name [RNAM] " + name.as_string());
            }

            files[held_file].name = name;

            ///incase we rename the master virtual file
            update_master_virtual_file();
        }

        break;

    case RSIZ:
        if(held_file == -1)
            throw std::runtime_error("Not holding file [RSIZ]");

        {
            cpu_file& cur = files[held_file];
            int next_size = NUM(RN(next[0])).value;

            if(next_size > 1024 * 1024)
                throw std::runtime_error("Files are limited to 1KiB of storage");

            if(next_size < 0)
                throw std::runtime_error("[RSIZ] argument must be >= 0");

            cur.set_size(next_size);

            update_length_register();
        }

        break;

    case GRAB:
    {
        if(held_file != -1)
            throw std::runtime_error("Already holding file " + files[held_file].name.as_string());

        register_value& to_grab = RNLS(next[0]);

        if(to_grab.is_label() && to_grab.label == "FILES")
        {
            update_master_virtual_file();
        }

        for(int kk=0; kk < (int)files.size(); kk++)
        {
            if(files[kk].name == to_grab)
            {
                held_file = kk;
                break;
            }
        }

        if(held_file == -1)
            throw std::runtime_error("No file [GRAB] " + to_grab.as_string());

        update_length_register();

        break;
    }
    case instructions::FILE:
        if(held_file == -1)
            throw std::runtime_error("Not holding file [FILE]");

        R(next[0]) = files[held_file].name;
        break;

    case SEEK:
        if(held_file == -1)
            throw std::runtime_error("Not holding file [SEEK]");

        {
            int off = RN(next[0]).value;

            files[held_file].file_pointer += off;
            files[held_file].file_pointer = clamp(files[held_file].file_pointer, 0, files[held_file].len());
        }

        break;


    case VOID_FUCK_WINAPI:
        if(held_file == -1)
            throw std::runtime_error("Not holding file [VOID]");

        if(files[held_file].file_pointer < (int)files[held_file].len())
        {
            files[held_file].data.erase(files[held_file].data.begin() + files[held_file].file_pointer);
        }

        break;

    case DROP:
        if(held_file == -1)
            throw std::runtime_error("Not holding file [DROP]");

        drop_file();
        update_length_register();
        break;

    case WIPE:
        if(held_file == -1)
            throw std::runtime_error("Not holding file [WIPE]");

        files.erase(files.begin() + held_file);
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
        throw std::runtime_error("Should never be executed [end]");
    case AT_N_M:
        throw std::runtime_error("Should never be executed [n_m]");
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

void cpu_state::reset()
{
    auto instr_backup = inst;

    *this = cpu_state();

    inst = instr_backup;
}

void cpu_state::reset_rpc()
{
    rpc("reset", *this);
}

void cpu_state::drop_file()
{
    if(held_file == -1)
        throw std::runtime_error("BAD DROP FILE SHOULD NOT HAPPEN IN HERE BUT ITS OK");

    files[held_file].file_pointer = 0;
    held_file = -1;
}

void cpu_state::update_length_register()
{
    int len = -1;

    if(held_file != -1)
    {
        len = files[held_file].len();
    }

    register_states[(int)registers::FILE_LENGTH].set_int(len);
}

void cpu_state::update_f_register()
{
    if(held_file == -1)
    {
        register_states[(int)registers::FILE].set_int(0);
    }
    else
    {
        if(files[held_file].file_pointer >= (int)files[held_file].data.size())
            register_states[(int)registers::FILE].set_eof();
        else
            register_states[(int)registers::FILE] = files[held_file].data[files[held_file].file_pointer];
    }

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
    if((int)type < 0 || (int)type >= (int)register_states.size())
        throw std::runtime_error("No such register " + std::to_string((int)type));

    if(type == registers::FILE)
    {
        if(held_file == -1)
            throw std::runtime_error("No file held");

        if(files[held_file].file_pointer >= (int)files[held_file].data.size())
            throw std::runtime_error("Invalid file pointer " + std::to_string(files[held_file].file_pointer));

        int& fp = files[held_file].file_pointer;

        register_value& next = files[held_file].data[fp];

        fp++;

        return next;
    }

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

std::optional<cpu_file*> cpu_state::get_create_capability_file(const std::string& filename)
{
    for(int i=0; i < (int)files.size(); i++)
    {
        if((files[i].name.is_symbol() && files[i].name.symbol == filename) || (files[i].name.is_label() && files[i].name.label == filename))
        {
            if(held_file == i)
                return std::nullopt;

            return &files[i];
        }
    }

    cpu_file fle;
    fle.name.set_label(filename);

    ///cannot invalidate held file integer
    files.push_back(fle);

    return &files.back();
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

        assert(test.register_states[(int)registers::TEST].value == 1);

        test.step(); //rsiz
        test.step(); //test

        assert(test.register_states[(int)registers::TEST].value == 0);

        test.step(); //copy
        test.step(); //test

        assert(test.register_states[(int)registers::TEST].value == 1);

        test.debug_state();
    }

    //exit(0);
}
