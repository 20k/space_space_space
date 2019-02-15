#ifndef RADAR_FIELD_HPP_INCLUDED
#define RADAR_FIELD_HPP_INCLUDED

#include <vec/vec.hpp>
#include <array>
#include <vector>
#include <map>
#include <optional>

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

    static inline uint32_t gid = 0;

    ///{1, 0}
    float frequency = 0;
    //float direction = 0;
    float intensity = 0;
    vec2f origin = {0,0};

    int iterations = 0;
    float restrict_angle = 2 * M_PI;
    float start_angle = 0;
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

    std::map<uint32_t, frequency_packet> packets;
};

std::array<frequency_packet, 4> distribute_packet(vec2f rel, frequency_packet packet);

using frequency_chart = std::vector<std::vector<frequencies>>;

///for one playspace
struct radar_field
{
    std::vector<std::vector<frequencies>> freq;
    std::vector<std::vector<frequencies>> collisions;
    std::vector<std::vector<int>> collideables;

    vec2f offset = {0,0};
    vec2f dim = {100,100};
    vec2f target_dim = {0,0};

    radar_field(vec2f target);

    void add_packet(frequency_packet p, vec2f absolute_location, bool update_origin = true);
    void add_packet_to(std::vector<std::vector<frequencies>>& field, frequency_packet p, vec2f absolute_location, bool update_origin = true) const;

    void add_raw_packet_to(std::vector<std::vector<frequencies>>& field, frequency_packet p, int x, int y) const;

    void render(sf::RenderWindow& win);
    void tick(double dt_s);
    frequency_chart tick_raw(double dt_s, frequency_chart& next, bool collides);

    vec2f index_to_position(int x, int y);

    std::optional<vec2f> get_approximate_location(frequency_chart& chart, vec2f pos, uint32_t packet_id);

    void add_simple_collideable(vec2f real_position);

    //vec2f get_absolute_approximate_location(std::vector<std::vector<frequencies>>& freqs, )
};

#endif // RADAR_FIELD_HPP_INCLUDED