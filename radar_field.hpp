#ifndef RADAR_FIELD_HPP_INCLUDED
#define RADAR_FIELD_HPP_INCLUDED

#include <vec/vec.hpp>
#include <array>
#include <vector>
#include <map>
#include <unordered_map>
#include <optional>
#include <SFML/System/Clock.hpp>

#define FREQUENCY_BUCKETS 100
#define MIN_FREQ 1
#define MAX_FREQ 10000

namespace sf
{
    struct RenderWindow;
}

struct frequency_packet
{
    uint32_t id = -1;
    uint32_t transient_id = -1;
    uint32_t collides_with = -1;

    static inline uint32_t gid = 0;

    ///{1, 0}
    float frequency = 0;
    //float direction = 0;
    float intensity = 0;
    vec2f origin = {0,0};

    int iterations = 0;
    float restrict_angle = 2 * M_PI;
    float start_angle = 0;

    float packet_wavefront_width = 7.5;

    //std::vector<uint32_t> ignore_list;
};

/*struct frequency_band
{
    float band = 0; ///?
    std::vector<frequency_packet> packets;
};*/

/*struct frequencies
{
     std::array<frequency_band, FREQUENCY_BUCKETS> buckets;
};*/

struct frequencies
{
    //std::vector<frequency_packet> packets;

    std::vector<frequency_packet> packets;

    //std::unordered_map<uint32_t, frequency_packet> packets;
};

std::array<frequency_packet, 4> distribute_packet(vec2f rel, frequency_packet packet);

using frequency_chart = std::vector<std::vector<frequencies>>;

struct ignore_data
{
    uint32_t id = -1;
    sf::Clock clk;
};

struct collideable_ignore
{
    std::vector<ignore_data> to_ignore;

    void ignore(uint32_t id)
    {
        for(auto& i : to_ignore)
        {
            if(i.id == id)
            {
                i.clk.restart();
                return;
            }
        }

        to_ignore.push_back({id});
    }

    bool ignored(uint32_t id)
    {
        for(int i=0; i < (int)to_ignore.size(); i++)
        {
            if(to_ignore[i].id == id && to_ignore[i].clk.getElapsedTime().asMilliseconds() < 500)
            {
                return true;
            }

            if(to_ignore[i].clk.getElapsedTime().asMilliseconds() >= 500)
            {
                to_ignore.erase(to_ignore.begin() + i);
                i--;
                continue;
            }
        }

        return false;
    }
};

struct rcollideable
{
    vec2f dim = {0,0};
    float angle = 0;
    uint32_t uid = 0;

    float get_cross_section(float angle);
};

///for one playspace
struct radar_field
{
    std::vector<std::vector<frequencies>> freq;
    std::vector<std::vector<frequencies>> collisions;
    std::vector<std::vector<std::vector<rcollideable>>> collideables;

    std::map<uint32_t, collideable_ignore> ignore_map;

    //std::map<uint32_t, ignore_status> ignore_map;

    vec2f offset = {0,0};
    vec2f dim = {100,100};
    vec2f target_dim = {0,0};

    float light_move_per_tick = 1.5;

    radar_field(vec2f target);

    void add_packet(const frequency_packet& p, vec2f absolute_location, bool update_origin = true);
    void add_packet_to(std::vector<std::vector<frequencies>>& field, const frequency_packet& p, vec2f absolute_location, bool update_origin = true, bool distribute = true) const;

    void add_raw_packet_to(std::vector<std::vector<frequencies>>& field, const frequency_packet& p, int x, int y) const;

    float get_intensity_at(int idx, int idy);
    float get_intensity_at_of(int idx, int idy, const frequency_packet& pack);
    void render(sf::RenderWindow& win);
    void tick(double dt_s, uint32_t iterations);
    frequency_chart tick_raw(double dt_s, frequency_chart& next, bool collides, uint32_t iterations);

    void prune(frequency_chart& in);

    vec2f index_to_position(int x, int y);

    std::optional<vec2f> get_approximate_location(frequency_chart& chart, vec2f pos, uint32_t packet_id);

    void add_simple_collideable(float angle, vec2f dim, vec2f location, uint32_t uid);

    //vec2f get_absolute_approximate_location(std::vector<std::vector<frequencies>>& freqs, )
};

///every packet is unique
struct alt_frequency_packet
{
    double frequency = 0;
    float intensity = 0;
    vec2f origin = {0,0};

    int iterations = 0;
    float restrict_angle = 2 * M_PI;
    float start_angle = 0;

    float packet_wavefront_width = 7.5;

    uint32_t id = 0;
    uint32_t id_block = 0;
    static inline uint32_t gid = 0;
};

struct alt_collideable
{
    vec2f dim = {0,0};
    float angle = 0;
    uint32_t uid = 0;
    vec2f pos = {0,0};

    float get_cross_section(float angle);
};

struct alt_radar_field
{
    vec2f target_dim;

    std::vector<alt_frequency_packet> packets;
    //std::vector<alt_frequency_packet> subtractive_packets;
    std::map<uint32_t, std::vector<alt_frequency_packet>> subtractive_packets;

    std::vector<alt_collideable> collideables;

    std::map<uint32_t, std::map<uint32_t, sf::Clock>> ignore_map;

    float speed_of_light_per_tick = 1.5;

    alt_radar_field(vec2f in);

    void add_packet(alt_frequency_packet freq, vec2f pos);
    void add_simple_collideable(float angle, vec2f dim, vec2f location, uint32_t uid);

    void tick(double dt_s, uint32_t iterations);
    void render(sf::RenderWindow& win);

    float get_intensity_at(vec2f pos);
    float get_intensity_at_of(vec2f pos, alt_frequency_packet& packet);
};

#endif // RADAR_FIELD_HPP_INCLUDED
