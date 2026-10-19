#pragma once

#include <JuceHeader.h>
#include "Params.h"
#include "SynthVoice.h"
#include "StereoDelay.h"

class PolyEvolverAudioProcessor : public juce::AudioProcessor
{
public:
    PolyEvolverAudioProcessor();
    ~PolyEvolverAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Order matters: apvts must be constructed before 'params' and 'synth'.
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;   // shared with the on-screen keyboard

private:
    evo::ParamPointers params;
    juce::Synthesiser synth;
    evo::StereoDelay delay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PolyEvolverAudioProcessor)
};
