#include "PluginEditor.h"

ForceSampleDelayPluginAudioProcessorEditor::ForceSampleDelayPluginAudioProcessorEditor
    (ForceSampleDelayPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    lookaheadMs.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    lookaheadMs.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
    lookaheadMs.setTextValueSuffix (" ms");
    lookaheadMs.setChangeNotificationOnlyOnRelease (true);
    addAndMakeVisible (lookaheadMs);

    lookaheadLabel.setText ("Delay (Lookahead) in ms", juce::dontSendNotification);
    lookaheadLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (lookaheadLabel);

    lookaheadAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "lookaheadMs", lookaheadMs);

    bypassButton.setClickingTogglesState (true);
    bypassButton.setButtonText ("BYPASS OFF");
    bypassButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff465264));
    bypassButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffffc857));
    bypassButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    bypassButton.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff171b22));
    bypassButton.onStateChange = [this]
    {
        const bool bypassed = bypassButton.getToggleState();
        bypassButton.setButtonText (bypassed ? "BYPASS ON" : "BYPASS OFF");
        bypassButton.setTooltip (bypassed ? "Delay bypassed. Click to enable delay."
                                        : "Delay active. Click to bypass with zero latency.");
    };
    bypassButton.onStateChange();
    addAndMakeVisible (bypassButton);
    bypassAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, "bypass", bypassButton);

    setSize (240, 260);
}

void ForceSampleDelayPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff20242b));
}

void ForceSampleDelayPluginAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    lookaheadLabel.setBounds (area.removeFromTop (20));
    bypassButton.setBounds (area.removeFromBottom (36));
    area.removeFromBottom (12);
    lookaheadMs.setBounds (area.reduced (20));
}
