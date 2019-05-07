#ifndef DRAGGABLE_HPP_INCLUDED
#define DRAGGABLE_HPP_INCLUDED

#include <optional>
#include <networking/serialisable.hpp>

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
    //entity* found = nullptr;
    ///well this is really just terrible
    void* drag_source = nullptr;

    uint64_t hash_code = 0;

    template<typename T>
    void set_drag_source(std::vector<T>* in)
    {
        drag_source = (void*)in;

        hash_code = typeid(T).hash_code();
    }

    void tick();
    void drop();

    bool just_dropped();
    //entity* claim();

    template<typename T>
    entity* claim()
    {
        if(!current)
            throw std::runtime_error("Bad Claim entity");

        if(!drag_source)
            throw std::runtime_error("No drag source");

        if(hash_code != typeid(T).hash_code())
            throw std::runtime_error("Bad type in claim");

        std::vector<T>& check = *(std::vector<T>*)drag_source;

        entity* found = nullptr;

        for(T& val : check)
        {
            found = find_by_id(val, current.value().drag_id);

            if(found)
                break;
        }

        return found;
    }

    //bool trying_dragging();
    bool dragging();
    //entity* peek();

    void reset();

private:
    int dropped_c = 0;
};

draggable_manager& get_global_draggable_manager();

#endif // DRAGGABLE_HPP_INCLUDED
