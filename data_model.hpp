#ifndef DATA_MODEL_HPP_INCLUDED
#define DATA_MODEL_HPP_INCLUDED

#include <vector>
#include "radar_field.hpp"
#include <networking/serialisable.hpp>
#include "player.hpp"

#define DB_USER_ID 1

struct persistent_user_data : serialisable, db_storable<persistent_user_data>, free_function
{
    player_research research;
    blueprint_manager blueprint_manage;
};

template<typename T>
struct data_model : serialisable
{
    std::vector<T> ships;
    std::vector<client_renderable> renderables;
    std::vector<client_poi_data> labels;
    alt_radar_sample sample;
    uint32_t client_network_id = 0;
    persistent_user_data persistent_data;
    size_t controlled_ship_id = -1;
    size_t last_controlled_ship_id = -1; ///not networked
    bool is_docked = false;
    std::vector<system_descriptor> connected_systems;
    vec2f room_position;
    std::vector<nearby_ship_info> nearby_info;
    std::vector<ship> nearby_unfinished_blueprints;

    SERIALISE_SIGNATURE(data_model<T>)
    {
        DO_SERIALISE(ships);
        DO_SERIALISE(renderables);
        DO_SERIALISE(labels);
        DO_SERIALISE(sample);
        DO_SERIALISE(client_network_id);
        DO_SERIALISE(persistent_data);
        DO_SERIALISE(controlled_ship_id);
        DO_SERIALISE(is_docked);
        DO_SERIALISE(connected_systems);
        DO_SERIALISE(room_position);
        DO_SERIALISE(nearby_info);
        DO_SERIALISE(nearby_unfinished_blueprints);
    }
};

template<typename T>
struct data_model_manager
{
    std::map<uint64_t, data_model<T>> data;
    std::map<uint64_t, data_model<T>> backup;

    data_model<T>& fetch_by_id(uint64_t id)
    {
        backup[id];

        return data[id];
    }
};

#endif // DATA_MODEL_HPP_INCLUDED
