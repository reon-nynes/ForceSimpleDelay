#include "PluginEditor.h"

ForceSampleDelayPluginAudioProcessorEditor::ForceSampleDelayPluginAudioProcessorEditor
    (ForceSampleDelayPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    lookaheadMs.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    lookaheadMs.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
    lookaheadMs.setTextValueSuffix (" ms");
    addAndMakeVisible (lookaheadMs);

    lookaheadLabel.setText ("Delay (Lookahead) in ms", juce::dontSendNotification);
    lookaheadLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (lookaheadLabel);

    lookaheadAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "lookaheadMs", lookaheadMs);

    setSize (240, 200);
}

void ForceSampleDelayPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
}

void ForceSampleDelayPluginAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    lookaheadLabel.setBounds (area.removeFromTop (20));
    lookaheadMs.setBounds (area.reduced (20));
}
