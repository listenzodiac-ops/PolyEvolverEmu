#pragma once

#include <JuceHeader.h>
#include "Params.h"
#include "WaveTables.h"

namespace evo
{
// ---------------------------------------------------------------------------
// Small DSP helpers
// ---------------------------------------------------------------------------

// PolyBLEP: cheap anti-aliasing correction for saw / pulse edges.
inline float polyBlep (float t, float dt) noexcept
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

// Rational tanh approximation, good enough for saturation and much cheaper.
inline float fastTanh (float x) noexcept
{
    x = juce::jlimit (-3.0f, 3.0f, x);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Moog-style transistor-ladder lowpass: four one-pole stages with tanh
// saturation and global resonance feedback. Take the output from stage 2
// (12 dB/oct) or stage 4 (24 dB/oct), like the Evolver's 2-/4-pole switch.
struct LadderFilter
{
    void reset() noexcept
    {
        for (auto& v : y)  v = 0.0f;
        for (auto& v : ty) v = 0.0f;
    }

    void setCoefficients (float cutoffHz, float resonance01, float sampleRate) noexcept
    {
        g = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * cutoffHz / sampleRate);
        k = resonance01 * 4.0f;   // k = 4 is about where it self-oscillates
    }

    float process (float in, float driveGain, bool fourPole) noexcept
    {
        const float x = fastTanh (in * driveGain - k * y[3]);

        y[0] += g * (x    - ty[0]);  ty[0] = fastTanh (y[0]);
        y[1] += g * (ty[0] - ty[1]); ty[1] = fastTanh (y[1]);
        y[2] += g * (ty[1] - ty[2]); ty[2] = fastTanh (y[2]);
        y[3] += g * (ty[2] - ty[3]); ty[3] = fastTanh (y[3]);

        return fourPole ? y[3] : y[1];
    }

    float y[4] {}, ty[4] {};
    float g = 0.1f, k = 0.0f;
};

// Control-rate LFO (evaluated every few samples inside the voice).
struct Lfo
{
    void reset()
    {
        phase = 0.0f;
        held = rng.nextFloat() * 2.0f - 1.0f;
    }

    float next (int shape, float rateHz, int numSamples, float sampleRate)
    {
        float v = 0.0f;

        switch (shape)
        {
            case lfoSine:     v = std::sin (juce::MathConstants<float>::twoPi * phase); break;
            case lfoTriangle: v = 1.0f - 4.0f * std::abs (phase - 0.5f);               break;
            case lfoSaw:      v = 2.0f * phase - 1.0f;                                 break;
            case lfoSquare:   v = phase < 0.5f ? 1.0f : -1.0f;                         break;
            default:          v = held;                                                break;
        }

        phase += rateHz * (float) numSamples / sampleRate;

        if (phase >= 1.0f)
        {
            phase -= std::floor (phase);
            held = rng.nextFloat() * 2.0f - 1.0f;   // new sample for S&H
        }

        return v;
    }

    float phase = 0.0f, held = 0.0f;
    juce::Random rng;
};

// ---------------------------------------------------------------------------
// Sound + Voice
// ---------------------------------------------------------------------------
class EvolverSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

class EvolverVoice : public juce::SynthesiserVoice
{
public:
    explicit EvolverVoice (const ParamPointers& params)
        : P (params), tables (WaveTables::instance()) {}

    bool canPlaySound (juce::SynthesiserSound* s) override
    {
        return dynamic_cast<EvolverSound*> (s) != nullptr;
    }

