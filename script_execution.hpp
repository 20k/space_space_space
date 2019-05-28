#ifndef SCRIPT_EXECUTION_HPP_INCLUDED
#define SCRIPT_EXECUTION_HPP_INCLUDED

#include <string>
#include <vector>
#include <map>

namespace registers
{
    enum type
    {
        TEST,
        GENERAL_PURPOSE0,
        COUNT,
    };

    static inline std::vector<std::string> rnames
    {
        "T",
        "X0",
    };
}

struct cpu_state;

struct register_value
{
    registers::type reg = registers::COUNT;
    int value = 0;
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

    register_value& decode(cpu_state& state);
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
        KILL,
        LINK,
        HOST,
        MODE,
        VOID_FUCK_WINAPI,
        MAKE,
        GRAB,
        FILE,
        SEEK,
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
        "KILL",
        "LINK",
        "HOST",
        "MODE",
        "VOID",
        "MAKE",
        "GRAB",
        "FILE",
        "SEEK",
        "DROP",
        "WIPE",
        "NOOP",
        "NOTE",
        "RAND",
        "@REP",
        "@END",
        "@@@@",
        "DATA",
    };

    type fetch(const std::string& name);
}

struct instruction
{
    instructions::type type = instructions::COUNT;
    std::vector<register_value> args;

    void make(const std::vector<std::string>& raw);

    register_value& fetch(int idx);

    register_value& operator[](int idx)
    {
        return fetch(idx);
    }
};

struct cpu_state
{
    cpu_state();

    std::map<registers::type, register_value> register_states;
    std::vector<instruction> inst;
    int pc = 0; ///instruction to be executed next

    void step();

    void debug_state();

    register_value& fetch(registers::type type);

    void add(const std::vector<std::string>& raw);
    int label_to_pc(const std::string& label);
};

void cpu_tests();

#endif // SCRIPT_EXECUTION_HPP_INCLUDED
