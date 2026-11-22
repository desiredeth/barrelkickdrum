#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "KickEngine.h"
#include "Rumble.h"

class BarrelKickProcessor : public juce::AudioProcessor
{
public:
    BarrelKickProcessor();
    ~BarrelKickProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "BARREL KICK GENERATOR"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<bool> auditionRequested { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float get (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    float distortSample (float x) const;

    static constexpr int kMaxBlock = 512;

    KickEngine engine;
    juce::AudioBuffer<float> mono;
    std::unique_ptr<juce::dsp::Oversampling<float>> os[2];

    int   distType = 0;
    float driveLin = 1.f, distMix = 1.f, outGain = 1.f;
    int   lastOsIdx = 0;
    float hpX = 0.f, hpY = 0.f;

    // Rumble
    RumbleFDN fdn;
    SVF rumbleHP[2], rumbleLP[2];
    float duckT = 10.f;
    int   trigPos[64] {};
    int   numTrig = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BarrelKickProcessor)
};