    void setCurrentPlaybackSampleRate (double newRate) override
    {
        juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);
        sr = (float) newRate;
        ampEnv.setSampleRate (newRate);
        filterEnv.setSampleRate (newRate);
    }

    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int pitchWheel) override
    {
        note = midiNoteNumber;
        vel = velocity;
        pitchBend = (float) (pitchWheel - 8192) / 8192.0f * 2.0f;   // +/- 2 semitones

        // Free-running-style random start phases for the analog oscillators.
        for (int o = 0; o < numOscs; ++o)
            phase[o] = (o < 2) ? rng.nextFloat() : 0.0f;

        for (auto& l : lfos)
            l.reset();

        // Don't reset the filter if this voice is being re-used while still ringing (avoids clicks).
        if (! ampEnv.isActive())
            filter.reset();

        readBlockParams();
        lastFilterEnv = 0.0f;
        controlCounter = 0;

        ampEnv.noteOn();
        filterEnv.noteOn();
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            ampEnv.noteOff();
            filterEnv.noteOff();
        }
        else
        {
            ampEnv.reset();
            filterEnv.reset();
            clearCurrentNote();
        }
    }

    void pitchWheelMoved (int newValue) override
    {
        pitchBend = (float) (newValue - 8192) / 8192.0f * 2.0f;
    }

    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override
    {
        if (! isVoiceActive())
            return;

        readBlockParams();

        float* left  = buffer.getWritePointer (0, startSample);
        float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1, startSample) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            if ((controlCounter++ & (controlRate - 1)) == 0)
                updateControl();

            const float aenv = ampEnv.getNextSample();
            lastFilterEnv    = filterEnv.getNextSample();

            if (! ampEnv.isActive())
            {
                clearCurrentNote();
                break;
            }

            // --- oscillators ---------------------------------------------------
            float mix = 0.0f;

            for (int o = 0; o < numOscs; ++o)
            {
                if (snap.level[o] <= 0.0001f)
                    continue;

                const float s = (o < 2) ? analogSample (phase[o], dt[o], curShape[o], curPW[o])
                                        : digitalSample (phase[o], curShape[o], band[o]);

                phase[o] += dt[o];
                if (phase[o] >= 1.0f)
                    phase[o] -= 1.0f;

                mix += s * snap.level[o];
            }

            mix *= 0.4f;   // headroom for up to four oscillators

            // --- filter + amp --------------------------------------------------
            const float filtered = filter.process (mix, snap.driveGain, snap.fourPole);
            const float out = filtered * aenv * ampGain * (0.25f + 0.75f * vel);

            left[i] += out;
            if (right != nullptr)
                right[i] += out;
        }
    }

