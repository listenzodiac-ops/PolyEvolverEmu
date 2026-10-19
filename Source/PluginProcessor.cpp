#include "PluginProcessor.h"
#include "PluginEditor.h"

PolyEvolverAudioProcessor::PolyEvolverAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", evo::createLayout())
{
    evo::bindPointers (apvts, params);

    synth.addSound (new evo::EvolverSound());

    for (int i = 0; i < evo::numVoices; ++i)
        synth.addVoice (new evo::EvolverVoice (params));

    synth.setNoteStealingEnabled (true);
}

void PolyEvolverAudioProcessor::prepareToPlay (double sampleRate, int)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    delay.prepare (sampleRate);
}

bool PolyEvolverAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PolyEvolverAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    // Merge notes played on the on-screen keyboard into the host's MIDI stream.
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    synth.renderNextBlock (buffer, midi, 0, numSamples);

    if (buffer.getNumChannels() >= 2)
    {
        delay.process (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples,
                       params.delayTime->load(), params.delayFeedback->load(),
                       params.delayTone->load(), params.delayMix->load());
    }

    // Master volume + soft clip so a loud patch can't blast your ears or speakers.
    const float gain = juce::Decibels::decibelsToGain (params.master->load());

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] = std::tanh (data[i] * gain);
    }
}

juce::AudioProcessorEditor* PolyEvolverAudioProcessor::createEditor()
{
    return new PolyEvolverAudioProcessorEditor (*this);
}

void PolyEvolverAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PolyEvolverAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PolyEvolverAudioProcessor();
}
