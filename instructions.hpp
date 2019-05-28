#ifndef INSTRUCTIONS_HPP_INCLUDED
#define INSTRUCTIONS_HPP_INCLUDED

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


#endif // INSTRUCTIONS_HPP_INCLUDED
