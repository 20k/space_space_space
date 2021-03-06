#ifndef PLAYSPACE_HPP_INCLUDED
#define PLAYSPACE_HPP_INCLUDED

#include <networking/serialisable_fwd.hpp>
#include <vec/vec.hpp>
#include <memory>
#include <map>
#include <optional>

struct entity;
struct ship;
struct component;
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

namespace poi_type
{
    enum type
    {
        UNKNOWN = 0,
        SUN = 1,
        PLANET = 2,
        DEAD_SPACE = 3,
        ASTEROID_BELT = 4,
        U_5 = 5,
        U_6 = 6,
        U_7 = 7,
        U_8 = 8,
        U_9 = 9,
    };

    static inline std::vector<std::string> rnames
    {
        "UNK",
        "SUN",
        "PLN",
        "DED",
        "AST",
        "UN5",
        "UN6",
        "UN7",
        "UN8",
        "UN9",
    };
}

#define ROOM_POI_SCALE 0.01f

struct heatable_entity;
struct precise_aggregator;

///aka poi
struct room : serialisable, owned
{
    size_t friendly_id = -1;
    //room_type::type type = room_type::POI;

    poi_type::type ptype = poi_type::DEAD_SPACE;
    int poi_offset = 0; ///idx offset

    std::string name;
    vec2f position;

    bool radar_active = true;

    std::shared_ptr<entity> my_entity;
    std::shared_ptr<precise_aggregator> packet_harvester;
    std::shared_ptr<alt_radar_field> field;

    room();
    ~room();

    entity_manager* entity_manage = nullptr;

    SERIALISE_SIGNATURE_NOSMOOTH(room);

    void tick(double dt_s, bool reaggregate);
    void add(std::shared_ptr<entity> e);
    void rem(std::shared_ptr<entity> e);

    vec2f get_in_local(vec2f absolute);
    vec2f get_in_absolute(vec2f local);

    void import_radio_waves_from(alt_radar_field& theirs);

    std::vector<std::shared_ptr<ship>> get_nearby_unfinished_ships(ship& me);
    std::optional<std::shared_ptr<ship>> get_nearby_unfinished_ship(ship& me, size_t ship_pid);
    std::vector<std::pair<ship, std::vector<component>>> get_nearby_accessible_ships(ship& me, std::optional<size_t> unconditional_access = std::nullopt);

    bool try_dock_to(size_t child, size_t parent);
};

void room_merge(room* r1, room* r2);

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
    int iteration = 0;

    size_t friendly_id = -1;
    playspace_type::type type = playspace_type::SOLAR_SYSTEM;
    std::string name;
    vec2f position;

    std::vector<playspace*> connections;

    std::vector<room*> rooms;
    std::vector<room*> pending_rooms;

    std::shared_ptr<alt_radar_field> field;
    entity_manager* entity_manage = nullptr;
    entity_manager* drawables = nullptr;

    std::map<size_t, std::vector<std::shared_ptr<entity>>> room_specific_cleanup;

    playspace();
    ~playspace();

    std::vector<room*> all_rooms();

    room* make_room(vec2f where, float entity_rad, poi_type::type ptype);
    void delete_room(room* r);

    SERIALISE_SIGNATURE_NOSMOOTH(playspace);

    void tick(double dt_s);
    void init_default(int seed);

    void add(std::shared_ptr<entity> e);
    void rem(std::shared_ptr<entity> e);
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
    int offset = 0;

    SERIALISE_SIGNATURE_NOSMOOTH(client_poi_data);
};

struct ship_network_data
{
    std::vector<std::shared_ptr<ship>> ships;
    std::vector<client_renderable> renderables;
    std::vector<client_poi_data> pois;
};

struct cpu_move_args;

struct ship_location_data
{
    std::shared_ptr<ship> s;
    playspace* play = nullptr;
    room* r = nullptr;
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

    SERIALISE_SIGNATURE_NOSMOOTH(playspace_manager);

    void tick(double dt_s);

    ship_network_data get_network_data_for(const std::shared_ptr<entity>& e, size_t id);

    std::optional<room*> get_nearby_room(const std::shared_ptr<entity>& e);
    void exit_room(const std::shared_ptr<entity>& e);
    void enter_room(const std::shared_ptr<entity>& e, room* r);

    std::pair<playspace*, room*> get_location_for(const std::shared_ptr<entity>& e);
    std::vector<playspace*> get_connected_systems_for(const std::shared_ptr<entity>& e);
    std::optional<playspace*> get_playspace_from_id(size_t pid);
    std::optional<std::pair<playspace*, room*>> get_room_from_id(size_t pid);

    std::optional<room*> get_room_from_symbol(playspace* play, const std::string& sym);
    std::optional<playspace*> get_playspace_from_name(const std::string& name);

    bool start_warp_travel(ship& s, size_t pid);
    bool start_room_travel(ship& s, size_t pid);
    //bool start_realspace_travel(ship& s, size_t pid);
    //bool start_realspace_travel(ship& s, vec2f coord);

    bool start_realspace_travel(ship& s, const cpu_move_args& args);

    std::map<uint64_t, ship_location_data> get_locations_for(const std::vector<uint64_t>& users);
    std::optional<ship*> get_docked_ship(uint64_t ship_pid);
    void undock(const std::vector<uint64_t>& in);
};

#endif // PLAYSPACE_HPP_INCLUDED
