#ifndef SCRIPT_EXECUTION_HPP_INCLUDED
#define SCRIPT_EXECUTION_HPP_INCLUDED

#include <string>
#include <vector>
#include <map>
#include <array>
#include <networking/serialisable_fwd.hpp>

namespace registers
{
    enum type
    {
        TEST,
        GENERAL_PURPOSE0,
        FILE_LENGTH,
        COUNT,
    };

    static inline std::vector<std::string> rnames
    {
        "T",
        "X0",
        "L",
    };
}

struct cpu_state;

struct register_value : serialisable
{
    registers::type reg = registers::COUNT;
    int value = 0;
    int address = 0;
    registers::type reg_address = registers::COUNT;
    std::string symbol;
    std::string label;

    int which = -1;

    void make(const std::string& str);
    std::string as_string();

    bool is_reg() const
    {
        return which == 0;
    }

    bool is_int() const
    {
        return which == 1;
    }

    bool is_symbol() const
    {
        return which == 2;
    }

    bool is_label() const
    {
        return which == 3;
    }

    bool is_address() const
    {
        return which == 4 || which == 5;
    }

    void set_reg(registers::type type)
    {
        reg = type;
        which = 0;
    }

    void set_int(int v)
    {
        value = v;
        which = 1;
    }

    void set_symbol(const std::string& symb)
    {
        symbol = symb;
        which = 2;
    }

    void set_label(const std::string& lab)
    {
        label = lab;
        which = 3;
    }

    void set_address(int addr)
    {
        address = addr;
        which = 4;
    }

    void set_reg_address(registers::type type)
    {
        reg_address = type;
        which = 5;
    }

    bool operator==(const register_value& second)
    {
        if(which != second.which)
            return false;

        if(is_reg())
            return reg == second.reg;

        if(is_int())
            return value == second.value;

        if(is_symbol())
            return symbol == second.symbol;

        if(is_label())
            return label == second.label;

        if(which == 4)
            return address == second.address;

        if(which == 5)
            return reg_address == second.reg_address;

        return false;
    }

    register_value& decode(cpu_state& state);

    SERIALISE_SIGNATURE();
};

///so extensions to exapunks syntax
///[0xFF] or [reg] means memory address?
///basically: need some way to store data, need some way to poke puzzles and obscure hardware
namespace instructions
{
    enum type
    {
        COPY,
        ADDI,
        SUBI,
        MULI,
        DIVI,
        MODI,
        SWIZ,
        MARK,
        JUMP,
        TJMP,
        FJMP,
        TEST,
        //REPL,
        HALT,
        WAIT,
        //KILL,
        //LINK,
        HOST,
        //MODE,
        //VOID_FUCK_WINAPI,
        MAKE,
        RSIZ,
        GRAB,
        FILE,
        //SEEK,
        DROP,
        WIPE,
        NOOP,
        NOTE,
        RAND,
        AT_REP,
        AT_END,
        AT_N_M,
        //WAIT,
        DATA,
        WARP,
        SLIP,
        TRVL,
        COUNT,
    };

    static inline std::vector<std::string> rnames
    {
        "COPY",
        "ADDI",
        "SUBI",
        "MULI",
        "DIVI",
        "MODI",
        "SWIZ",
        "MARK",
        "JUMP",
        "TJMP",
        "FJMP",
        "TEST",
        "HALT",
        "WAIT",
        //"KILL",
        //"LINK",
        "HOST",
        //"MODE",
        //"VOID",
        "MAKE",
        "RSIZ",
        "GRAB",
        "FILE",
        //"SEEK",
        "DROP",
        "WIPE",
        "NOOP",
        "NOTE",
        "RAND",
        "@REP",
        "@END",
        "@@@@",
        "DATA",
        "WARP",
        "SLIP",
        "TRVL",
    };

    type fetch(const std::string& name);
}

namespace hardware
{
    enum type
    {
        W_DRIVE, ///warp drive
        S_DRIVE, ///fsd space
        T_DRIVE, ///thrusters
        COUNT,
    };
}

struct instruction : serialisable
{
    instructions::type type = instructions::COUNT;
    std::vector<register_value> args;

    void make(const std::vector<std::string>& raw);
    void make(const std::string& str);

    int num_args();
    register_value& fetch(int idx);

    register_value& operator[](int idx)
    {
        return fetch(idx);
    }

    SERIALISE_SIGNATURE();
};

struct cpu_file : serialisable
{
    register_value name;
    std::vector<register_value> data;

    SERIALISE_SIGNATURE();
};

struct cpu_state : serialisable, owned
{
    cpu_state();

    std::vector<cpu_file> files;
    int held_file = -1;

    std::vector<register_value> register_states;
    std::vector<instruction> inst;
    int pc = 0; ///instruction to be executed next
    bool free_running = false;
    std::string last_error;

    std::vector<register_value> ports;
    std::vector<int> blocking_status;
    bool waiting_for_hardware_feedback = false;

    bool any_blocked();

    void step();

    void debug_state();

    register_value& fetch(registers::type type);

    void add(const std::vector<std::string>& raw);
    void add_line(const std::string& str);
    int label_to_pc(const std::string& label);

    SERIALISE_SIGNATURE();

    void set_program(std::string str);

    void inc_pc();
    void inc_pc_rpc();

    void upload_program_rpc(std::string str);

    void update_length_register();

    std::optional<cpu_file*> get_create_capability_file(const std::string& filename);
};

void cpu_tests();

#endif // SCRIPT_EXECUTION_HPP_INCLUDED
