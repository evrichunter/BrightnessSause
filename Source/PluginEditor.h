#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// Evric Hunter visual identity: carbon black, dark surfaces, matte gold.
namespace ui
{
    const juce::Colour bgTop { 0xff131313 }, bgBottom { 0xff0a0a0a }, surface { 0xff1b1b1b }, surfaceHi { 0xff262626 },
                       line { 0xff303030 }, gold { 0xffc9a24b }, goldHi { 0xffe0bd6a }, goldDim { 0xff6f5a2c },
                       text { 0xffeee9dd }, textDim { 0xffa8a294 }, clip { 0xffd0553f };
    juce::Font font (float height, bool bold = false);
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Font getLabelFont (juce::Label&) override;
};

// Knob with a title above and an editable numeric value below (double-click = default).
class KnobComp : public juce::Component
{
public:
    enum Size { Large, Medium, Small };
    KnobComp (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& title, Size = Medium);
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    juce::Slider slider;
    juce::String title;
    Size size;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// Horizontal L/R peak meter fed from the processor's atomics. Click to reset the clip latch.
class MeterComp : public juce::Component
{
public:
    MeterComp (std::atomic<float>* peaks, const juce::String& name) : src (peaks), title (name) {}
    void tick();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { clip[0] = clip[1] = false; repaint(); }
private:
    std::atomic<float>* src;
    juce::String title;
    float level[2] {}, hold[2] {};
    bool clip[2] {};
};

class BrightnessSauseEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BrightnessSauseEditor (BrightnessSauseProcessor&);
    ~BrightnessSauseEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void applyMoreState();

    BrightnessSauseProcessor& proc;
    LookAndFeel laf;
    MeterComp inMeter, outMeter;
    KnobComp excite, air, brightness;
    KnobComp mix, output, lowcut, presGain, airFreq;
    KnobComp character, harmonics, presAmt, softness, presFreq, airGain, qKnob;
    KnobComp dsFreq, dsRange, dsSens;
    juce::ToggleButton bypassBtn { "BYPASS" }, eqBypassBtn { "EQ BYPASS" }, deessBtn { "DE-ESS" },
                       listenBtn { "LISTEN" }, moreBtn { "MORE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt, eqBypassAtt, deessAtt, listenAtt;
    float grShown = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrightnessSauseEditor)
};
