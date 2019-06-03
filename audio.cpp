#include "audio.hpp"
#include <SFML/Audio.hpp>
#include <cmath>
#include <iostream>
#include <assert.h>
#include <vec/vec.hpp>

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

#define SAMPLES 44100
#define SAMPLE_RATE (int)(4410 * 1.5)

template<typename T>
std::array<sf::Int16, SAMPLES>
apply_sample_fetcher(T in, float amplitude, double frequency)
{
	//const unsigned SAMPLE_RATE = 44100;
	const unsigned AMPLITUDE = amplitude;

	std::array<sf::Int16, SAMPLES> ret;

	const double increment = frequency/SAMPLE_RATE;
	double x = 0;
	for (unsigned i = 0; i < SAMPLES; i++) {
		ret[i] = AMPLITUDE * in(std::fmod(x * M_PI * 2, M_PI*2));
		x += increment;
	}

	return ret;
}

double triangle_wave(double in)
{
    if(in < M_PI)
        return mix(-1, 1, in / M_PI);

    if(in >= M_PI)
        return mix(1, -1, (in - M_PI) / M_PI);

    return 0;
}

double square_wave(double in)
{
    return sin(in) > 0 ? 1 : -1;
}

double sawtooth_wave(double in)
{
    return mix(-1, 1, in / (2 * M_PI));
}

std::array<sf::Int16, SAMPLES>
smooth_samples(const std::array<sf::Int16, SAMPLES>& in)
{
    auto ret = in;

    for(int i=SAMPLES-100; i < SAMPLES; i++)
    {
        float frac = (i - (SAMPLES - 100)) / 99.;

        ret[i] = mix(in[i], 0, frac);
    }

    return ret;
}


void shared_audio::play_all()
{
    for(int i=0; i < (int)buffers.size(); i++)
    {
        if(sounds[i]->getStatus() == sf::Sound::Status::Stopped || sounds[i]->getStatus() == sf::Sound::Status::Paused)
        {
            delete sounds[i];
            delete buffers[i];

            sounds.erase(sounds.begin() + i);
            buffers.erase(buffers.begin() + i);
            i--;
            continue;
        }
    }

    for(int i=0; i < (int)relative_amplitudes.size(); i++)
    {
        sf::SoundBuffer* buf = new sf::SoundBuffer;
        sf::Sound* sound = new sf::Sound;

        float amp = relative_amplitudes[i];
        float freq = frequencies[i];

        if(types[i] == waveform::SIN)
            buf->loadFromSamples(smooth_samples(apply_sample_fetcher(sinf, amp, freq)).data(), SAMPLES, 1, SAMPLE_RATE);
        if(types[i] == waveform::SAW)
            buf->loadFromSamples(smooth_samples(apply_sample_fetcher(sawtooth_wave, amp, freq)).data(), SAMPLES, 1, SAMPLE_RATE);
        if(types[i] == waveform::TRI)
            buf->loadFromSamples(smooth_samples(apply_sample_fetcher(triangle_wave, amp, freq)).data(), SAMPLES, 1, SAMPLE_RATE);
        if(types[i] == waveform::SQR)
            buf->loadFromSamples(smooth_samples(apply_sample_fetcher(square_wave, amp, freq)).data(), SAMPLES, 1, SAMPLE_RATE);

        sound->setBuffer(*buf);
        sound->play();
    }

    relative_amplitudes.clear();
    frequencies.clear();
    types.clear();
}

void audio_test()
{
	/*const unsigned SAMPLE_RATE = 44100;

    auto swave = apply_sample_fetcher(square_wave);

	sf::SoundBuffer Buffer;
	if (!Buffer.loadFromSamples(swave.data(), SAMPLES, 1, SAMPLE_RATE)) {
		std::cerr << "Loading failed!" << std::endl;
		return;
	}

	sf::Sound Sound;
	Sound.setBuffer(Buffer);
	Sound.setLoop(true);
	Sound.play();
	while (1) {
		sf::sleep(sf::milliseconds(100));
	}*/


}

