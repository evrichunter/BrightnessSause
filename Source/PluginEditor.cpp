#include "PluginEditor.h"

namespace
{
    constexpr int kW = 840, kH = 520, kHMore = 852, kM = 32;
}

juce::Font ui::font (float h, bool bold)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}

// ------------------------------------------------------------ look and feel
LookAndFeel::LookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, ui::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, ui::goldDim);
    setColour (juce::Label::textColourId, ui::text);
    setColour (juce::TextEditor::backgroundColourId, ui::surfaceHi);
    setColour (juce::TextEditor::textColourId, ui::text);
    setColour (juce::TextEditor::outlineColourId, ui::gold);
    setColour (juce::TextEditor::focusedOutlineColourId, ui::gold);
    setColour (juce::CaretComponent::caretColourId, ui::gold);
}

juce::Label* LookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = juce::LookAndFeel_V4::createSliderTextBox (s);
    l->getProperties().set ("fs", s.getProperties().getWithDefault ("fs", 15.0f));
    l->setJustificationType (juce::Justification::centred);
    return l;
}

juce::Font LookAndFeel::getLabelFont (juce::Label& l)
{
    return ui::font ((float) (double) l.getProperties().getWithDefault ("fs", 15.0f), true);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float d = juce::jmin (area.getWidth(), area.getHeight()) - 6.0f;
    auto b = juce::Rectangle<float> (d, d).withCentre (area.getCentre());
    const float ang = a0 + pos * (a1 - a0);
    const bool active = s.isMouseOverOrDragging();
    const float arcW = juce::jmax (2.5f, d * 0.04f);

    // value arc and track
    auto arcB = b.reduced (arcW * 0.5f);
    juce::Path track, val;
    track.addCentredArc (arcB.getCentreX(), arcB.getCentreY(), arcB.getWidth() / 2, arcB.getHeight() / 2, 0, a0, a1, true);
    val.addCentredArc (arcB.getCentreX(), arcB.getCentreY(), arcB.getWidth() / 2, arcB.getHeight() / 2, 0, a0, ang, true);
    g.setColour (ui::line);
    g.strokePath (track, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    if (active) // subtle glow while hovered/dragged
    {
        g.setColour (ui::gold.withAlpha (0.18f));
        g.strokePath (val, juce::PathStrokeType (arcW + 5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (active ? ui::goldHi : ui::gold);
    g.strokePath (val, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // body
    auto body = b.reduced (arcW + d * 0.07f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (body.translated (0, d * 0.025f).expanded (1.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff343434), body.getX(), body.getY(),
                                             juce::Colour (0xff161616), body.getRight(), body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (active ? ui::goldDim : juce::Colour (0xff3d3d3d));
    g.drawEllipse (body, 1.2f);
    g.setColour (juce::Colour (0xff0c0c0c));
    g.drawEllipse (body.reduced (d * 0.06f), 1.0f);

    // position indicator (line + end cap)
    const auto c = body.getCentre();
    const float rad = body.getWidth() * 0.5f;
    const float sn = std::sin (ang), cs = -std::cos (ang);
    g.setColour (active ? ui::goldHi : ui::gold);
    g.drawLine (c.x + sn * rad * 0.42f, c.y + cs * rad * 0.42f, c.x + sn * rad * 0.86f, c.y + cs * rad * 0.86f,
                juce::jmax (2.5f, d * 0.035f));
    g.fillEllipse (juce::Rectangle<float> (juce::jmax (4.0f, d * 0.045f), juce::jmax (4.0f, d * 0.045f))
                       .withCentre ({ c.x + sn * rad * 0.86f, c.y + cs * rad * 0.86f }));
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hi, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const float rad = 5.0f;
    if (on) { g.setColour (hi ? ui::goldHi : ui::gold); g.fillRoundedRectangle (r, rad); }
    else
    {
        g.setColour (hi ? ui::surfaceHi : ui::surface);
        g.fillRoundedRectangle (r, rad);
        g.setColour (hi ? ui::gold : juce::Colour (0xff4a4a4a));
        g.drawRoundedRectangle (r, rad, 1.2f);
    }
    g.setColour (on ? juce::Colour (0xff111111) : ui::text);
    g.setFont (ui::font (13.0f, true).withExtraKerningFactor (0.06f));
    g.drawText (b.getButtonText(), r.toNearestInt(), juce::Justification::centred);
}

// ------------------------------------------------------------ knob
KnobComp::KnobComp (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& t, Size sz)
    : title (t), size (sz)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.getProperties().set ("fs", sz == Large ? 19.0f : sz == Medium ? 16.0f : 15.0f);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 110, sz == Large ? 28 : 24);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    slider.setMouseDragSensitivity (200);
    addAndMakeVisible (slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    slider.setTextBoxIsEditable (true);
}
void KnobComp::resized() { slider.setBounds (getLocalBounds().withTrimmedTop (size == Large ? 28 : 24)); }
void KnobComp::paint (juce::Graphics& g)
{
    g.setColour (size == Large ? ui::text : ui::textDim);
    g.setFont (ui::font (size == Large ? 14.0f : 12.5f, true).withExtraKerningFactor (0.1f));
    g.drawText (title, getLocalBounds().removeFromTop (size == Large ? 26 : 22), juce::Justification::centred);
}

// ------------------------------------------------------------ meters
void MeterComp::tick()
{
    for (int c = 0; c < 2; ++c)
    {
        const float pk = src[c].exchange (0.0f);
        const float db = juce::Decibels::gainToDecibels (pk, -60.0f);
        const float v = juce::jmap (juce::jlimit (-60.0f, 0.0f, db), -60.0f, 0.0f, 0.0f, 1.0f);
        level[c] = v > level[c] ? v : level[c] * 0.86f + v * 0.14f;
        hold[c] = v > hold[c] ? v : juce::jmax (0.0f, hold[c] - 0.004f);
        if (pk >= 1.0f) clip[c] = true;
    }
    repaint();
}

void MeterComp::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (ui::textDim);
    g.setFont (ui::font (12.5f, true).withExtraKerningFactor (0.1f));
    g.drawText (title, r.removeFromLeft (38.0f).toNearestInt(), juce::Justification::centredLeft);
    const float bh = 6.0f, gap = 6.0f;
    float y = r.getCentreY() - bh - gap * 0.5f;
    for (int c = 0; c < 2; ++c, y += bh + gap)
    {
        auto bar = juce::Rectangle<float> (r.getX(), y, r.getWidth() - 12.0f, bh);
        g.setColour (ui::surface);
        g.fillRoundedRectangle (bar, 3.0f);
        auto fill = bar.withWidth (bar.getWidth() * level[c]);
        g.setColour (level[c] > 0.97f ? ui::clip : ui::gold);
        g.fillRoundedRectangle (fill, 3.0f);
        if (hold[c] > 0.02f) // peak-hold tick
        {
            g.setColour (ui::text);
            g.fillRect (bar.getX() + bar.getWidth() * hold[c] - 1.0f, bar.getY() - 1.0f, 2.0f, bh + 2.0f);
        }
        g.setColour (clip[c] ? ui::clip : ui::line); // clip lamp
        g.fillEllipse (bar.getRight() + 3.0f, bar.getCentreY() - 3.5f, 7.0f, 7.0f);
    }
}

// ------------------------------------------------------------ editor
BrightnessSauseEditor::BrightnessSauseEditor (BrightnessSauseProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      inMeter (p.inPeak, "IN"), outMeter (p.outPeak, "OUT"),
      excite (p.apvts, "excite", "EXCITE", KnobComp::Large), air (p.apvts, "air", "AIR", KnobComp::Large),
      brightness (p.apvts, "brightness", "BRIGHTNESS", KnobComp::Large),
      mix (p.apvts, "mix", "MIX"), output (p.apvts, "output", "OUTPUT"), lowcut (p.apvts, "lowcut", "LOW CUT"),
      presGain (p.apvts, "presgain", "PRESENCE"), airFreq (p.apvts, "airfreq", "AIR FREQ"),
      character (p.apvts, "character", "CHARACTER", KnobComp::Small), harmonics (p.apvts, "harmonics", "HARMONICS", KnobComp::Small),
      presAmt (p.apvts, "presence", "PRES. AMOUNT", KnobComp::Small), softness (p.apvts, "softness", "SOFTNESS", KnobComp::Small),
      presFreq (p.apvts, "presfreq", "PRES. FREQ", KnobComp::Small), airGain (p.apvts, "airgain", "AIR GAIN", KnobComp::Small),
      qKnob (p.apvts, "q", "Q", KnobComp::Small),
      dsFreq (p.apvts, "deessfreq", "FREQUENCY", KnobComp::Small), dsRange (p.apvts, "deessrange", "RANGE", KnobComp::Small),
      dsSens (p.apvts, "deesssens", "SENSITIVITY", KnobComp::Small)
{
    setLookAndFeel (&laf);
    for (auto* c : std::initializer_list<juce::Component*> { &inMeter, &outMeter, &excite, &air, &brightness, &mix, &output, &lowcut,
                     &presGain, &airFreq, &character, &harmonics, &presAmt, &softness, &presFreq, &airGain, &qKnob,
                     &dsFreq, &dsRange, &dsSens, &bypassBtn, &eqBypassBtn, &deessBtn, &listenBtn, &moreBtn })
        addAndMakeVisible (*c);

    sendLookAndFeelChange(); // rebuild slider text boxes with the plugin look and feel

    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "bypass", bypassBtn);
    eqBypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "eqbypass", eqBypassBtn);
    deessAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "deess", deessBtn);
    listenAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "deesslisten", listenBtn);

    moreBtn.setToggleState ((bool) p.apvts.state.getProperty ("more", false), juce::dontSendNotification);
    moreBtn.onClick = [this] { proc.apvts.state.setProperty ("more", moreBtn.getToggleState(), nullptr); applyMoreState(); };
    setSize (kW, moreBtn.getToggleState() ? kHMore : kH);
    resized();
    startTimerHz (30); // meters only; nothing here touches the audio thread
}

BrightnessSauseEditor::~BrightnessSauseEditor() { stopTimer(); setLookAndFeel (nullptr); }

void BrightnessSauseEditor::applyMoreState() { setSize (kW, moreBtn.getToggleState() ? kHMore : kH); }

void BrightnessSauseEditor::timerCallback()
{
    inMeter.tick();
    outMeter.tick();
    const float gr = proc.deessReduction.load (std::memory_order_relaxed);
    grShown = gr > grShown ? gr : grShown * 0.85f + gr * 0.15f;
    repaint (juce::Rectangle<int> (kW - kM - 160, 410, 160, 50));
}

void BrightnessSauseEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (ui::bgTop, 0, 0, ui::bgBottom, 0, (float) getHeight(), false));
    g.fillAll();

    // brand
    g.setColour (ui::text);
    g.setFont (ui::font (23.0f, true).withExtraKerningFactor (0.06f));
    g.drawText ("BRIGHTNESS SAUSE", juce::Rectangle<int> (kM, 22, 380, 32), juce::Justification::centredLeft);
    g.setColour (ui::gold);
    g.setFont (ui::font (13.0f, true).withExtraKerningFactor (0.28f));
    g.drawText ("BY EVRIC HUNTER", juce::Rectangle<int> (kM, 54, 300, 18), juce::Justification::centredLeft);

    // hairlines
    g.setColour (ui::gold.withAlpha (0.35f));
    g.fillRect (kM, 88, kW - 2 * kM, 1);
    g.fillRect (kM, 308, kW - 2 * kM, 1);

    // de-esser reduction readout (under the DE-ESS switch)
    {
        auto bar = juce::Rectangle<float> ((float) kW - kM - 150.0f, 426.0f, 150.0f, 5.0f);
        g.setColour (ui::surface);
        g.fillRoundedRectangle (bar, 2.5f);
        g.setColour (ui::gold);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, grShown / 6.0f)), 2.5f);
        g.setColour (ui::textDim);
        g.setFont (ui::font (12.5f, true).withExtraKerningFactor (0.06f));
        g.drawText ("REDUCTION  " + (grShown > 0.05f ? "-" + juce::String (grShown, 1) : juce::String ("0.0")) + " dB", juce::Rectangle<int> (kW - kM - 150, 436, 150, 18), juce::Justification::centredLeft);
    }

    if (getHeight() > kH)
    {
        g.setColour (ui::surface.withAlpha (0.55f));
        g.fillRoundedRectangle (juce::Rectangle<float> ((float) kM - 12.0f, 514.0f, (float) kW - 2 * kM + 24.0f, 326.0f), 8.0f);
        g.setColour (ui::gold);
        g.setFont (ui::font (12.5f, true).withExtraKerningFactor (0.28f));
        g.drawText ("TONE", juce::Rectangle<int> (kM, 524, 200, 18), juce::Justification::centredLeft);
        g.drawText ("DE-ESSER", juce::Rectangle<int> (kM, 678, 200, 18), juce::Justification::centredLeft);
        g.setColour (ui::line);
        g.fillRect (kM, 674, kW - 2 * kM, 1);
    }
}

