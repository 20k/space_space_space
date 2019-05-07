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
        dropped_c--;

    if(current)
    {
        draggable& d = current.value();

        if(d.drag->cleanup)
            current = std::nullopt;
    }

    if(!current)
    {
        dropped_c = 0;
    }
}

void draggable_manager::drop()
{
    if(!current)
        return;

    dropped_c = 2;
}

bool draggable_manager::just_dropped()
{
    return current && (dropped_c > 0);
}

entity* draggable_manager::claim()
{
    entity* en = current.value().drag;

    current = std::nullopt;
    dropped_c = 0;

    return en;
}
