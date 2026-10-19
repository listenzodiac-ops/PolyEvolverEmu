#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

namespace ui
{
using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

// A labelled rotary knob bound to a parameter.
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramID, const juce::String& labelText);
    void resized() override;

private:
    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label label;
    std::unique_ptr<SliderAttachment> attachment;
};

// A labelled drop-down bound to a choice parameter.
class Choice : public juce::Component
{
public:
    Choice (juce::AudioProcessorValueTreeState& state, const juce::String& paramID, const juce::String& labelText);
    void resized() override;

private:
    juce::ComboBox box;
    juce::Label label;
    std::unique_ptr<ComboBoxAttachment> attachment;
};

// A titled panel holding a row of knobs / drop-downs.
class Section : public juce::Component
{
public:
    Section (juce::AudioProcessorValueTreeState& s, const juce::String& titleText);

    void addKnob   (const juce::String& paramID, const juce::String& label);
    void addChoice (const juce::String& paramID, const juce::String& label);

    int getNumItems() const { return items.size(); }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& state;
    juce::String title;
    juce::OwnedArray<juce::Component> items;
};
} // namespace ui

class PolyEvolverAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit PolyEvolverAudioProcessorEditor (PolyEvolverAudioProcessor&);
    ~PolyEvolverAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ui::Section& addSection (juce::OwnedArray<ui::Section>& row, const juce::String& title);
    static void layoutRow (juce::Rectangle<int> area, const juce::OwnedArray<ui::Section>& row);

    PolyEvolverAudioProcessor& proc;
    juce::LookAndFeel_V4 laf;

    juce::OwnedArray<ui::Section> oscRow, filterRow, lfoRow;
    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PolyEvolverAudioProcessorEditor)
};
