#ifndef PREBAKED_PROGRAMS_HPP_INCLUDED
#define PREBAKED_PROGRAMS_HPP_INCLUDED

#include <string>
#include <vector>
#include <optional>

struct prebaked_program
{
    std::string name;
    std::string program;
};

struct prebaked_program_manager
{
    std::vector<prebaked_program> programs;

    prebaked_program_manager();
};

std::optional<prebaked_program> fetch_program(const std::string& name);

#endif // PREBAKED_PROGRAMS_HPP_INCLUDED
