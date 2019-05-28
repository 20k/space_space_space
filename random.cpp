#include "random.hpp"
#include <random>

#include <windows.h>
#include <Wincrypt.h>

std::string random_binary_string(int len)
{
    if(len <= 0)
        return std::string();

    //unsigned char random_bytes[len] = {0};

    std::vector<unsigned char> random_bytes;
    random_bytes.resize(len);

    HCRYPTPROV provider = 0;

    if(!CryptAcquireContext(&provider, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
        return "";

    if(!CryptGenRandom(provider, len, &random_bytes[0]))
        return "";

    CryptReleaseContext(provider, 0);

    std::string to_ret;
    to_ret.resize(len);

    for(int i=0; i < len; i++)
    {
        to_ret[i] = random_bytes[i];
    }

    return to_ret;
}

uint32_t random_uint_secure()
{
    std::string str = random_binary_string(4);

    uint32_t ret;
    memcpy(&ret, &str[0], sizeof(uint32_t));

    return ret;
}

uint32_t random_variable()
{
    static thread_local std::minstd_rand0 rng(random_uint_secure());

    return rng();
}

uint32_t get_random_value()
{
    static std::minstd_rand0 rng;
    static bool init = false;

    if(!init)
    {
        rng.seed(0);
        init = true;
    }

    return rng();
}
