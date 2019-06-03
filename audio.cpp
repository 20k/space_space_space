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

template<typename T>
std::array<sf::Int16, SAMPLES>
apply_sample_fetcher(T in)
{
	//const unsigned SAMPLE_RATE = 44100;
	const unsigned AMPLITUDE = 1000;

	std::array<sf::Int16, SAMPLES> ret;

	const double increment = 440./44100;
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

void audio_test()
{
	const unsigned SAMPLE_RATE = 44100;

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
	}
}

