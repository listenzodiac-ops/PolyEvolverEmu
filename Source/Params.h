#pragma once

#include <JuceHeader.h>

// All parameter IDs, enums and the pointer cache the voices use to read
// parameters from the audio thread.
namespace evo
{
constexpr int numOscs   = 4;   // osc 1-2: analog-style, osc 3-4: digital wavetable
constexpr int numLfos   = 4;
constexpr int numVoices = 4;   // the Poly Evolver is 4-voice
constexpr int numWaves  = 16;  // waves in the digital wavetable bank

enum LfoDest  { destOff = 0, destPitch, destOscShape, destPulseWidth, destCutoff, destAmp };
enum LfoShape { lfoSine = 0, lfoTriangle, lfoSaw, lfoSquare, lfoSampleHold };

inline juce::String oscID (int i, const char* name) { return "osc" + juce::String (i + 1) + "_" + name; }
inline juce::String lfoID (int i, const char* name) { return "lfo" + juce::String (i + 1) + "_" + name; }

struct ParamPointers
{
    std::atomic<float>* oscLevel[numOscs] {};
    std::atomic<float>* oscSemi [numOscs] {};
    std::atomic<float>* oscFine [numOscs] {};
    std::atomic<float>* oscShape[numOscs] {};
    std::atomic<float>* oscPW   [numOscs] {};   // only 0 and 1 are used

    std::atomic<float>* fm43       = nullptr;   // osc 4 -> osc 3 FM amount
    std::atomic<float>* fm34       = nullptr;   // osc 3 -> osc 4 FM amount

    std::atomic<float>* cutoff     = nullptr;
    std::atomic<float>* resonance  = nullptr;
    std::atomic<float>* poles      = nullptr;   // 0 = 2-pole, 1 = 4-pole
    std::atomic<float>* filterEnv  = nullptr;
    std::atomic<float>* keyTrack   = nullptr;
    std::atomic<float>* drive      = nullptr;

    std::atomic<float>* fltA = nullptr; std::atomic<float>* fltD = nullptr;
    std::atomic<float>* fltS = nullptr; std::atomic<float>* fltR = nullptr;
    std::atomic<float>* ampA = nullptr; std::atomic<float>* ampD = nullptr;
    std::atomic<float>* ampS = nullptr; std::atomic<float>* ampR = nullptr;

    std::atomic<float>* lfoRate [numLfos] {};
    std::atomic<float>* lfoShape[numLfos] {};
    std::atomic<float>* lfoAmount[numLfos] {};
    std::atomic<float>* lfoDest [numLfos] {};

    std::atomic<float>* delayTime = nullptr;
    std::atomic<float>* delayFeedback = nullptr;
    std::atomic<float>* delayTone = nullptr;
    std::atomic<float>* delayMix = nullptr;

    // Step sequencer
    std::atomic<float>* seqOn   = nullptr;
    std::atomic<float>* seqGate = nullptr;   // retrigger envelopes on every step
    std::atomic<float>* seqRate = nullptr;   // 0 = 1/4, 1 = 1/8, 2 = 1/16, 3 = 1/32
    std::atomic<float>* seqDest = nullptr;   // 0 = pitch, 1 = cutoff, 2 = pan
    std::atomic<float>* seqStep[16] {};

    std::atomic<float>* master = nullptr;
};

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
void bindPointers (juce::AudioProcessorValueTreeState& apvts, ParamPointers& p);
} // namespace evo
