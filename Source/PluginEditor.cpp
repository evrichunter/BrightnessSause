#include "PluginEditor.h"

juce::Font ice::condensed (float h, bool bold)
{
    return juce::Font (juce::FontOptions ("Impact", h, bold ? juce::Font::bold : juce::Font::plain)).withHorizontalScale (bold ? 0.9f : 0.85f);
}

// ------------------------------------------------------------ look and feel
IceLookAndFeel::IceLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, ice::white);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, ice::white);
}

void IceLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float d = juce::jmin (r.getWidth(), r.getHeight());
    auto b = juce::Rectangle<float> (d, d).withCentre (r.getCentre());
    const float ang = a0 + pos * (a1 - a0);
    const float lw = juce::jmax (3.0f, d * 0.07f);

    // outer value ring
    juce::Path track, val;
    auto ringB = b.reduced (lw * 0.5f);
    track.addCentredArc (ringB.getCentreX(), ringB.getCentreY(), ringB.getWidth() / 2, ringB.getHeight() / 2, 0, a0, a1, true);
    val.addCentredArc (ringB.getCentreX(), ringB.getCentreY(), ringB.getWidth() / 2, ringB.getHeight() / 2, 0, a0, ang, true);
    g.setColour (ice::outline);
    g.strokePath (track, juce::PathStrokeType (lw + 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (ice::panel.brighter (0.2f));
    g.strokePath (track, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (s.isEnabled() ? ice::cyan : ice::cyan.withAlpha (0.3f));
    g.strokePath (val, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // knob body
    auto body = b.reduced (lw * 2.0f + 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (body.translated (0, 3.0f));
    g.setGradientFill (juce::ColourGradient (ice::coral.brighter (0.25f), body.getX() + body.getWidth() * 0.3f, body.getY() + body.getHeight() * 0.2f,
                                             ice::coralDark.darker (0.3f), body.getRight(), body.getBottom(), true));
    g.fillEllipse (body);
    g.setColour (ice::outline);
    g.drawEllipse (body, juce::jmax (2.5f, d * 0.03f));

    // frosty highlight
    juce::Path hl;
    hl.addCentredArc (body.getCentreX(), body.getCentreY(), body.getWidth() * 0.38f, body.getHeight() * 0.38f, 0,
                      -juce::MathConstants<float>::pi * 0.85f, -juce::MathConstants<float>::pi * 0.35f, true);
    g.setColour (ice::white.withAlpha (0.55f));
    g.strokePath (hl, juce::PathStrokeType (juce::jmax (2.0f, d * 0.035f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // pointer
    const float rad = body.getWidth() * 0.5f;
    auto c = body.getCentre();
    juce::Line<float> ln (c.x + std::sin (ang) * rad * 0.35f, c.y - std::cos (ang) * rad * 0.35f,
                          c.x + std::sin (ang) * rad * 0.82f, c.y - std::cos (ang) * rad * 0.82f);
    g.setColour (ice::outline);
    g.drawLine (ln, juce::jmax (5.0f, d * 0.06f) + 2.0f);
    g.setColour (ice::white);
    g.drawLine (ln, juce::jmax (3.0f, d * 0.045f));
}

void IceLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hi, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();
    g.setColour (ice::outline);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    auto in = r.reduced (2.0f);
    g.setColour (on ? ice::cyan : (hi ? ice::coral.brighter (0.2f) : ice::coral));
    g.fillRoundedRectangle (in, in.getHeight() * 0.5f);
    g.setColour (on ? ice::outline : ice::white);
    g.setFont (ice::condensed (juce::jmin (16.0f, in.getHeight() * 0.7f)));
    g.drawText (b.getButtonText(), in.toNearestInt(), juce::Justification::centred);
}

// ------------------------------------------------------------ knob
KnobComp::KnobComp (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& t, bool big)
    : title (t), large (big)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, big ? 100 : 80, big ? 22 : 18);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    slider.setMouseDragSensitivity (180);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    addAndMakeVisible (slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    slider.setTextBoxIsEditable (true);
}
void KnobComp::resized() { slider.setBounds (getLocalBounds().withTrimmedTop (large ? 26 : 20)); }
void KnobComp::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().removeFromTop (large ? 26 : 20);
    g.setFont (ice::condensed (large ? 24.0f : 16.0f));
    g.setColour (ice::outline);
    g.drawText (title, r.translated (0, 2), juce::Justification::centred);
    g.setColour (ice::white);
    g.drawText (title, r, juce::Justification::centred);
}

// ------------------------------------------------------------ meters
void MeterComp::tick()
{
    for (int c = 0; c < 2; ++c)
    {
        const float pk = src[c].exchange (0.0f);
        const float db = juce::Decibels::gainToDecibels (pk, -60.0f);
        const float v = juce::jmap (juce::jlimit (-60.0f, 0.0f, db), -60.0f, 0.0f, 0.0f, 1.0f);
        level[c] = v > level[c] ? v : level[c] * 0.88f + v * 0.12f;
        if (v > hold[c]) hold[c] = v; else hold[c] = juce::jmax (0.0f, hold[c] - 0.004f);
        if (pk >= 1.0f) clip[c] = true;
    }
    repaint();
}
void MeterComp::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto lab = r.removeFromTop (14.0f);
    g.setColour (ice::white);
    g.setFont (ice::condensed (13.0f));
    g.drawText (title, lab, juce::Justification::centred);
    const float gap = 4.0f, bw = (r.getWidth() - gap) / 2;
    for (int c = 0; c < 2; ++c)
    {
        auto bar = juce::Rectangle<float> (r.getX() + c * (bw + gap), r.getY(), bw, r.getHeight());
        g.setColour (ice::outline);
        g.fillRoundedRectangle (bar, 3.0f);
        auto in = bar.reduced (2.0f);
        g.setColour (ice::panel);
        g.fillRoundedRectangle (in, 2.0f);
        auto fill = in.withTrimmedTop (in.getHeight() * (1.0f - level[c]));
        g.setGradientFill (juce::ColourGradient (ice::coral, fill.getX(), in.getY(), ice::cyan, fill.getX(), in.getBottom(), false));
        g.fillRoundedRectangle (fill, 2.0f);
        g.setColour (ice::white);
        g.fillRect (in.getX(), in.getBottom() - in.getHeight() * hold[c] - 1.0f, in.getWidth(), 2.0f);
        if (clip[c]) { g.setColour (juce::Colours::red); g.fillRoundedRectangle (bar.removeFromTop (6.0f).reduced (1.0f), 2.0f); }
    }
}

// ------------------------------------------------------------ ice stream
IceStream::IceStream()
{
    setInterceptsMouseClicks (false, false); // never steals knob interaction
    setOpaque (false);
    parts.resize (46);
    for (auto& p : parts) respawn (p, true);
}

void IceStream::respawn (Particle& p, bool anywhere)
{
    const float cx = getWidth() * 0.5f;
    p.x = cx + (rng.nextFloat() - 0.5f) * (anywhere ? (float) getWidth() * 0.9f : 46.0f);
    p.y = anywhere ? rng.nextFloat() * (float) juce::jmax (1, getHeight()) : 70.0f + rng.nextFloat() * 20.0f;
    p.vy = 40.0f + rng.nextFloat() * 110.0f;
    p.vx = (rng.nextFloat() - 0.5f) * 30.0f;
    p.size = 4.0f + rng.nextFloat() * 10.0f;
    p.rot = rng.nextFloat() * 6.28f;
    p.vr = (rng.nextFloat() - 0.5f) * 3.0f;
    p.alpha = 0.35f + rng.nextFloat() * 0.5f;
    p.kind = rng.nextInt (3);
}

void IceStream::advance (float dt)
{
    if (! animating) return;
    phase += dt * 3.0f;
    for (auto& p : parts)
    {
        p.y += p.vy * dt; p.x += p.vx * dt; p.rot += p.vr * dt;
        if (p.y > (float) getHeight() + 12.0f) respawn (p, false);
    }
    repaint();
}

void IceStream::paint (juce::Graphics& g)
{
    const float cx = getWidth() * 0.5f, H = (float) getHeight();

    // frozen sauce bottle (illustration), top centre
    juce::Path bottle;
    bottle.addRoundedRectangle (cx - 34.0f, 6.0f, 68.0f, 34.0f, 10.0f);          // shoulder / cap area
    bottle.addRoundedRectangle (cx - 16.0f, 34.0f, 32.0f, 38.0f, 8.0f);          // neck
    g.setGradientFill (juce::ColourGradient (ice::white.withAlpha (0.85f), cx - 34, 6, ice::cyanDeep.withAlpha (0.55f), cx + 34, 70, false));
    g.fillPath (bottle);
    g.setColour (ice::outline);
    g.strokePath (bottle, juce::PathStrokeType (3.0f));
    g.setColour (ice::coral);
    g.fillRoundedRectangle (cx - 14.0f, 10.0f, 28.0f, 12.0f, 3.0f); // label
    g.setColour (ice::outline);
    g.drawRoundedRectangle (cx - 14.0f, 10.0f, 28.0f, 12.0f, 3.0f, 2.0f);

    // liquid stream
    juce::Path s;
    const float top = 70.0f;
    const int steps = 28;
    std::vector<juce::Point<float>> left, right;
    for (int i = 0; i <= steps; ++i)
    {
        const float t = (float) i / steps, y = top + t * (H - top);
        const float wob = std::sin (y * 0.045f - phase) * (3.0f + 8.0f * t);
        const float wdt = 10.0f + 12.0f * t;
        left.push_back ({ cx + wob - wdt, y });
        right.push_back ({ cx + wob + wdt, y });
    }
    s.startNewSubPath (left.front());
    for (auto& pt : left) s.lineTo (pt);
    for (auto it = right.rbegin(); it != right.rend(); ++it) s.lineTo (*it);
    s.closeSubPath();
    g.setGradientFill (juce::ColourGradient (ice::white.withAlpha (0.55f), cx, top, ice::cyan.withAlpha (0.12f), cx, H, false));
    g.fillPath (s);
    g.setColour (ice::cyan.withAlpha (0.55f));
    g.strokePath (s, juce::PathStrokeType (1.5f));

    // particles
    for (auto& p : parts)
    {
        const float a = p.alpha;
        const auto tf = juce::AffineTransform::rotation (p.rot, p.x, p.y);
        if (p.kind == 0) // crystal
        {
            g.setColour (ice::white.withAlpha (a));
            for (int k = 0; k < 3; ++k)
            {
                const float an = p.rot + k * juce::MathConstants<float>::pi / 3.0f;
                g.drawLine (p.x - std::cos (an) * p.size * 0.5f, p.y - std::sin (an) * p.size * 0.5f,
                            p.x + std::cos (an) * p.size * 0.5f, p.y + std::sin (an) * p.size * 0.5f, 1.6f);
            }
        }
        else if (p.kind == 1) // translucent cube
        {
            juce::Path c; c.addRoundedRectangle (p.x - p.size * 0.5f, p.y - p.size * 0.5f, p.size, p.size, 2.0f);
            c.applyTransform (tf);
            g.setColour (ice::cyan.withAlpha (a * 0.45f)); g.fillPath (c);
            g.setColour (ice::white.withAlpha (a)); g.strokePath (c, juce::PathStrokeType (1.2f));
        }
        else // droplet
        {
            g.setColour (ice::cyan.withAlpha (a * 0.8f));
            g.fillEllipse (p.x - p.size * 0.25f, p.y - p.size * 0.4f, p.size * 0.5f, p.size * 0.8f);
        }
    }
}

// ------------------------------------------------------------ editor
static constexpr int kW = 780, kH = 590, kHMore = 720;

BrightnessSauseEditor::BrightnessSauseEditor (BrightnessSauseProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      inMeter (p.inPeak, "IN"), outMeter (p.outPeak, "OUT"),
      excite (p.apvts, "excite", "EXCITE", true), air (p.apvts, "air", "AIR", true), brightness (p.apvts, "brightness", "BRIGHTNESS", true),
      mix (p.apvts, "mix", "MIX"), output (p.apvts, "output", "OUTPUT"), lowcut (p.apvts, "lowcut", "LOW CUT"),
      presGain (p.apvts, "presgain", "PRESENCE"), airFreq (p.apvts, "airfreq", "AIR FREQ"),
      character (p.apvts, "character", "CHARACTER"), harmonics (p.apvts, "harmonics", "HARMONICS"),
      presAmt (p.apvts, "presence", "PRES. AMOUNT"), softness (p.apvts, "softness", "SOFTNESS"),
      presFreq (p.apvts, "presfreq", "PRES. FREQ"), airGain (p.apvts, "airgain", "AIR GAIN"), qKnob (p.apvts, "q", "Q")
{
    setLookAndFeel (&laf);
    addAndMakeVisible (stream); // added first: sits behind the controls
    for (auto* c : { &inMeter, &outMeter }) addAndMakeVisible (*c);
    for (auto* c : { &excite, &air, &brightness, &mix, &output, &lowcut, &presGain, &airFreq,
                     &character, &harmonics, &presAmt, &softness, &presFreq, &airGain, &qKnob })
        addAndMakeVisible (*c);
    for (auto* b : { &bypassBtn, &eqBypassBtn, &animBtn, &moreBtn }) addAndMakeVisible (*b);

    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "bypass", bypassBtn);
    eqBypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "eqbypass", eqBypassBtn);

    const bool anim = (bool) p.apvts.state.getProperty ("iceFx", true);
    animBtn.setToggleState (anim, juce::dontSendNotification);
    stream.setAnimating (anim);
    animBtn.onClick = [this] {
        proc.apvts.state.setProperty ("iceFx", animBtn.getToggleState(), nullptr);
        stream.setAnimating (animBtn.getToggleState());
    };
    moreBtn.setToggleState ((bool) p.apvts.state.getProperty ("more", false), juce::dontSendNotification);
    moreBtn.onClick = [this] { proc.apvts.state.setProperty ("more", moreBtn.getToggleState(), nullptr); applyMoreState(); };

    setSize (kW, moreBtn.getToggleState() ? kHMore : kH);
    startTimerHz (30);
}

BrightnessSauseEditor::~BrightnessSauseEditor() { stopTimer(); setLookAndFeel (nullptr); }

void BrightnessSauseEditor::applyMoreState() { setSize (kW, moreBtn.getToggleState() ? kHMore : kH); }

void BrightnessSauseEditor::timerCallback()
{
    inMeter.tick();
    outMeter.tick();
    stream.advance (1.0f / 30.0f);
}

void BrightnessSauseEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (ice::bgTop, 0, 0, ice::bgBottom, 0, (float) getHeight(), false));
    g.fillAll();

    // frosty speckle bands
    g.setColour (ice::cyan.withAlpha (0.05f));
    for (int i = 0; i < 6; ++i) g.fillRoundedRectangle (30.0f + i * 125.0f, 90.0f, 70.0f, 230.0f, 35.0f);

    // brand
    g.setColour (ice::outline);
    g.setFont (ice::condensed (40.0f));
    g.drawText ("BRIGHTNESS SAUSE", juce::Rectangle<int> (22, 10, 400, 44).translated (0, 3), juce::Justification::centredLeft);
    g.setColour (ice::white);
    g.drawText ("BRIGHTNESS SAUSE", juce::Rectangle<int> (22, 10, 400, 44), juce::Justification::centredLeft);
    g.setColour (ice::cyan);
    g.setFont (ice::condensed (17.0f));
    g.drawText ("BY EVRIC HUNTER", juce::Rectangle<int> (24, 50, 300, 20), juce::Justification::centredLeft);

    // bottom panel
    auto panel = juce::Rectangle<float> (14.0f, 372.0f, (float) kW - 28.0f, 200.0f);
    g.setColour (ice::outline);
    g.fillRoundedRectangle (panel.expanded (3.0f), 20.0f);
    g.setColour (ice::panel);
    g.fillRoundedRectangle (panel, 18.0f);
    if (getHeight() > kH)
    {
        auto more = juce::Rectangle<float> (14.0f, 580.0f, (float) kW - 28.0f, 128.0f);
        g.setColour (ice::outline);
        g.fillRoundedRectangle (more.expanded (3.0f), 20.0f);
        g.setColour (ice::panel);
        g.fillRoundedRectangle (more, 18.0f);
    }
}