void BrightnessSauseEditor::resized()
{
    // header
    bypassBtn.setBounds (kW - kM - 96, 32, 96, 32);
    outMeter.setBounds (kW - kM - 96 - 28 - 150, 28, 150, 40);
    inMeter.setBounds (kW - kM - 96 - 28 - 150 - 24 - 150, 28, 150, 40);

    // primary controls
    const int cell = (kW - 2 * kM) / 3;
    int i = 0;
    for (auto* k : { &excite, &air, &brightness })
    { k->setBounds (kM + i * cell + (cell - 210) / 2, 100, 210, 202); ++i; }

    // secondary row
    const int y = 322, w = 112;
    int x = kM;
    for (auto* k : { &mix, &output, &lowcut, &presGain, &airFreq }) { k->setBounds (x, y, w, 136); x += w + 8; }
    eqBypassBtn.setBounds (kW - kM - 150, 334, 150, 32);
    deessBtn.setBounds (kW - kM - 150, 376, 150, 32);

    // MORE
    moreBtn.setBounds ((kW - 120) / 2, 470, 120, 30);
    const bool more = getHeight() > kH;
    int mx = kM;
    for (auto* k : { &character, &harmonics, &presAmt, &softness, &presFreq, &airGain, &qKnob })
    { k->setBounds (mx, 548, 108, 118); mx += 111; k->setVisible (more); }
    mx = kM;
    for (auto* k : { &dsFreq, &dsRange, &dsSens }) { k->setBounds (mx, 702, 108, 118); mx += 111; k->setVisible (more); }
    listenBtn.setBounds (mx + 30, 740, 130, 32);
    listenBtn.setVisible (more);
}
