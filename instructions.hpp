#ifndef INSTRUCTIONS_HPP_INCLUDED
#define INSTRUCTIONS_HPP_INCLUDED

#include <random>
#include "random.hpp"
#include <limits>

int hex_to_dec(char hex)
{
    if(hex == '0')
        return 0;
    if(hex == '1')
        return 1;
    if(hex == '2')
        return 2;
    if(hex == '3')
        return 3;
    if(hex == '4')
        return 4;
    if(hex == '5')
        return 5;
    if(hex == '6')
        return 6;
    if(hex == '7')
        return 7;
    if(hex == '8')
        return 8;
    if(hex == '9')
        return 9;
    if(hex == 'A' || hex == 'a')
        return 10;
    if(hex == 'B' || hex == 'b')
        return 11;
    if(hex == 'C' || hex == 'c')
        return 12;
    if(hex == 'D' || hex == 'd')
        return 13;
    if(hex == 'E' || hex == 'e')
        return 14;
    if(hex == 'F' || hex == 'f')
        return 15;

    return 0;
}

int saturate(int64_t in)
{
    if(in > std::numeric_limits<int>::max())
        return std::numeric_limits<int>::max();

    if(in < std::numeric_limits<int>::lowest())
        return std::numeric_limits<int>::lowest();

    return (int)in;
}

template<typename T>
int saturating_op(int v1, int v2, T op)
{
    int64_t i1 = v1;
    int64_t i2 = v2;

    ///man i love c++
    int64_t i3 = op(i1, i2);

    return saturate(i3);
}

void icopy(register_value& one, register_value& two)
{
    two = one;
}

void iaddi(register_value& r1, register_value& r2, register_value& r3)
{
    /*NUM(r1);
    NUM(r2);

    r3.set_int(r1.value + r2.value);*/
        //r3.set_int(r1.value + r2.value);

    if(r1.is_label() && r2.is_symbol())
        r3.set_label(r1.label + r2.as_uniform_string());

    else if(r1.is_symbol() && r2.is_label())
        r3.set_label(r1.as_uniform_string() + r2.label);

    else if(r1.which != r2.which)
        throw std::runtime_error("ADDI arguments must be of same type, got " + r1.as_string() + " and " + r2.as_string());

    else if(r1.is_int())
        r3.set_int(saturating_op(r1.value, r2.value, std::plus<int64_t>{}));

    else if(r1.is_label())
        r3.set_label(r1.label + r2.label);

    else if(r1.is_symbol())
        r3.set_symbol(r1.symbol + r2.symbol);

    else
        throw std::runtime_error("Somehow non int/label/symbol snuck into ADDI, probably developer error but your fault too: " + r1.as_string() + " " + r2.as_string());
}

void isubi(register_value& r1, register_value& r2, register_value& r3)
{
    NUM(r1);
    NUM(r2);

    r3.set_int(saturating_op(r1.value, r2.value, std::minus<int64_t>{}));

    //r3.set_int(r1.value - r2.value);
}

void imuli(register_value& r1, register_value& r2, register_value& r3)
{
    NUM(r1);
    NUM(r2);

    r3.set_int(saturating_op(r1.value, r2.value, std::multiplies<int64_t>{}));

    //r3.set_int(r1.value * r2.value);
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

    ///-'128' % -1
    r3.set_int(saturating_op(r1.value, r2.value, std::modulus<int64_t>{}));

    //r3.set_int(r1.value % r2.value);
}

void sswiz(register_value& r1, register_value& r2, register_value& r3)
{
    NUM(r2);

    std::string str = r1.is_label() ? r1.label : r1.symbol;

    int r2_v = r2.value;

    if(r2_v < 0 || r2_v >= (int)str.size())
        throw std::runtime_error("Bad string SWIZ, out of bounds: index " + std::to_string(r2_v) + " in string " + str + " of length " + std::to_string(str.size()));

    if(r1.is_label())
        r3.set_label(std::string(1, str[r2_v]));

    if(r1.is_symbol())
        r3.set_symbol(std::string(1, str[r2_v]));
}

void iswiz(register_value& r1, register_value& r2, register_value& r3)
{
    if(r1.is_label() || r1.is_symbol())
        return sswiz(r1, r2, r3);

    NUM(r1);
    NUM(r2);

    std::string base_10_val = std::to_string(r1.value);
    std::string base_10_mask = std::to_string(r2.value);

    int start = 0;

    if(r2.value < 0)
        start = 1;

    std::string result = "";

    for(int i=start; i < (int)base_10_mask.size(); i++)
    {
        int val = hex_to_dec(base_10_mask[i]);

        ///1 indexed
        int offset_val = base_10_val.size() - val;

        if(offset_val < 0)
        {
            result += std::string("0");
            continue;
        }

        if(offset_val >= (int)base_10_val.size())
        {
            result += std::string("0");
            continue;
        }

        result += base_10_val[offset_val];
    }

    int64_t fval = 0;

    try
    {
        fval = std::stoi(result);
    }
    catch(...)
    {
        throw std::runtime_error("Error in iswiz, raw result string: " + result);
    }

    if(r2.value >= 0)
        r3.set_int(fval);
    else
        r3.set_int(-fval);
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
