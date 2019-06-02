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
        FILE,
        COUNT,
    };

    static inline std::vector<std::string> rnames
    {
        "T",
        "X0",
        "L",
        "F",
    };
}

struct cpu_state;
struct cpu_stash;

struct register_value : serialisable
{
    registers::type reg = registers::COUNT;
    int value = 0;
    int address = 0;
    registers::type reg_address = registers::COUNT;
    std::string symbol;
    std::string label;

    std::string help;

    int which = -1;

    bool file_eof = false;

    void make(const std::string& str);
    std::string as_string() const;

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

    bool is_eof() const
    {
        return which == 6;
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

    void set_eof()
    {
        file_eof = true;
        which = 6;
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

        if(which == 6)
            return file_eof == second.file_eof;

        return false;
    }

    register_value& decode(cpu_state& state, cpu_stash& stash);

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
        VOID_FUCK_WINAPI,
        MAKE,
        RNAM,
        RSIZ,
        TXFR,
        GRAB,
        FILE,
        SEEK,
        DROP,
        WIPE,
        NOOP,
        NOTE,
        RAND,
        TIME,
        AT_REP,
        AT_END,
        AT_N_M,
        AT_DEF,
        CALL,
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
        "VOID",
        "MAKE",
        "RNAM",
        "RSIZ",
        "TXFR",
        "GRAB",
        "FILE",
        "SEEK",
        "DROP",
        "WIPE",
        "NOOP",
        "NOTE",
        "RAND",
        "TIME",
        "@REP",
        "@END",
        "@@@@",
        "@DEF",
        "CALL", ///implementatin detail but someone could use it if they want
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

    void make(const std::vector<std::string>& raw, cpu_state& cpu);
    void make(const std::string& str, cpu_state& cpu);

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
    int file_pointer = 0;
    bool was_xferred = false;
    size_t owner = -1;
    size_t owner_offset = -1;

    cpu_file();

    int len();
    int len_with_eof();
    bool ensure_eof(); ///true on eof update

    void set_size(int next);

    void clear();
    register_value& operator[](int idx);

    SERIALISE_SIGNATURE();
};

struct spair : serialisable
{
    register_value first;
    int second = 0;

    spair();
    spair(register_value f1, int s);

    SERIALISE_SIGNATURE();
};

struct cpu_xfer : serialisable
{
    std::string from, to;
    float fraction = 1;
    bool is_fractiony = false;
    int held_file = -1;

    SERIALISE_SIGNATURE();
};

struct cpu_stash : serialisable
{
    int held_file = -1;

    std::vector<register_value> register_states;
    int pc = 0;

    ///int is which cpu stash they are
    std::vector<spair> called_with;
    std::vector<std::string> my_argument_names;

    cpu_stash();

    SERIALISE_SIGNATURE();
};

struct custom_instruction
{
    std::string name;
    std::vector<std::string> args;
};

struct cpu_state : serialisable, owned
{
    cpu_state();

    std::vector<cpu_stash> all_stash;
    std::vector<cpu_file> files;
    std::vector<custom_instruction> custom;

    std::vector<instruction> inst;

    cpu_stash context;

    bool free_running = false;
    std::string last_error;

    std::vector<register_value> ports;
    std::vector<cpu_xfer> xfers;
    std::vector<int> blocking_status;
    bool waiting_for_hardware_feedback = false;
    bool tx_pending = false;
    bool had_tx_pending = false;

    std::string saved_program;

    cpu_file get_master_virtual_file();
    bool update_master_virtual_file();

    bool any_blocked();

    void step();

    void debug_state();

    register_value& fetch(cpu_stash& stash, registers::type type);

    void add(const std::vector<std::string>& raw);
    void add_line(const std::string& str);
    int label_to_pc(const std::string& label);
    int get_custom_instr_pc(const std::string& name);

    SERIALISE_SIGNATURE();

    void set_program(std::string str);

    void inc_pc();
    void inc_pc_rpc();

    void reset();
    void reset_rpc();

    void drop_file();

    void upload_program_rpc(std::string str);

    void update_length_register();
    void update_f_register();

    std::optional<cpu_file*> get_create_capability_file(const std::string& filename, size_t owner, size_t owner_offset);

    void remove_file(int idx);
};

void cpu_tests();

#endif // SCRIPT_EXECUTION_HPP_INCLUDED
