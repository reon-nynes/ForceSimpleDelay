#pragma once

#include "PluginProcessor.h"

class ForceSampleDelayPluginAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ForceSampleDelayPluginAudioProcessorEditor (ForceSampleDelayPluginAudioProcessor&);
    ~ForceSampleDelayPluginAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ForceSampleDelayPluginAudioProcessor& processor;

    juce::Slider lookaheadMs;
    juce::Label  lookaheadLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lookaheadAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ForceSampleDelayPluginAudioProcessorEditor)
};
