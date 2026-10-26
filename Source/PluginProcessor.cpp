#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ids
{
    // Stable parameter IDs (never rename: hosts store automation against these)
    constexpr const char* bypass = "bypass", *excite = "excite", *character = "character", *harmonics = "harmonics",
        *air = "air", *presence = "presence", *softness = "softness", *brightness = "brightness",
        *lowcut = "lowcut", *presfreq = "presfreq", *presgain = "presgain", *airfreq = "airfreq",
        *airgain = "airgain", *q = "q", *eqbypass = "eqbypass", *mix = "mix", *output = "output";
}

static juce::String pct (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }
static float fromPct (const juce::String& t) { return t.retainCharacters ("0123456789.-").getFloatValue() / 100.0f; }
static juce::String hz (float v, int)
{
    return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
}
static float fromHz (const juce::String& t)
{
    auto v = t.retainCharacters ("0123456789.-").getFloatValue();
    return t.containsIgnoreCase ("k") ? v * 1000.0f : v;
}
static juce::String dbs (float v, int) { return juce::String (v, 1) + " dB"; }
static float fromDb (const juce::String& t) { return t.retainCharacters ("0123456789.-").getFloatValue(); }

juce::AudioProcessorValueTreeState::ParameterLayout BrightnessSauseProcessor::createLayout()
{
    using namespace juce;
    using F = AudioParameterFloat;
    using B = AudioParameterBool;
    auto pctAttr = AudioParameterFloatAttributes().withStringFromValueFunction (pct).withValueFromStringFunction (fromPct);
    auto hzAttr  = AudioParameterFloatAttributes().withStringFromValueFunction (hz).withValueFromStringFunction (fromHz);
    auto dbAttr  = AudioParameterFloatAttributes().withStringFromValueFunction (dbs).withValueFromStringFunction (fromDb);
    auto qAttr   = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 2); });

    auto skewed = [] (float lo, float hi, float centre) { NormalisableRange<float> r (lo, hi); r.setSkewForCentre (centre); return r; };

    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<B> (ParameterID { ids::bypass, 1 }, "Bypass", false));
    l.add (std::make_unique<F> (ParameterID { ids::excite, 1 }, "Excite", NormalisableRange<float> (0, 1), 0.20f, pctAttr));
    l.add (std::make_unique<F> (ParameterID { ids::character, 1 }, "Character", NormalisableRange<float> (0, 1), 0.5f, pctAttr));
    l.add (std::make_unique<F> (ParameterID { ids::harmonics, 1 }, "Harmonics", NormalisableRange<float> (0, 1), 0.5f, pctAttr));
    l.add (std::make_unique<F> (ParameterID { ids::air, 1 }, "Air", NormalisableRange<float> (0, 1), 0.15f, pctAttr));
    l.add (std::make_unique<F> (ParameterID { ids::presence, 1 }, "Presence", NormalisableRange<float> (0, 1), 0.0f, pctAttr));
    l.add (std::make_unique<F> (ParameterID { ids::softness, 1 }, "Softness", NormalisableRange<float> (0, 1), 0.5f, pctAttr));
    l.add (std::make_unique<F> (ParameterID { ids::brightness, 1 }, "Brightness", NormalisableRange<float> (0, 1), 0.25f, pctAttr));
    l.add (std::make_unique<F> (ParameterID { ids::lowcut, 1 }, "Low Cut", skewed (20, 500, 100), 80.0f, hzAttr));
    l.add (std::make_unique<F> (ParameterID { ids::presfreq, 1 }, "Presence Freq", skewed (1000, 10000, 3500), 4000.0f, hzAttr));
    l.add (std::make_unique<F> (ParameterID { ids::presgain, 1 }, "Presence Gain", NormalisableRange<float> (-6, 6, 0.1f), 1.0f, dbAttr));
    l.add (std::make_unique<F> (ParameterID { ids::airfreq, 1 }, "Air Freq", skewed (5000, 20000, 11000), 12000.0f, hzAttr));
    l.add (std::make_unique<F> (ParameterID { ids::airgain, 1 }, "Air Gain", NormalisableRange<float> (-6, 9, 0.1f), 2.0f, dbAttr));
    l.add (std::make_unique<F> (ParameterID { ids::q, 1 }, "Q", skewed (0.3f, 4.0f, 1.0f), 0.8f, qAttr));
    l.add (std::make_unique<B> (ParameterID { ids::eqbypass, 1 }, "EQ Bypass", false));
    l.add (std::make_unique<F> (ParameterID { ids::mix, 1 }, "Mix", NormalisableRange<float> (0, 1), 1.0f, pctAttr));
    l.add (std::make_unique<F> (ParameterID { ids::output, 1 }, "Output", NormalisableRange<float> (-12, 12, 0.1f), 0.0f, dbAttr));
    return l;
}

BrightnessSauseProcessor::BrightnessSauseProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BrightnessSause", createLayout())
{
    const char* order[17] = { ids::excite, ids::character, ids::harmonics, ids::air, ids::presence, ids::softness,
                              ids::brightness, ids::lowcut, ids::presfreq, ids::presgain, ids::airfreq, ids::airgain,
                              ids::q, ids::eqbypass, ids::mix, ids::output, ids::bypass };
    for (int i = 0; i < 17; ++i) p[i] = apvts.getRawParameterValue (order[i]);
    setLatencySamples (0); // strict zero-latency design
}

bool BrightnessSauseProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto o = l.getMainOutputChannelSet();
    return (o == juce::AudioChannelSet::mono() || o == juce::AudioChannelSet::stereo()) && o == l.getMainInputChannelSet();
}

void BrightnessSauseProcessor::prepareToPlay (double sr, int)
{
    engine.prepare (sr);
}

void BrightnessSauseProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int nCh = juce::jmin (buffer.getNumChannels(), 2), n = buffer.getNumSamples();

    bs::Params q;
    q.excite = *p[0]; q.character = *p[1]; q.harmonics = *p[2]; q.air = *p[3]; q.presence = *p[4];
    q.softness = *p[5]; q.brightness = *p[6]; q.lowCutHz = *p[7]; q.presFreqHz = *p[8]; q.presGainDb = *p[9];
    q.airFreqHz = *p[10]; q.airGainDb = *p[11]; q.q = *p[12]; q.eqBypass = *p[13] > 0.5f;
    q.mix = *p[14]; q.outputDb = *p[15]; q.bypass = *p[16] > 0.5f;
    engine.setParams (q);

    for (int c = 0; c < nCh; ++c) pushPeak (inPeak[c], buffer.getMagnitude (c, 0, n));
    engine.process (buffer.getArrayOfWritePointers(), nCh, n);
    for (int c = 0; c < nCh; ++c) pushPeak (outPeak[c], buffer.getMagnitude (c, 0, n));
    if (nCh == 1) { pushPeak (inPeak[1], inPeak[0].load()); pushPeak (outPeak[1], outPeak[0].load()); }
}

void BrightnessSauseProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void BrightnessSauseProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* BrightnessSauseProcessor::createEditor() { return new BrightnessSauseEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BrightnessSauseProcessor(); }
