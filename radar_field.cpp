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

    if(relative_loc.x() < 0 || relative_loc.y() < 0 || relative_loc.x() >= dim.x() || relative_loc.y() >= dim.y())
        return;

    vec2f cell_floor = floor(relative_loc);

    packet.origin = relative_loc;

    std::array<frequency_packet, 4> distributed = distribute_packet(relative_loc - cell_floor, packet);
}
