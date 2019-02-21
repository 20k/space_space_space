#ifndef STARDUST_HPP_INCLUDED
#define STARDUST_HPP_INCLUDED

#include "entity.hpp"

struct stardust : entity
{

};

struct stardust_manager
{
    stardust_manager(camera& cam, entity_manager& manage);
};

#endif // STARDUST_HPP_INCLUDED
