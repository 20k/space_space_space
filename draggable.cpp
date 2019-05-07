#include "draggable.hpp"
#include "entity.hpp"

draggable_manager& get_global_draggable_manager()
{
    static thread_local draggable_manager d;

    return d;
}

void draggable::start()
{
    get_global_draggable_manager().current = *this;
}

void draggable_manager::tick()
{
    if(dropped_c > 0)
    {
        dropped_c--;

        if(dropped_c == 0)
        {
            current = std::nullopt;
            found = nullptr;
        }
    }

    if(!current)
    {
        dropped_c = 0;
        found = nullptr;
    }
}

void draggable_manager::drop()
{
    if(!current || dropped_c > 0 || !found)
        return;

    dropped_c = 2;
}

bool draggable_manager::just_dropped()
{
    return current && (dropped_c > 0) && found;
}

entity* draggable_manager::claim()
{
    if(!current)
        throw std::runtime_error("Bad Claim entity");

    entity* en = found;

    current = std::nullopt;
    dropped_c = 0;

    return en;
}

bool draggable_manager::trying_dragging()
{
    return current && dropped_c == 0;
}

bool draggable_manager::dragging()
{
    return current && dropped_c == 0 && found;
}

entity* draggable_manager::peek()
{
    if(!current || dropped_c == 0)
        throw std::runtime_error("Nothing to peek");

    return found;
}

void draggable_manager::reset()
{
    current = std::nullopt;
    found = nullptr;
    dropped_c = 0;
}
