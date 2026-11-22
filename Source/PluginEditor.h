#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class BarrelKickEditor : public juce::AudioProcessorEditor
{
public:
    explicit BarrelKickEditor (BarrelKickProcessor&);
    ~BarrelKickEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    BarrelKickProcessor& proc;
    juce::LookAndFeel_V4 laf;
    juce::TextButton audition { "AUDITION" };
    juce::GenericAudioProcessorEditor generic;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BarrelKickEditor)
};
