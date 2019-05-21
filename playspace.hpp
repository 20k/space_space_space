#ifndef PLAYSPACE_HPP_INCLUDED
#define PLAYSPACE_HPP_INCLUDED

#include <networking/serialisable_fwd.hpp>
#include <vec/vec.hpp>
#include <memory>
#include <map>

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

#define ROOM_POI_SCALE 0.1f

///aka poi
struct room : serialisable, owned
{
    size_t friendly_id = -1;
    room_type::type type = room_type::POI;

    std::string name;
    vec2f position;

    entity* my_entity = nullptr;
    std::shared_ptr<alt_radar_field> field;

    std::map<uint32_t, bool> imported_waves;

    room();
    ~room();

    entity_manager* entity_manage = nullptr;

    SERIALISE_SIGNATURE();

    void tick(double dt_s);
    void add(entity* e);
    void rem(entity* e);

    vec2f get_in_local(vec2f absolute);
    vec2f get_in_absolute(vec2f local);

    void import_radio_waves_from(alt_radar_field& theirs);
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

    std::vector<room*> rooms;

    std::shared_ptr<alt_radar_field> field;
    entity_manager* entity_manage = nullptr;

    playspace();
    ~playspace();

    room* make_room(vec2f where);

    SERIALISE_SIGNATURE();

    void tick(double dt_s);
    void init_default();

    void add(entity* e);
    void rem(entity* e);
};

struct ship;
struct client_renderable;

struct ship_network_data
{
    std::vector<ship*> ships;
    std::vector<client_renderable> renderables;
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

    ship_network_data get_network_data_for(size_t id);

    std::optional<room*> get_nearby_room(entity* e);
    void exit_room(entity* e);
    void enter_room(entity* e, room* r);
};

#endif // PLAYSPACE_HPP_INCLUDED
