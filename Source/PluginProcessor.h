#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP.h"

class BrightnessSauseProcessor : public juce::AudioProcessor
{
public:
    BrightnessSauseProcessor();
    ~BrightnessSauseProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Brightness Sause By Evric Hunter"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // Meters (written by audio thread, read by GUI thread). Index 0 = L, 1 = R.
    std::atomic<float> inPeak[2] { 0.f, 0.f }, outPeak[2] { 0.f, 0.f };
    std::atomic<float> deessReduction { 0.f }; // dB, for the GUI reduction readout

private:
    bs::Engine engine;
    std::atomic<float>* p[22] {};
    static void pushPeak (std::atomic<float>& a, float v)
    {
        float cur = a.load (std::memory_order_relaxed);
        while (v > cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrightnessSauseProcessor)
};
