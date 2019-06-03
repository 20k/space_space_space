#ifndef AUDIO_HPP_INCLUDED
#define AUDIO_HPP_INCLUDED

#include <networking/serialisable_fwd.hpp>
#include <vector>

namespace sf
{
    struct SoundBuffer;
    struct Sound;
}

namespace waveform
{
    enum type
    {
        SIN,
        SAW,
        TRI,
        SQR,
        COUNT,
    };
}

struct shared_audio : serialisable
{
    std::vector<float> relative_amplitudes;
    std::vector<double> frequencies;
    std::vector<waveform::type> types;

    void add(float intensity, double frequency, waveform::type type);

    std::vector<sf::SoundBuffer*> buffers;
    std::vector<sf::Sound*> sounds;

    void play_all();

    SERIALISE_SIGNATURE_SIMPLE(shared_audio);
};

#endif // AUDIO_HPP_INCLUDED
