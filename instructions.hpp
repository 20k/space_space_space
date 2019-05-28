#ifndef INSTRUCTIONS_HPP_INCLUDED
#define INSTRUCTIONS_HPP_INCLUDED

#include <random>
#include "random.hpp"

void icopy(register_value& one, register_value& two)
{
    two = one;
}

void iaddi(register_value& r1, register_value& r2, register_value& r3)
{
    NUM(r1);
    NUM(r2);

    r3.set_int(r1.value + r2.value);
}

void isubi(register_value& r1, register_value& r2, register_value& r3)
{
    NUM(r1);
    NUM(r2);

    r3.set_int(r1.value - r2.value);
}

void imuli(register_value& r1, register_value& r2, register_value& r3)
{
    NUM(r1);
    NUM(r2);

    r3.set_int(r1.value * r2.value);
}

void idivi(register_value& r1, register_value& r2, register_value& r3)
{
    NUM(r1);
    NUM(r2);

    if(r2.value == 0)
        throw std::runtime_error("Tried to DIVI by 0");

    r3.set_int(r1.value / r2.value);
}

void imodi(register_value& r1, register_value& r2, register_value& r3)
{
    NUM(r1);
    NUM(r2);

    if(r2.value == 0)
        throw std::runtime_error("Tried to MODI by 0");

    r3.set_int(r1.value % r2.value);
}

void itest(register_value& r1, register_value& msym, register_value& r2, register_value& t)
{
    if(r1.which != r2.which)
    {
        t.set_int(0);
        return;
    }

    int res = 0;

    if(msym.symbol == "=")
    {
        if(r1.is_int())
        {
            res = r1.value == r2.value;
        }

        if(r1.is_label())
        {
            res = r1.label == r2.label;
        }

        if(r1.is_symbol())
        {
            res = r1.symbol == r2.symbol;
        }
    }

    if(msym.symbol == "<")
    {
        if(r1.is_int())
        {
            res = r1.value < r2.value;
        }

        if(r1.is_label())
        {
            res = r1.label < r2.label;
        }

        if(r1.is_symbol())
        {
            res = r1.symbol < r2.symbol;
        }
    }

    if(msym.symbol == ">")
    {
        if(r1.is_int())
        {
            res = r1.value > r2.value;
        }

        if(r1.is_label())
        {
            res = r1.label > r2.label;
        }

        if(r1.is_symbol())
        {
            res = r1.symbol > r2.symbol;
        }
    }

    t.set_int(res);
}

void irandi(register_value& r1, register_value& r2, register_value& r3)
{
    NUM(r1);
    NUM(r2);

    int mx = r2.value;
    int mn = r1.value;

    auto rnd = random_variable();

    std::default_random_engine generator(rnd);
    std::uniform_int_distribution<int> distribution(mn,mx);

    int rval = distribution(generator);

    r3.set_int(rval);
}

#endif // INSTRUCTIONS_HPP_INCLUDED
