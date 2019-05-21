#include "playspace.hpp"
#include <networking/serialisable.hpp>
#include "entity.hpp"

void room::add(entity* e)
{
    for(auto& i : entities)
    {
        if(i == e)
            return;
    }

    entities.push_back(e);
}

bool room::rem(entity* e)
{
    for(int i=0; i < (int)entities.size(); i++)
    {
        if(entities[i] == e)
        {
            entities.erase(entities.begin() + i);
            return true;
        }
    }

    return false;
}

void room::serialise(serialise_context& ctx, nlohmann::json& data, self_t* other)
{
    DO_SERIALISE(friendly_id);
    DO_SERIALISE(type);
    DO_SERIALISE(name);
    DO_SERIALISE(position);
    DO_SERIALISE(entities);
}

void playspace::serialise(serialise_context& ctx, nlohmann::json& data, self_t* other)
{
    DO_SERIALISE(friendly_id);
    DO_SERIALISE(type);
    DO_SERIALISE(name);
    DO_SERIALISE(position);
    DO_SERIALISE(renderables);
}

void playspace_manager::serialise(serialise_context& ctx, nlohmann::json& data, self_t* other)
{
    DO_SERIALISE(spaces);
}
