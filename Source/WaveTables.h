#pragma once

#include <JuceHeader.h>
#include "Params.h"
#include <array>
#include <vector>

namespace evo
{
// Band-limited single-cycle wavetables for the "digital" oscillators, standing in
// for the Evolver's 128 digital waveforms. There are 16 waves, each built
// additively, and 7 octave-spaced bands so high notes use fewer harmonics
// (which avoids aliasing).
//
//   waves  0-3  : sine -> triangle-ish -> saw-ish (increasingly bright)
//   waves  4-7  : odd harmonics only (hollow, square-ish)
//   waves  8-11 : formant-like peaks moving up the spectrum
//   waves 12-15 : seeded pseudo-random harmonic spectra (metallic / bell-ish)
class WaveTables
{
public:
    static constexpr int tableSize = 2048;
    static constexpr int numBands  = 7;      // max harmonics: 128, 64, 32, 16, 8, 4, 2
    static constexpr int maxHarmonics = 128;

    static const WaveTables& instance()
    {
        static const WaveTables tables;      // built once, shared by all plugin instances
        return tables;
    }

    // Each table has tableSize + 1 entries (extra guard sample for interpolation).
    const float* table (int band, int wave) const noexcept
    {
        return data.data() + (((size_t) band * numWaves) + (size_t) wave) * (tableSize + 1);
    }

    static int bandForFrequency (float freqHz, float sampleRate) noexcept
    {
        for (int b = 0; b < numBands; ++b)
            if ((float) (maxHarmonics >> b) * freqHz <= 0.45f * sampleRate)
                return b;

        return numBands - 1;
    }

private:
    WaveTables()
    {
        data.assign ((size_t) numBands * numWaves * (tableSize + 1), 0.0f);

        std::array<std::array<float, maxHarmonics + 1>, numWaves> amp {}, phase {};

        for (int w = 0; w < numWaves; ++w)
        {
            juce::Random rng (1000 + w);

            for (int h = 1; h <= maxHarmonics; ++h)
            {
                amp[(size_t) w][(size_t) h]   = recipeAmp (w, h, rng);
                phase[(size_t) w][(size_t) h] = (w >= 12) ? rng.nextFloat() * juce::MathConstants<float>::twoPi : 0.0f;
            }
        }

        for (int b = 0; b < numBands; ++b)
        {
            const int maxH = maxHarmonics >> b;

            for (int w = 0; w < numWaves; ++w)
            {
                float* t = data.data() + (((size_t) b * numWaves) + (size_t) w) * (tableSize + 1);

                for (int h = 1; h <= maxH; ++h)
                {
                    const float a = amp[(size_t) w][(size_t) h];
                    if (a == 0.0f)
                        continue;

                    const float ph = phase[(size_t) w][(size_t) h];
                    const float step = juce::MathConstants<float>::twoPi * (float) h / (float) tableSize;

                    for (int n = 0; n < tableSize; ++n)
                        t[n] += a * std::sin (step * (float) n + ph);
                }

                float peak = 0.0f;
                for (int n = 0; n < tableSize; ++n)
                    peak = std::max (peak, std::abs (t[n]));

                if (peak > 0.0f)
                    for (int n = 0; n < tableSize; ++n)
                        t[n] /= peak;

                t[tableSize] = t[0];   // guard sample
            }
        }
    }

    static float recipeAmp (int w, int h, juce::Random& rng)
    {
        const float hf = (float) h;

        if (w < 4)   // sine -> saw-like
            return 1.0f / std::pow (hf, 3.0f - 0.55f * (float) w);

        if (w < 8)   // odd harmonics only
        {
            if ((h & 1) == 0)
                return 0.0f;

            return 1.0f / std::pow (hf, 2.0f - 0.4f * (float) (w - 4));
        }

        if (w < 12)  // moving formant peak
        {
            const float centre = 3.0f + 3.0f * (float) (w - 8);
            const float d = (hf - centre) / 1.5f;
            return 1.0f / std::pow (hf, 1.5f) + 0.9f * std::exp (-d * d) / std::sqrt (hf);
        }

        // random spectra
        return (0.3f + 0.7f * rng.nextFloat()) / hf;
    }

    std::vector<float> data;
};
} // namespace evo
