#include "radar_field.hpp"

radar_field::radar_field()
{
    freq.resize(dim.y());
    for(auto& i : freq)
    {
        i.resize(dim.x());
    }
}

float cross2d(vec2f p1, vec2f p2)
{
    ///x0*y1 - y0*x1

    return p1.x() * p2.y() - p1.y() * p2.x();
}

///rel is [0 -> 1]

///[0, 2]
///[1, 3]
std::array<frequency_packet, 4> distribute_packet(vec2f rel, frequency_packet packet)
{
    std::array<frequency_packet, 4> ret;

    for(auto& i : ret)
    {
        i = packet;
        i.intensity = 0;
    }

    float x2 = 1;
    float x1 = 0;
    float y2 = 1;
    float y1 = 0;

    float x = rel.x();
    float y = rel.y();

    double div = (x2 - x1) * (y2 - y1);

    double q11 = (x2 - x) * (y2 - y) / div;
    double q21 = (x - x1) * (y2 - y) / div;
    double q12 = (x2 - x) * (y - y1) / div;
    double q22 = (x - x1) * (y - y1) / div;

    ret[0].intensity = q11 * packet.intensity;
    ret[1].intensity = q12 * packet.intensity;
    ret[2].intensity = q21 * packet.intensity;
    ret[3].intensity = q22 * packet.intensity;

    return ret;
}

void radar_field::add_packet(frequency_packet packet, vec2f absolute_location)
{
    vec2f relative_loc = absolute_location - offset;

    if(relative_loc.x() < 1 || relative_loc.y() < 1 || relative_loc.x() >= dim.x() - 1 || relative_loc.y() >= dim.y() - 1)
        return;

    vec2f cell_floor = floor(relative_loc);

    packet.origin = relative_loc;

    std::array<frequency_packet, 4> distributed = distribute_packet(relative_loc - cell_floor, packet);

    double my_freq_frac = (packet.frequency - MIN_FREQ) / (MAX_FREQ - MIN_FREQ);

    int band = my_freq_frac * FREQUENCY_BUCKETS;

    band = clamp(band, 0, FREQUENCY_BUCKETS-1);

    freq[(int)cell_floor.y()][(int)cell_floor.x()].buckets[band].packets.push_back(distributed[0]);
    freq[(int)cell_floor.y() + 1][(int)cell_floor.x()].buckets[band].packets.push_back(distributed[1]);
    freq[(int)cell_floor.y()][(int)cell_floor.x() + 1].buckets[band].packets.push_back(distributed[2]);
    freq[(int)cell_floor.y() + 1][(int)cell_floor.x() + 1].buckets[band].packets.push_back(distributed[3]);
}
