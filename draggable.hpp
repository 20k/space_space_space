#ifndef DRAGGABLE_HPP_INCLUDED
#define DRAGGABLE_HPP_INCLUDED

#include <optional>

struct entity;

struct draggable
{
    size_t drag_id = -1;

    draggable(size_t _drag) : drag_id(_drag){}

    void start();
};

struct draggable_manager
{
    std::optional<draggable> current;
    entity* found = nullptr;

    void tick();
    void drop();

    bool just_dropped();
    entity* claim();

    bool dragging();
    entity* peek();

private:
    int dropped_c = 0;
};

draggable_manager& get_global_draggable_manager();

#endif // DRAGGABLE_HPP_INCLUDED
