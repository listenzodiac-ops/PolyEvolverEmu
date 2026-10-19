#include "PluginProcessor.h"
#include "PluginEditor.h"

PolyEvolverAudioProcessor::PolyEvolverAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", evo::createLayout())
{
    evo::bindPointers (apvts, params);

    synth.addSound (new evo::EvolverSound());

    for (int i = 0; i < evo::numVoices; ++i)
        synth.addVoice (new evo::EvolverVoice (params, seqState));

    synth.setNoteStealingEnabled (true);
}

void PolyEvolverAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sampleRateHz = sampleRate;
    synth.setCurrentPlaybackSampleRate (sampleRate);
    delay.prepare (sampleRate);
    lastStep = -1;
    seqState = {};
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

    // ---- step sequencer ---------------------------------------------------------------
    // Locks to the host: it runs while the DAW is playing and follows its tempo and position.
    // The standalone app has no host, so it uses an internal 120 BPM clock instead.
    bool   seqRunning = false;
    double ppq0 = 0.0, bpm = 120.0;

    if (params.seqOn->load() > 0.5f)
    {
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (pos->getIsPlaying())
                    if (auto p = pos->getPpqPosition())
                    {
                        ppq0 = *p;
                        seqRunning = true;
                        if (auto b = pos->getBpm())
                            bpm = juce::jlimit (20.0, 400.0, *b);
                    }

        if (! seqRunning && wrapperType == wrapperType_Standalone)
        {
            ppq0 = internalPpq;
            seqRunning = true;
        }
    }

    if (seqRunning)
    {
        const double spq   = sampleRateHz * 60.0 / bpm;                  // samples per quarter note
        static const double stepQuarters[4] = { 1.0, 0.5, 0.25, 0.125 };   // 1/4, 1/8, 1/16, 1/32
        const double stepQ = stepQuarters[juce::jlimit (0, 3, (int) (params.seqRate->load() + 0.5f))];
        const int    dest  = (int) (params.seqDest->load() + 0.5f);
        const bool   gate  = params.seqGate->load() > 0.5f;

        int s = 0;
        while (s < numSamples)
        {
            const double ppq = ppq0 + (double) s / spq;
            const auto   idx = (long long) std::floor (ppq / stepQ);
            const int    step = (int) (((idx % 16) + 16) % 16);

            if (step != lastStep)
            {
                lastStep = step;
                applyStep (step, dest);

                if (gate)
                    for (int v = 0; v < synth.getNumVoices(); ++v)
                        if (auto* voice = dynamic_cast<evo::EvolverVoice*> (synth.getVoice (v)))
                            voice->retrigger();
            }

            // render up to the next step boundary so steps change on the exact sample
            const double boundary = (double) (idx + 1) * stepQ;
            const int toBoundary  = juce::jmax (1, (int) std::ceil ((boundary - ppq) * spq - 1.0e-6));
            const int len         = juce::jmin (numSamples - s, toBoundary);

            synth.renderNextBlock (buffer, midi, s, len);
            s += len;
        }

        currentStep.store (lastStep);

        if (wrapperType == wrapperType_Standalone)
            internalPpq += (double) numSamples / spq;
    }
    else
    {
        if (lastStep != -1)
        {
            lastStep = -1;
            seqState = {};
            currentStep.store (-1);
        }

        synth.renderNextBlock (buffer, midi, 0, numSamples);
    }

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

void PolyEvolverAudioProcessor::applyStep (int step, int dest)
{
    const float v = params.seqStep[step]->load();

    seqState = {};

    switch (dest)
    {
        case 0:  seqState.pitch     = (float) juce::roundToInt (v * 12.0f); break;   // +/- 12 semitones
        case 1:  seqState.cutoffOct = v * 3.0f;                              break;   // +/- 3 octaves
        default: seqState.pan       = v;                                     break;   // -1 .. +1
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
