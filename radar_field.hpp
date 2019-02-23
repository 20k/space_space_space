#ifndef RADAR_FIELD_HPP_INCLUDED
#define RADAR_FIELD_HPP_INCLUDED

#include <vec/vec.hpp>
#include <array>
#include <vector>
#include <map>
#include <unordered_map>
#include <optional>
#include <SFML/System/Clock.hpp>
#include <networking/serialisable.hpp>
#include "entity.hpp"
#include "player.hpp"

#define FREQUENCY_BUCKETS 100
#define MIN_FREQ 1
#define MAX_FREQ 10000

#define MAX_RESOLUTION_INTENSITY 4

#define HEAT_FREQ 50
#define BEST_UNCERTAINTY 1

namespace sf
{
    struct RenderWindow;
}

///every packet is unique
struct alt_frequency_packet
{
    double frequency = 0;
    float intensity = 0;
    vec2f origin = {0,0};

    int iterations = 0;
    float restrict_angle = 2 * M_PI;
    float start_angle = 0;

    float packet_wavefront_width = 20;
    //float cross_section = 1;
    bool has_cs = false;
    vec2f cross_dim = {0,0};
    float cross_angle = 0;

    uint32_t id = 0;
    uint32_t id_block = 0;
    static inline uint32_t gid = 0;

    uint32_t emitted_by = -1;
    uint32_t reflected_by = -1;
    vec2f reflected_position = {0,0};

    /*uint32_t prev_reflected_by = -1;
    vec2f last_reflected_position = {0,0};*/

    ///parent packet
    std::shared_ptr<alt_frequency_packet> last_packet;
};

float get_physical_cross_section(vec2f dim, float initial_angle, float observe_angle);

struct alt_collideable
{
    vec2f dim = {0,0};
    float angle = 0;
    uint32_t uid = 0;
    vec2f pos = {0,0};

    float get_cross_section(float angle);
    float get_physical_cross_section(float angle);
};

struct hacky_clock
{
    bool once = true;
    sf::Clock clk;

    auto getElapsedTime()
    {
        if(once)
        {
            return sf::milliseconds(1000000);
        }
        else
        {
            return clk.getElapsedTime();
        }
    }

    auto restart()
    {
        once = false;

        return clk.restart();
    }

    bool should_ignore()
    {
        return getElapsedTime().asMicroseconds() / 1000. < 200;
    }
};

template<typename T>
struct alt_object_property : serialisable
{
    uint32_t id_e = -1;
    uint32_t id_r = -1;
    uint32_t uid = -1;
    T property = T();
    float frequency = 0;
    float cross_section = 1;

    alt_object_property()
    {

    }

    alt_object_property(uint32_t _id_e, uint32_t _id_r, T _property, float _frequency, float _cross_section) :
        id_e(_id_e), id_r(_id_r), property(_property), frequency(_frequency), cross_section(_cross_section)
    {

    }

    alt_object_property(uint32_t _uid, T _property) : uid(_uid), property(_property)
    {

    }

    virtual void serialise(nlohmann::json& data, bool encode)
    {
        DO_SERIALISE(id_e);
        DO_SERIALISE(id_r);
        DO_SERIALISE(uid);
        DO_SERIALISE(property);
        DO_SERIALISE(frequency);
        DO_SERIALISE(cross_section);
    }
};

struct alt_radar_sample : serialisable
{
    vec2f location;

    std::vector<float> frequencies;
    std::vector<float> intensities;

    /*std::vector<vec2f> echo_position;
    std::vector<uint32_t> echo_id; ///?*/

    std::vector<alt_object_property<vec2f>> echo_pos;
    std::vector<alt_object_property<vec2f>> echo_dir;
    std::vector<alt_object_property<vec2f>> receive_dir;
    std::vector<alt_object_property<client_renderable>> raw_renderables;
    std::vector<alt_object_property<uncertain_renderable>> low_detail;

    bool fresh = false;

    virtual void serialise(nlohmann::json& data, bool encode)
    {
        DO_SERIALISE(location);
        DO_SERIALISE(frequencies);
        DO_SERIALISE(intensities);

        DO_SERIALISE(echo_pos);
        DO_SERIALISE(echo_dir);
        DO_SERIALISE(receive_dir);

        DO_SERIALISE(raw_renderables);
        DO_SERIALISE(low_detail);
        DO_SERIALISE(fresh);

        /*DO_SERIALISE(echo_position);
        DO_SERIALISE(echo_id);*/
    }
};

struct player_model;

struct alt_radar_field
{
    vec2f target_dim;

    double time_between_ticks_s = 16/1000.;

    std::vector<alt_frequency_packet> packets;
    //std::vector<alt_frequency_packet> subtractive_packets;
    std::map<uint32_t, std::vector<alt_frequency_packet>> subtractive_packets;

    std::vector<alt_frequency_packet> speculative_packets;
    std::map<uint32_t, std::vector<alt_frequency_packet>> speculative_subtractive_packets;

    std::vector<alt_collideable> collideables;

    std::map<uint32_t, std::map<uint32_t, hacky_clock>> ignore_map;

    std::map<uint32_t, sf::Clock> sample_time;
    std::map<uint32_t, alt_radar_sample> cached_samples;

    std::vector<alt_frequency_packet> imaginary_packets;
    std::map<uint32_t, player_model*> imaginary_collideable_list;
    std::map<uint32_t, std::vector<alt_frequency_packet>> imaginary_subtractive_packets;

    float speed_of_light_per_tick = 10.5;

    alt_radar_field(vec2f in);

    std::optional<std::pair<alt_frequency_packet, alt_frequency_packet>>
    test_reflect_from(alt_frequency_packet& packet, alt_collideable& collide, std::map<uint32_t, std::vector<alt_frequency_packet>>& subtractive);

    void add_packet(alt_frequency_packet freq, vec2f pos);
    void add_packet_raw(alt_frequency_packet freq, vec2f pos);
    void add_simple_collideable(float angle, vec2f dim, vec2f location, uint32_t uid);

    void emit(alt_frequency_packet freq, vec2f pos, uint32_t uid);
    void emit_with_imaginary_packet(alt_frequency_packet freq, vec2f pos, uint32_t uid, player_model* model);

    bool packet_expired(alt_frequency_packet& packet);

    void tick(double dt_s, uint32_t iterations);
    void render(sf::RenderWindow& win);

    float get_intensity_at(vec2f pos);
    float get_intensity_at_of(vec2f pos, alt_frequency_packet& packet, std::map<uint32_t, std::vector<alt_frequency_packet>>& subtractive) const;

    bool angle_valid(alt_frequency_packet& packet, float angle);

    alt_radar_sample sample_for(vec2f pos, uint32_t uid, entity_manager& entities, std::optional<player_model*> player = std::nullopt);

    void add_player_model(player_model*);
    std::vector<player_model*> models;
};

inline
alt_radar_field& get_radar_field()
{
    static alt_radar_field radar({800, 800});

    return radar;
}

#endif // RADAR_FIELD_HPP_INCLUDED
