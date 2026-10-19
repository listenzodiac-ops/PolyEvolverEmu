#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

namespace ui
{
using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;

inline const juce::Colour accent    { 0xff4db8ff };   // electric blue
inline const juce::Colour accentHot { 0xffcdeeff };   // white-hot core of the lights

// Everything that changes between the two skins.
struct Theme
{
    juce::Colour plateTop, plateBottom, plateGlow;
    float vignette = 0.6f;
    juce::Colour panelTop, panelBottom, panelBorder, panelBevel;
    juce::Colour text, textDim, tick;
    juce::Colour logoTop, logoBottom, subtitle;
    float knobShadow = 0.8f;
    float bloom = 1.0f;
};

const Theme& theme();
void setSkin (int index);   // 0 = blue, 1 = white
int  getSkin();

// Hardware-style look: machined black knobs with a white pointer, recessed LCD-style drop-downs,
// and key-style toggle buttons.
class EvolverLookAndFeel : public juce::LookAndFeel_V4
{
public:
    EvolverLookAndFeel();
    void applyTheme();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
};

// A slider whose dragging can snap to detents (used so the sequencer's pitch steps click into semitones).
class SnapSlider : public juce::Slider
{
public:
    SnapSlider() : juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox) {}

    std::function<double (double)> snapper;

    double snapValue (double attemptedValue, DragMode) override
    {
        return snapper ? snapper (attemptedValue) : attemptedValue;
    }
};

// A labelled hardware knob bound to a parameter.
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramID, const juce::String& labelText);

    SnapSlider& getSlider() { return slider; }
    void setActive (bool shouldBeActive);   // sequencer: marks the step that is playing

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    SnapSlider slider;
    juce::String labelText;
    bool active = false;
    std::unique_ptr<SliderAttachment> attachment;
};

// A labelled drop-down bound to a choice parameter.
class Choice : public juce::Component
{
public:
    Choice (juce::AudioProcessorValueTreeState& state, const juce::String& paramID, const juce::String& labelText);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::ComboBox box;
    juce::String labelText;
    std::unique_ptr<ComboBoxAttachment> attachment;
};

// A labelled on/off key bound to a bool parameter.
class Switch : public juce::Component
{
public:
    Switch (juce::AudioProcessorValueTreeState& state, const juce::String& paramID, const juce::String& labelText);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::ToggleButton button;
    juce::String labelText;
    std::unique_ptr<ButtonAttachment> attachment;
};

// LFO indicator light: a tiny glass dome over an LED. It pulses at the LFO's rate, following the
// LFO's shape and amount, and fades in and out like a real lamp rather than switching.
class LfoLed : public juce::Component
{
public:
    LfoLed (juce::AudioProcessorValueTreeState& state, int lfoIndex);

    void tick (float dtSeconds);
    void paint (juce::Graphics&) override;

private:
    std::atomic<float>* rate   = nullptr;
    std::atomic<float>* shape  = nullptr;
    std::atomic<float>* amount = nullptr;
    std::atomic<float>* dest   = nullptr;

    float phase = 0.0f, held = 0.0f, level = 0.04f, painted = -1.0f;
    juce::Random rng;
};

// Two round swatches (blue / white) in the header for choosing the skin.
class SkinSwitch : public juce::Component
{
public:
    std::function<void (int)> onPick;

    void setCurrent (int index) { current = index; repaint(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    int current = 0;
};

// A titled, silk-screened panel group holding a row of controls.
class Section : public juce::Component
{
public:
    Section (juce::AudioProcessorValueTreeState& s, const juce::String& titleText);

    Knob* addKnob  (const juce::String& paramID, const juce::String& label);
    void addChoice (const juce::String& paramID, const juce::String& label);
    void addSwitch (const juce::String& paramID, const juce::String& label);

    int  getNumItems() const { return items.size(); }
    void setReservesLedSpace (bool b) { hasLed = b; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& state;
    juce::String title;
    bool hasLed = false;
    juce::OwnedArray<juce::Component> items;
};
} // namespace ui

class PolyEvolverAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit PolyEvolverAudioProcessorEditor (PolyEvolverAudioProcessor&);
    ~PolyEvolverAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int numRows = 4;

    void timerCallback() override;
    void applySkin (int index);
    ui::Section& addSection (int row, const juce::String& title);
    static void layoutRow (juce::Rectangle<int> area, const juce::OwnedArray<ui::Section>& row, int gap);

    PolyEvolverAudioProcessor& proc;
    ui::EvolverLookAndFeel laf;

    juce::OwnedArray<ui::Section> rows[numRows];
    ui::Section* lfoSections[evo::numLfos] {};
    ui::Knob* stepKnobs[16] {};
    juce::OwnedArray<ui::LfoLed> leds;

    ui::SkinSwitch skinSwitch;
    juce::ParameterAttachment skinAttach;

    double lastTick = 0.0;
    int shownStep = -2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PolyEvolverAudioProcessorEditor)
};
