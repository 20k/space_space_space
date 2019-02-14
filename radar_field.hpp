#ifndef RADAR_FIELD_HPP_INCLUDED
#define RADAR_FIELD_HPP_INCLUDED

#include <vec/vec.hpp>
#include <array>
#include <vector>

#define FREQUENCY_BUCKETS 100

struct frequency_packet
{
    ///{1, 0}
    float frequency = 0;
    //float direction = 0;
    float intensity = 0;
    vec2f origin = {0,0};
};

struct frequency_band
{
    float band = 0; ///?
    std::vector<frequency_packet> packets;
};

struct frequencies
{
     std::array<frequency_band, FREQUENCY_BUCKETS> buckets;
};

std::array<frequency_packet, 4> distribute_packet(vec2f rel, frequency_packet packet);

///for one playspace
struct radar_field
{
    std::vector<std::vector<frequencies>> freq;

    vec2f offset = {0,0};
    vec2f dim = {100,100};

    radar_field();

    void add_packet(frequency_packet p, vec2f absolute_location);
};

#endif // RADAR_FIELD_HPP_INCLUDED
