#include "Params.h"

namespace evo
{
using APF  = juce::AudioParameterFloat;
using APC  = juce::AudioParameterChoice;
using Attr = juce::AudioParameterFloatAttributes;
using PID  = juce::ParameterID;
using Range = juce::NormalisableRange<float>;

static Range skewed (float lo, float hi, float centre)
{
    Range r (lo, hi);
    r.setSkewForCentre (centre);
    return r;
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // ---- Oscillators --------------------------------------------------------
    for (int i = 0; i < numOscs; ++i)
    {
        const bool analog = i < 2;
        const juce::String n = "Osc " + juce::String (i + 1) + " ";

        layout.add (std::make_unique<APF> (PID { oscID (i, "level"), 1 }, n + "Level",
                                           Range (0.0f, 1.0f, 0.001f),
                                           i == 0 ? 1.0f : (i == 1 ? 0.6f : 0.0f)));

        layout.add (std::make_unique<APF> (PID { oscID (i, "semi"), 1 }, n + "Semitones",
                                           Range (-24.0f, 24.0f, 1.0f), 0.0f,
                                           Attr().withLabel ("st")));

        layout.add (std::make_unique<APF> (PID { oscID (i, "fine"), 1 }, n + "Fine",
                                           Range (-100.0f, 100.0f, 0.1f),
                                           i == 1 ? 7.0f : 0.0f,   // slight detune on osc 2
                                           Attr().withLabel ("ct")));

        // Analog osc: 0 = triangle, 0.5 = saw, 1 = pulse. Digital osc: wave 1..16.
        layout.add (std::make_unique<APF> (PID { oscID (i, "shape"), 1 },
                                           n + (analog ? "Shape" : "Wave"),
                                           Range (0.0f, 1.0f, 0.001f),
                                           analog ? 0.5f : 0.0f));

        if (analog)
            layout.add (std::make_unique<APF> (PID { oscID (i, "pw"), 1 }, n + "Pulse Width",
                                               Range (0.05f, 0.95f, 0.001f), 0.5f));
    }

    // ---- Digital FM (oscillators 3 and 4 only) -----------------------------
    layout.add (std::make_unique<APF> (PID { "fm_4to3", 1 }, "FM 4 > 3",
                                       Range (0.0f, 1.0f, 0.001f), 0.0f));
    layout.add (std::make_unique<APF> (PID { "fm_3to4", 1 }, "FM 3 > 4",
                                       Range (0.0f, 1.0f, 0.001f), 0.0f));

    // ---- Filter -------------------------------------------------------------
    layout.add (std::make_unique<APF> (PID { "cutoff", 1 }, "Cutoff", skewed (20.0f, 20000.0f, 1000.0f),
                                       1500.0f, Attr().withLabel ("Hz")));
    layout.add (std::make_unique<APF> (PID { "resonance", 1 }, "Resonance",
                                       Range (0.0f, 1.0f, 0.001f), 0.25f));
    layout.add (std::make_unique<APC> (PID { "poles", 1 }, "Poles",
                                       juce::StringArray { "2-pole", "4-pole" }, 1));
    layout.add (std::make_unique<APF> (PID { "filter_env", 1 }, "Env Amount",
                                       Range (-1.0f, 1.0f, 0.001f), 0.5f));
    layout.add (std::make_unique<APF> (PID { "key_track", 1 }, "Key Track",
                                       Range (0.0f, 1.0f, 0.001f), 0.5f));
    layout.add (std::make_unique<APF> (PID { "drive", 1 }, "Drive",
                                       Range (0.0f, 1.0f, 0.001f), 0.1f));

    // ---- Envelopes ----------------------------------------------------------
    auto time = [] { return skewed (0.001f, 10.0f, 0.4f); };
    auto sec  = [] { return Attr().withLabel ("s"); };

    layout.add (std::make_unique<APF> (PID { "flt_attack",  1 }, "Filter Attack",  time(), 0.005f, sec()));
    layout.add (std::make_unique<APF> (PID { "flt_decay",   1 }, "Filter Decay",   time(), 0.4f,   sec()));
    layout.add (std::make_unique<APF> (PID { "flt_sustain", 1 }, "Filter Sustain", Range (0.0f, 1.0f, 0.001f), 0.2f));
    layout.add (std::make_unique<APF> (PID { "flt_release", 1 }, "Filter Release", time(), 0.3f,   sec()));

    layout.add (std::make_unique<APF> (PID { "amp_attack",  1 }, "Amp Attack",  time(), 0.005f, sec()));
    layout.add (std::make_unique<APF> (PID { "amp_decay",   1 }, "Amp Decay",   time(), 0.3f,   sec()));
    layout.add (std::make_unique<APF> (PID { "amp_sustain", 1 }, "Amp Sustain", Range (0.0f, 1.0f, 0.001f), 0.8f));
    layout.add (std::make_unique<APF> (PID { "amp_release", 1 }, "Amp Release", time(), 0.25f,  sec()));

    // ---- LFOs ---------------------------------------------------------------
    for (int i = 0; i < numLfos; ++i)
    {
        const juce::String n = "LFO " + juce::String (i + 1) + " ";

        layout.add (std::make_unique<APF> (PID { lfoID (i, "rate"), 1 }, n + "Rate",
                                           skewed (0.02f, 30.0f, 2.0f), 2.0f, Attr().withLabel ("Hz")));
        layout.add (std::make_unique<APC> (PID { lfoID (i, "shape"), 1 }, n + "Shape",
                                           juce::StringArray { "Sine", "Tri", "Saw", "Square", "S&H" }, 0));
        layout.add (std::make_unique<APF> (PID { lfoID (i, "amount"), 1 }, n + "Amount",
                                           Range (-1.0f, 1.0f, 0.001f), 0.0f));
        layout.add (std::make_unique<APC> (PID { lfoID (i, "dest"), 1 }, n + "Destination",
                                           juce::StringArray { "Off", "Pitch", "Shape", "PW", "Cutoff", "Amp" },
                                           i == 0 ? destPitch : destOff));
    }

    // ---- Delay & master -----------------------------------------------------
    layout.add (std::make_unique<APF> (PID { "delay_time", 1 }, "Delay Time",
                                       skewed (10.0f, 1000.0f, 250.0f), 300.0f, Attr().withLabel ("ms")));
    layout.add (std::make_unique<APF> (PID { "delay_feedback", 1 }, "Delay Feedback",
                                       Range (0.0f, 0.95f, 0.001f), 0.35f));
    layout.add (std::make_unique<APF> (PID { "delay_tone", 1 }, "Delay Tone",
                                       skewed (200.0f, 16000.0f, 3000.0f), 6000.0f, Attr().withLabel ("Hz")));
    layout.add (std::make_unique<APF> (PID { "delay_mix", 1 }, "Delay Mix",
                                       Range (0.0f, 1.0f, 0.001f), 0.0f));

    // ---- Step sequencer -----------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterBool> (PID { "seq_on", 1 },   "Seq On",   false));
    layout.add (std::make_unique<juce::AudioParameterBool> (PID { "seq_gate", 1 }, "Seq Gate", true));
    layout.add (std::make_unique<APC> (PID { "seq_rate", 1 }, "Seq Rate",
                                       juce::StringArray { "1/4", "1/8", "1/16", "1/32" }, 2));
    layout.add (std::make_unique<APC> (PID { "seq_dest", 1 }, "Seq Destination",
                                       juce::StringArray { "Pitch", "Cutoff", "Pan" }, 0));

    // A musical starter pattern (in 1/12ths, so pitch lands on semitones: 7 = fifth, 12 = octave).
    static const float pattern[16] = { 0, 7, 4, 7,  0, 7, 12, 7,  0, 7, 4, 7,  0, 4, 7, 12 };
    for (int i = 0; i < 16; ++i)
        layout.add (std::make_unique<APF> (PID { "seq_step_" + juce::String (i + 1), 1 },
                                           "Seq Step " + juce::String (i + 1),
                                           Range (-1.0f, 1.0f, 0.001f), pattern[i] / 12.0f));

    layout.add (std::make_unique<APC> (PID { "skin", 1 }, "Skin",
                                       juce::StringArray { "Blue", "White" }, 0));

    layout.add (std::make_unique<APF> (PID { "master", 1 }, "Master",
                                       Range (-48.0f, 6.0f, 0.1f), -6.0f, Attr().withLabel ("dB")));

    return layout;
}

void bindPointers (juce::AudioProcessorValueTreeState& s, ParamPointers& p)
{
    auto get = [&s] (const juce::String& id)
    {
        auto* ptr = s.getRawParameterValue (id);
        jassert (ptr != nullptr);
        return ptr;
    };

    for (int i = 0; i < numOscs; ++i)
    {
        p.oscLevel[i] = get (oscID (i, "level"));
        p.oscSemi [i] = get (oscID (i, "semi"));
        p.oscFine [i] = get (oscID (i, "fine"));
        p.oscShape[i] = get (oscID (i, "shape"));
        if (i < 2)
            p.oscPW[i] = get (oscID (i, "pw"));
    }

    p.fm43      = get ("fm_4to3");
    p.fm34      = get ("fm_3to4");

    p.cutoff    = get ("cutoff");
    p.resonance = get ("resonance");
    p.poles     = get ("poles");
    p.filterEnv = get ("filter_env");
    p.keyTrack  = get ("key_track");
    p.drive     = get ("drive");

    p.fltA = get ("flt_attack");  p.fltD = get ("flt_decay");
    p.fltS = get ("flt_sustain"); p.fltR = get ("flt_release");
    p.ampA = get ("amp_attack");  p.ampD = get ("amp_decay");
    p.ampS = get ("amp_sustain"); p.ampR = get ("amp_release");

    for (int i = 0; i < numLfos; ++i)
    {
        p.lfoRate  [i] = get (lfoID (i, "rate"));
        p.lfoShape [i] = get (lfoID (i, "shape"));
        p.lfoAmount[i] = get (lfoID (i, "amount"));
        p.lfoDest  [i] = get (lfoID (i, "dest"));
    }

    p.delayTime     = get ("delay_time");
    p.delayFeedback = get ("delay_feedback");
    p.delayTone     = get ("delay_tone");
    p.delayMix      = get ("delay_mix");
    p.seqOn   = get ("seq_on");
    p.seqGate = get ("seq_gate");
    p.seqRate = get ("seq_rate");
    p.seqDest = get ("seq_dest");
    for (int i = 0; i < 16; ++i)
        p.seqStep[i] = get ("seq_step_" + juce::String (i + 1));

    p.master        = get ("master");
}
} // namespace evo