private:
    static constexpr int controlRate = 16;   // modulation updates every 16 samples (power of two)

    // Parameter values copied once per audio block.
    struct Snapshot
    {
        float level[numOscs] {}, semi[numOscs] {}, fine[numOscs] {}, shape[numOscs] {}, pw[numOscs] {};
        float cutoff = 1000.0f, res = 0.0f, envAmt = 0.0f, keyTrack = 0.0f, driveGain = 1.0f;
        bool  fourPole = true;
        float lfoRate[numLfos] {}, lfoAmt[numLfos] {};
        int   lfoShape[numLfos] {}, lfoDest[numLfos] {};
    };

    void readBlockParams()
    {
        for (int o = 0; o < numOscs; ++o)
        {
            snap.level[o] = P.oscLevel[o]->load();
            snap.semi [o] = P.oscSemi [o]->load();
            snap.fine [o] = P.oscFine [o]->load();
            snap.shape[o] = P.oscShape[o]->load();
            snap.pw   [o] = (o < 2) ? P.oscPW[o]->load() : 0.5f;
        }

        snap.cutoff    = P.cutoff->load();
        snap.res       = P.resonance->load();
        snap.envAmt    = P.filterEnv->load();
        snap.keyTrack  = P.keyTrack->load();
        snap.driveGain = 1.0f + 5.0f * P.drive->load();
        snap.fourPole  = P.poles->load() > 0.5f;

        for (int l = 0; l < numLfos; ++l)
        {
            snap.lfoRate [l] = P.lfoRate[l]->load();
            snap.lfoAmt  [l] = P.lfoAmount[l]->load();
            snap.lfoShape[l] = (int) (P.lfoShape[l]->load() + 0.5f);
            snap.lfoDest [l] = (int) (P.lfoDest[l]->load() + 0.5f);
        }

        ampEnv.setParameters    ({ P.ampA->load(), P.ampD->load(), P.ampS->load(), P.ampR->load() });
        filterEnv.setParameters ({ P.fltA->load(), P.fltD->load(), P.fltS->load(), P.fltR->load() });
    }

    // Runs every 'controlRate' samples: LFOs, pitch, shapes, filter coefficients.
    void updateControl()
    {
        float pitchMod = 0.0f, shapeMod = 0.0f, pwMod = 0.0f, cutoffMod = 0.0f, ampAcc = 0.0f;

        for (int l = 0; l < numLfos; ++l)
        {
            // Always advance, so an LFO keeps its phase even when its destination is "Off".
            const float v = lfos[(size_t) l].next (snap.lfoShape[l], snap.lfoRate[l], controlRate, sr);
            const float a = snap.lfoAmt[l] * v;

            switch (snap.lfoDest[l])
            {
                case destPitch:      pitchMod  += a * 12.0f;            break;   // up to +/- 1 octave
                case destOscShape:   shapeMod  += a * 0.5f;             break;
                case destPulseWidth: pwMod     += a * 0.45f;            break;
                case destCutoff:     cutoffMod += a * 3.0f;             break;   // octaves
                case destAmp:        ampAcc    += snap.lfoAmt[l] * 0.5f * (v - 1.0f); break;   // tremolo
                default: break;
            }
        }

        const float baseNote = (float) note + pitchBend + pitchMod;

        for (int o = 0; o < numOscs; ++o)
        {
            const float semis = baseNote + snap.semi[o] + snap.fine[o] / 100.0f;
            const float freq  = juce::jlimit (8.0f, 0.45f * sr, 440.0f * std::exp2 ((semis - 69.0f) / 12.0f));

            dt[o]       = freq / sr;
            curShape[o] = juce::jlimit (0.0f, 1.0f, snap.shape[o] + shapeMod);
            curPW[o]    = juce::jlimit (0.05f, 0.95f, snap.pw[o] + pwMod);

            if (o >= 2)
                band[o] = WaveTables::bandForFrequency (freq, sr);
        }

        const float octaves = snap.envAmt * 5.0f * lastFilterEnv
                            + snap.keyTrack * ((float) note - 60.0f) / 12.0f
                            + cutoffMod;

        const float fc = juce::jlimit (20.0f, 0.45f * sr, snap.cutoff * std::exp2 (octaves));
        filter.setCoefficients (fc, snap.res, sr);

        ampGain = juce::jlimit (0.0f, 1.5f, 1.0f + ampAcc);
    }

    // Analog-style osc: shape 0 = triangle, 0.5 = saw, 1 = pulse (crossfaded in between).
    static float analogSample (float t, float dtv, float shape, float pw) noexcept
    {
        const float saw = 2.0f * t - 1.0f - polyBlep (t, dtv);
        const float tri = 1.0f - 4.0f * std::abs (t - 0.5f);

        float pulse = (t < pw) ? 1.0f : -1.0f;
        pulse += polyBlep (t, dtv);
        pulse -= polyBlep (std::fmod (t + 1.0f - pw, 1.0f), dtv);
        pulse -= (2.0f * pw - 1.0f);   // remove DC offset from asymmetric pulses

        if (shape < 0.5f)
        {
            const float m = shape * 2.0f;
            return tri * (1.0f - m) + saw * m;
        }

        const float m = (shape - 0.5f) * 2.0f;
        return saw * (1.0f - m) + pulse * m;
    }

    // Digital osc: shape sweeps (and crossfades) through the 16 wavetable waves.
    float digitalSample (float t, float shape, int b) const noexcept
    {
        const float pos = juce::jlimit (0.0f, 1.0f, shape) * (float) (numWaves - 1);
        const int w0 = (int) pos;
        const int w1 = std::min (w0 + 1, numWaves - 1);
        const float wf = pos - (float) w0;

        const float idx = t * (float) WaveTables::tableSize;
        const int i0 = (int) idx;
        const float f = idx - (float) i0;

        const float* t0 = tables.table (b, w0);
        const float* t1 = tables.table (b, w1);

        const float a = t0[i0] + f * (t0[i0 + 1] - t0[i0]);
        const float c = t1[i0] + f * (t1[i0 + 1] - t1[i0]);
        return a + wf * (c - a);
    }

    const ParamPointers& P;
    const WaveTables& tables;

    juce::ADSR ampEnv, filterEnv;
    LadderFilter filter;
    Lfo lfos[numLfos];
    juce::Random rng;
    Snapshot snap;

    float sr = 44100.0f;
    int note = 60;
    float vel = 1.0f, pitchBend = 0.0f;
    float lastFilterEnv = 0.0f, ampGain = 1.0f;
    unsigned int controlCounter = 0;

    float phase[numOscs] {}, dt[numOscs] {}, curShape[numOscs] {}, curPW[numOscs] {};
    int band[numOscs] {};
};
} // namespace evo
