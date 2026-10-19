#include "PluginEditor.h"

namespace ui
{
static const juce::Colour accent { 0xffe8a33d };
static const juce::Colour panel  { 0xff23272f };

// ---------------------------------------------------------------- Knob
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramID, const juce::String& labelText)
{
    label.setText (labelText, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);

    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 62, 16);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    addAndMakeVisible (slider);

    attachment = std::make_unique<SliderAttachment> (state, paramID, slider);

    if (auto* param = state.getParameter (paramID))
    {
        const juce::String suffix = param->getLabel().isEmpty() ? juce::String() : " " + param->getLabel();

        slider.textFromValueFunction = [suffix] (double v)
        {
            return (std::abs (v) >= 100.0 ? juce::String (juce::roundToInt (v)) : juce::String (v, 2)) + suffix;
        };
        slider.updateText();
    }
}

void Knob::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (16));
    slider.setBounds (area);
}

// -------------------------------------------------------------- Choice
Choice::Choice (juce::AudioProcessorValueTreeState& state, const juce::String& paramID, const juce::String& labelText)
{
    label.setText (labelText, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramID)))
        box.addItemList (choice->choices, 1);

    addAndMakeVisible (box);
    attachment = std::make_unique<ComboBoxAttachment> (state, paramID, box);
}

void Choice::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (16));
    box.setBounds (area.withSizeKeepingCentre (juce::jmax (10, area.getWidth() - 8), 24));
}

// ------------------------------------------------------------- Section
Section::Section (juce::AudioProcessorValueTreeState& s, const juce::String& titleText)
    : state (s), title (titleText) {}

void Section::addKnob (const juce::String& paramID, const juce::String& label)
{
    auto* k = items.add (new Knob (state, paramID, label));
    addAndMakeVisible (k);
}

void Section::addChoice (const juce::String& paramID, const juce::String& label)
{
    auto* c = items.add (new Choice (state, paramID, label));
    addAndMakeVisible (c);
}

void Section::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    g.setColour (accent);
    g.setFont (13.0f);
    g.drawText (title.toUpperCase(), getLocalBounds().reduced (10, 6).removeFromTop (16),
                juce::Justification::centredLeft);
}

void Section::resized()
{
    auto area = getLocalBounds().reduced (6);
    area.removeFromTop (20);

    if (items.isEmpty())
        return;

    const int w = area.getWidth() / items.size();

    for (auto* item : items)
        item->setBounds (area.removeFromLeft (w));
}
} // namespace ui

// =================================================================== Editor
PolyEvolverAudioProcessorEditor::PolyEvolverAudioProcessorEditor (PolyEvolverAudioProcessor& p)
    : AudioProcessorEditor (&p),
      proc (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    laf.setColourScheme (juce::LookAndFeel_V4::getMidnightColourScheme());
    setLookAndFeel (&laf);

    // ---- Oscillators ----
    for (int i = 0; i < evo::numOscs; ++i)
    {
        const bool analog = i < 2;
        auto& sec = addSection (oscRow, juce::String (analog ? "Analog Osc " : "Digital Osc ") + juce::String (i + 1));

        sec.addKnob (evo::oscID (i, "level"), "Level");
        sec.addKnob (evo::oscID (i, "semi"),  "Semi");
        sec.addKnob (evo::oscID (i, "fine"),  "Fine");
        sec.addKnob (evo::oscID (i, "shape"), analog ? "Shape" : "Wave");
        if (analog)
            sec.addKnob (evo::oscID (i, "pw"), "PW");
    }

    // ---- Filter, envelopes, delay, master ----
    {
        auto& f = addSection (filterRow, "Filter");
        f.addKnob ("cutoff", "Cutoff");
        f.addKnob ("resonance", "Reso");
        f.addChoice ("poles", "Poles");
        f.addKnob ("filter_env", "Env Amt");
        f.addKnob ("key_track", "Key Trk");
        f.addKnob ("drive", "Drive");

        auto& fe = addSection (filterRow, "Filter Envelope");
        fe.addKnob ("flt_attack", "A");  fe.addKnob ("flt_decay", "D");
        fe.addKnob ("flt_sustain", "S"); fe.addKnob ("flt_release", "R");

        auto& ae = addSection (filterRow, "Amp Envelope");
        ae.addKnob ("amp_attack", "A");  ae.addKnob ("amp_decay", "D");
        ae.addKnob ("amp_sustain", "S"); ae.addKnob ("amp_release", "R");

        auto& d = addSection (filterRow, "Delay");
        d.addKnob ("delay_time", "Time");  d.addKnob ("delay_feedback", "Fdbk");
        d.addKnob ("delay_tone", "Tone");  d.addKnob ("delay_mix", "Mix");

        auto& m = addSection (filterRow, "Master");
        m.addKnob ("master", "Vol");
    }

    // ---- LFOs ----
    for (int i = 0; i < evo::numLfos; ++i)
    {
        auto& l = addSection (lfoRow, "LFO " + juce::String (i + 1));
        l.addKnob   (evo::lfoID (i, "rate"),   "Rate");
        l.addChoice (evo::lfoID (i, "shape"),  "Shape");
        l.addKnob   (evo::lfoID (i, "amount"), "Amount");
        l.addChoice (evo::lfoID (i, "dest"),   "Dest");
    }

    keyboard.setAvailableRange (24, 96);
    keyboard.setOctaveForMiddleC (4);
    addAndMakeVisible (keyboard);

    setSize (1240, 640);
}

PolyEvolverAudioProcessorEditor::~PolyEvolverAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

ui::Section& PolyEvolverAudioProcessorEditor::addSection (juce::OwnedArray<ui::Section>& row, const juce::String& title)
{
    auto* sec = row.add (new ui::Section (proc.apvts, title));
    addAndMakeVisible (sec);
    return *sec;
}

void PolyEvolverAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff15171c));

    g.setColour (ui::accent);
    g.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
    g.drawText ("POLY EVOLVER EMU", 16, 6, 400, 28, juce::Justification::centredLeft);
}

void PolyEvolverAudioProcessorEditor::layoutRow (juce::Rectangle<int> area, const juce::OwnedArray<ui::Section>& row)
{
    int totalItems = 0;
    for (auto* s : row)
        totalItems += s->getNumItems();

    if (totalItems == 0)
        return;

    const int totalWidth = area.getWidth();

    for (auto* s : row)
    {
        const int w = totalWidth * s->getNumItems() / totalItems;
        s->setBounds (area.removeFromLeft (w));
    }
}

void PolyEvolverAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (8);
    area.removeFromTop (30);                       // title strip

    keyboard.setBounds (area.removeFromBottom (80));
    area.removeFromBottom (6);

    const int rowH = area.getHeight() / 3;
    layoutRow (area.removeFromTop (rowH), oscRow);
    layoutRow (area.removeFromTop (rowH), filterRow);
    layoutRow (area, lfoRow);
}
