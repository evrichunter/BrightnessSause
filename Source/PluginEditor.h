#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace ice
{
    const juce::Colour bgTop   { 0xff7a1426 }, bgBottom { 0xff2a0610 }, coral { 0xffff6b57 }, coralDark { 0xffc2362f },
                       outline { 0xff0b0204 }, white { 0xfff2fdff }, cyan { 0xffa4ecff }, cyanDeep { 0xff4fc3e8 },
                       panel   { 0xff3a0a14 };
    juce::Font condensed (float height, bool bold = true);
}

// Chunky tactile knobs, pill buttons
class IceLookAndFeel : public juce::LookAndFeel_V4
{
public:
    IceLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Font getLabelFont (juce::Label&) override { return ice::condensed (14.0f, false); }
};

class KnobComp : public juce::Component
{
public:
    KnobComp (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& title, bool large = false);
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    juce::Slider slider;
    juce::String title;
    bool large;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// Peak meter pair (L/R) fed from an atomic pair; click to reset the clip latch.
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

// Animated frozen-liquid stream, falling crystals and ice cubes. GUI thread only.
class IceStream : public juce::Component
{
public:
    IceStream();
    void setAnimating (bool on) { animating = on; repaint(); }
    void advance (float dt);
    void paint (juce::Graphics&) override;
private:
    struct Particle { float x, y, vy, vx, size, rot, vr, alpha; int kind; };
    void respawn (Particle&, bool anywhere);
    std::vector<Particle> parts;
    juce::Random rng;
    float phase = 0.0f;
    bool animating = true;
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
    IceLookAndFeel laf;
    IceStream stream;
    MeterComp inMeter, outMeter;
    KnobComp excite, air, brightness, mix, output, lowcut, presGain, airFreq;
    KnobComp character, harmonics, presAmt, softness, presFreq, airGain, qKnob;
    juce::ToggleButton bypassBtn { "BYPASS" }, eqBypassBtn { "EQ BYPASS" }, animBtn { "ICE FX" }, moreBtn { "MORE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt, eqBypassAtt;
    int lastTick = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrightnessSauseEditor)
};
