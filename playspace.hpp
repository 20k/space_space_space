#ifndef PLAYSPACE_HPP_INCLUDED
#define PLAYSPACE_HPP_INCLUDED

#include <networking/serialisable_fwd.hpp>
#include <vec/vec.hpp>
#include <memory>
#include <map>

struct entity;
struct ship;
struct entity_manager;
struct alt_radar_field;

#define RENDER_LAYER_REALSPACE 0
#define RENDER_LAYER_SSPACE 1

namespace room_type
{
    enum type
    {
        POI,
        ROOM
    };
}

namespace space_type
{
    enum type
    {
        REAL_SPACE = 0,///thrusters
        S_SPACE = 1,///sdrive
    };
}

#define ROOM_POI_SCALE 0.01f

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

    std::vector<playspace*> connections;

    std::vector<room*> rooms;
    std::vector<room*> pending_rooms;

    std::shared_ptr<alt_radar_field> field;
    entity_manager* entity_manage = nullptr;

    playspace();
    ~playspace();

    std::vector<room*> all_rooms();

    room* make_room(vec2f where, float entity_rad);
    void delete_room(room* r);

    SERIALISE_SIGNATURE();

    void tick(double dt_s);
    void init_default(int seed);

    void add(entity* e);
    void rem(entity* e);
};

void playspace_connect(playspace* p1, playspace* p2);
bool playspaces_connected(playspace* p1, playspace* p2);

struct ship;
struct client_renderable;

struct client_poi_data : serialisable
{
    std::string name;
    vec2f position;
    size_t poi_pid = 0;
    std::string type;

    SERIALISE_SIGNATURE();
};

struct ship_network_data
{
    std::vector<ship*> ships;
    std::vector<client_renderable> renderables;
    std::vector<client_poi_data> pois;
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

    ship_network_data get_network_data_for(entity* e, size_t id);

    std::optional<room*> get_nearby_room(entity* e);
    void exit_room(entity* e);
    void enter_room(entity* e, room* r);

    std::pair<playspace*, room*> get_location_for(entity* e);
    std::vector<playspace*> get_connected_systems_for(entity* e);
    std::optional<playspace*> get_playspace_from_id(size_t pid);
    std::optional<std::pair<playspace*, room*>> get_room_from_id(size_t pid);

    bool start_warp_travel(ship& s, size_t pid);
    bool start_room_travel(ship& s, size_t pid);
};

#endif // PLAYSPACE_HPP_INCLUDED
