#ifndef FORMAT_HPP_INCLUDED
#define FORMAT_HPP_INCLUDED

#include <string>
#include <iomanip>

template<typename T>
inline
std::string to_string_with_variable_prec(const T a_value)
{
    int n = ceil(log10(a_value)) + 1;

    if(a_value <= 0.0001f)
        n = 2;

    if(n < 2)
        n = 2;

    std::ostringstream out;
    out << std::setprecision(n) << a_value;
    return out.str();
}

template<typename T>
inline
std::string to_string_with(T a_value, int forced_dp = 1, bool sign = false)
{
    if(fabs(a_value) <= 0.0999999 && fabs(a_value) >= 0.0001)
        forced_dp++;

    std::string found_sign;

    if(sign)
    {
        if(a_value > 0)
            found_sign = "+";
        //if(a_value < 0)
        //    found_sign = "-";
    }

    std::string fstr = std::to_string(a_value);

    auto found = fstr.find('.');

    if(found == std::string::npos )
    {
        return found_sign + fstr + ".0";
    }

    found += forced_dp + 1;

    if(found >= fstr.size())
        return found_sign + fstr;

    fstr.resize(found);

    return found_sign + fstr;
}

inline
std::string format(std::string to_format, const std::vector<std::string>& all_strings)
{
    int len = 0;

    for(auto& i : all_strings)
    {
        if((int)i.length() > len)
            len = i.length();
    }

    for(int i=to_format.length(); i<len; i++)
    {
        to_format = to_format + " ";
    }

    return to_format;
}

inline
std::string obfuscate(const std::string& str, bool should_obfuscate)
{
    if(!should_obfuscate)
        return str;

    std::string ret = str;

    for(int i=0; i<(int)ret.length(); i++)
    {
        if(isalnum(ret[i]))
        {
            ret[i] = '?';
        }
    }

    return ret;
}

#endif // FORMAT_HPP_INCLUDED