void BrightnessSauseEditor::resized()
{
    auto r = getLocalBounds();
    // top bar controls
    inMeter.setBounds (470, 12, 44, 56);
    outMeter.setBounds (524, 12, 44, 56);
    bypassBtn.setBounds (584, 12, 90, 28);
    animBtn.setBounds (684, 12, 80, 28);
    moreBtn.setBounds (684, 42, 80, 28);

    // centre: stream behind the three primary knobs
    stream.setBounds (0, 76, kW, 290);
    excite.setBounds (50, 190, 180, 176);
    brightness.setBounds (550, 190, 180, 176);
    air.setBounds (300, 190, 180, 176);

    // bottom row
    const int y = 386, w = 112, h = 150;
    int x = 28;
    for (auto* k : { &mix, &output, &lowcut, &presGain, &airFreq }) { k->setBounds (x, y, w, h); x += w + 6; }
    eqBypassBtn.setBounds (x + 4, y + 52, 130, 36);

    if (r.getHeight() > kH)
    {
        const int cw = 100; int mx = 24;
        for (auto* k : { &character, &harmonics, &presAmt, &softness, &presFreq, &airGain, &qKnob })
        { k->setBounds (mx, 588, cw, 118); mx += cw + 8; }
    }
    for (auto* k : { &character, &harmonics, &presAmt, &softness, &presFreq, &airGain, &qKnob })
        k->setVisible (r.getHeight() > kH);
}
