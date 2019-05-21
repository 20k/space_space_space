#ifndef PLAYSPACE_HPP_INCLUDED
#define PLAYSPACE_HPP_INCLUDED

#include <networking/serialisable_fwd.hpp>
#include <vec/vec.hpp>

struct entity;
struct entity_manager;
struct alt_radar_field;

namespace room_type
{
    enum type
    {
        POI,
        ROOM
    };
}

///aka poi
struct room : serialisable, owned
{
    size_t friendly_id = -1;
    room_type::type type = room_type::POI;

    std::string name;
    vec2f position;

    room();
    ~room();

    entity_manager* entity_manage = nullptr;

    SERIALISE_SIGNATURE();

    void tick(double dt_s);
    void add(entity* e);
    void rem(entity* e);
};

namespace playspace_type
{
    enum type
    {
        SOLAR_SYSTEM,
        SHIP_SYSTEM,
    };
}

///aka solar system
struct playspace : serialisable, owned
{
    size_t friendly_id = -1;
    playspace_type::type type = playspace_type::SOLAR_SYSTEM;
    std::string name;
    vec2f position;

    std::vector<room> rooms;

    alt_radar_field* field = nullptr;
    entity_manager* entity_manage = nullptr;

    playspace();
    ~playspace();

    room* make_room(vec2f where);

    SERIALISE_SIGNATURE();

    void tick(double dt_s);
    void init_default();
};

struct playspace_manager : serialisable
{
    std::vector<playspace*> spaces;

    template<typename... U>
    playspace* make_new(U&&... u)
    {
        playspace* e = new playspace(std::forward<U>(u)...);

        spaces.push_back(e);

        return e;
    }

    SERIALISE_SIGNATURE();

    void tick(double dt_s);
};

#endif // PLAYSPACE_HPP_INCLUDED