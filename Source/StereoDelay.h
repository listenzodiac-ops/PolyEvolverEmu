#pragma once

#include <JuceHeader.h>
#include <vector>

namespace evo
{
// Simple stereo feedback delay. The right channel runs slightly longer than the
// left so a mono signal gets a wide, bouncing echo. A one-pole lowpass in the
// feedback path darkens each repeat (like the Evolver's analog-flavoured delay).
class StereoDelay
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = (float) newSampleRate;
        const int len = (int) (newSampleRate * 1.3) + 8;   // up to 1 s * 1.25 + margin

        for (auto& b : buffer)
            b.assign ((size_t) len, 0.0f);

        writePos = 0;
        lowpass[0] = lowpass[1] = 0.0f;
        time.reset (newSampleRate, 0.05);
        time.setCurrentAndTargetValue (300.0f);
    }

    void process (float* left, float* right, int numSamples,
                  float timeMs, float feedback, float toneHz, float mix)
    {
        if (buffer[0].empty())
            return;

        time.setTargetValue (timeMs);

        const float a = std::exp (-juce::MathConstants<float>::twoPi * toneHz / sampleRate);
        const int len = (int) buffer[0].size();

        for (int i = 0; i < numSamples; ++i)
        {
            const float delaySamples = time.getNextValue() * 0.001f * sampleRate;

            const float wetL = read (buffer[0], delaySamples);
            const float wetR = read (buffer[1], delaySamples * 1.25f);

            lowpass[0] = (1.0f - a) * wetL + a * lowpass[0];
            lowpass[1] = (1.0f - a) * wetR + a * lowpass[1];

            buffer[0][(size_t) writePos] = left[i]  + lowpass[0] * feedback;
            buffer[1][(size_t) writePos] = right[i] + lowpass[1] * feedback;

            left[i]  += lowpass[0] * mix;
            right[i] += lowpass[1] * mix;

            if (++writePos >= len)
                writePos = 0;
        }
    }

private:
    float read (const std::vector<float>& b, float delaySamples) const
    {
        const int len = (int) b.size();
        delaySamples = juce::jlimit (1.0f, (float) (len - 2), delaySamples);

        float rp = (float) writePos - delaySamples;
        if (rp < 0.0f)
            rp += (float) len;

        const int i0 = (int) rp;
        const float frac = rp - (float) i0;
        const int i1 = (i0 + 1 >= len) ? 0 : i0 + 1;

        return b[(size_t) i0] + frac * (b[(size_t) i1] - b[(size_t) i0]);
    }

    std::vector<float> buffer[2];
    juce::SmoothedValue<float> time { 300.0f };
    float lowpass[2] { 0.0f, 0.0f };
    float sampleRate = 44100.0f;
    int writePos = 0;
};
} // namespace evo
