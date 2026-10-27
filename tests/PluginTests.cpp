#include "PluginEditor.h"

#include <iostream>

#if FORCE_SAMPLE_DELAY_VST3_TESTS
void setVST3TestPath (const juce::String&);
#endif

namespace
{
void setLookahead (ForceSampleDelayPluginAudioProcessor& processor, float ms)
{
    auto* parameter = processor.apvts.getParameter ("lookaheadMs");
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (ms));
}

class PluginTests : public juce::UnitTest
{
public:
    PluginTests() : UnitTest ("Plugin controls", "ForceSampleDelay") {}

    void runTest() override
    {
        beginTest ("Bypass passes audio immediately, reports zero latency, and preserves the delay setting");
        {
            ForceSampleDelayPluginAudioProcessor processor;
            setLookahead (processor, 1.0f);
            processor.prepareToPlay (48000.0, 64);
            expectEquals (processor.getLatencySamples(), 48);

            auto* bypass = processor.getBypassParameter();
            expect (bypass != nullptr, "The host must have a bypass parameter");
            if (bypass != nullptr)
            {
                juce::MidiBuffer midi;
                juce::AudioBuffer<float> audio (2, 64);
                audio.clear();
                audio.setSample (0, 63, 0.75f);
                processor.processBlock (audio, midi);

                bypass->setValueNotifyingHost (1.0f);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                        audio.setSample (ch, i, (float) (i + ch * 64) / 128.0f);

                juce::AudioBuffer<float> input;
                input.makeCopyOf (audio);
                processor.processBlock (audio, midi);
                expectEquals (processor.getLatencySamples(), 0);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                        expectEquals (audio.getSample (ch, i), input.getSample (ch, i));

                setLookahead (processor, 2.0f);
                processor.processBlock (audio, midi);
                expectEquals (processor.getLatencySamples(), 0);
                bypass->setValueNotifyingHost (0.0f);

                for (int block = 0; block < 2; ++block)
                {
                    audio.clear();
                    if (block == 0)
                        audio.setSample (0, 0, 1.0f);
                    processor.processBlock (audio, midi);
                    expectEquals (processor.getLatencySamples(), 96);
                    for (int i = 0; i < 64; ++i)
                        expectEquals (audio.getSample (0, i), block == 1 && i == 32 ? 1.0f : 0.0f);
                }
            }
        }

        beginTest ("The maximum delay stays sample accurate across the delay buffer wrap");
        {
            ForceSampleDelayPluginAudioProcessor processor;
            setLookahead (processor, 5000.0f);
            processor.prepareToPlay (48000.0, 256);
            expectEquals (processor.getLatencySamples(), 240000);
            juce::AudioBuffer<float> audio (2, 256);
            juce::MidiBuffer midi;
            for (int block = 0; block < 940; ++block)
            {
                audio.clear();
                if (block == 0 || block == 1)
                    audio.setSample (0, 0, 1.0f);
                processor.processBlock (audio, midi);
                expectEquals (audio.getMagnitude (0, 0, 256), block == 937 || block == 938 ? 1.0f : 0.0f);
                if (block == 937 || block == 938)
                    expectEquals (audio.getSample (0, 128), 1.0f);
                expectEquals (audio.getMagnitude (1, 0, 256), 0.0f);
            }
        }

        beginTest ("Bypass and lookahead survive an immediate state save and restore");
        {
            ForceSampleDelayPluginAudioProcessor processor;
            auto* bypass = processor.getBypassParameter();
            expect (bypass != nullptr);
            if (bypass != nullptr)
            {
                setLookahead (processor, 12.5f);
                bypass->setValueNotifyingHost (1.0f);
                juce::MemoryBlock state;
                processor.getStateInformation (state);

                ForceSampleDelayPluginAudioProcessor restored;
                restored.setStateInformation (state.getData(), (int) state.getSize());
                restored.prepareToPlay (48000.0, 64);
                expectEquals (restored.apvts.getRawParameterValue ("lookaheadMs")->load(), 12.5f);
                expectEquals (restored.getBypassParameter()->getValue(), 1.0f);
                expectEquals (restored.getLatencySamples(), 0);
                restored.getBypassParameter()->setValueNotifyingHost (0.0f);
                juce::AudioBuffer<float> audio (2, 64);
                audio.clear();
                juce::MidiBuffer midi;
                restored.processBlock (audio, midi);
                expectEquals (restored.getLatencySamples(), 600);
            }
        }

        beginTest ("Dragging previews the knob value and commits the parameter only on mouse-up");
        {
            ForceSampleDelayPluginAudioProcessor processor;
            setLookahead (processor, 1000.0f);
            processor.prepareToPlay (48000.0, 64);
            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
            juce::Slider* knob = nullptr;
            juce::Button* bypassButton = nullptr;
            for (auto* child : editor->getChildren())
            {
                if (auto* slider = dynamic_cast<juce::Slider*> (child))
                    knob = slider;
                if (auto* button = dynamic_cast<juce::Button*> (child))
                    bypassButton = button;
            }

            expect (knob != nullptr);
            expect (bypassButton != nullptr, "The editor must have a bypass button");
            if (knob != nullptr)
            {
                const auto start = knob->getLocalBounds().getCentre().toFloat();
                const auto now = juce::Time::getCurrentTime();
                const auto event = [&] (juce::Point<float> position, bool dragged)
                {
                    return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), position,
                                             juce::ModifierKeys::leftButtonModifier, 1.0f, 0.0f,
                                             0.0f, 0.0f, 0.0f, knob, knob, now, start, now, 1, dragged);
                };

                knob->mouseDown (event (start, false));
                knob->mouseDrag (event (start.translated (0.0f, -30.0f), true));
                expect (knob->getValue() > 1000.0, "The knob should preview the drag");
                expectEquals (processor.apvts.getRawParameterValue ("lookaheadMs")->load(), 1000.0f);
                juce::AudioBuffer<float> audio (2, 64);
                audio.clear();
                juce::MidiBuffer midi;
                processor.processBlock (audio, midi);
                expectEquals (processor.getLatencySamples(), 48000);

                const float previewMs = (float) knob->getValue();
                knob->mouseUp (event (start.translated (0.0f, -30.0f), true));
                // JUCE delivers the release notification through the message queue.
                const auto deadline = juce::Time::getMillisecondCounterHiRes() + 1000.0;
                while (std::abs (processor.apvts.getRawParameterValue ("lookaheadMs")->load() - previewMs) > 0.01f
                       && juce::Time::getMillisecondCounterHiRes() < deadline)
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
                expectWithinAbsoluteError (processor.apvts.getRawParameterValue ("lookaheadMs")->load(),
                                           previewMs, 0.01f);
                processor.processBlock (audio, midi);
                expect (processor.getLatencySamples() > 48000);

                knob->setValue (1500.0, juce::sendNotificationSync);
                expectEquals (processor.apvts.getRawParameterValue ("lookaheadMs")->load(), 1500.0f);
            }

            if (bypassButton != nullptr)
            {
                bypassButton->setToggleState (true, juce::sendNotificationSync);
                juce::AudioBuffer<float> audio (2, 64);
                audio.clear();
                juce::MidiBuffer midi;
                processor.processBlock (audio, midi);
                expectEquals (processor.getLatencySamples(), 0);
            }
        }
    }
};

PluginTests pluginTests;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juce;
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    bool testingVST3 = false;
   #if FORCE_SAMPLE_DELAY_VST3_TESTS
    testingVST3 = argc == 3 && juce::String (argv[1]) == "--vst3";
    if (testingVST3)
        setVST3TestPath (juce::String::fromUTF8 (argv[2]));
   #endif
    runner.runTestsInCategory (testingVST3 ? "VST3" : "ForceSampleDelay");
    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        const auto* result = runner.getResult (i);
        failures += result->failures;
        std::cout << result->subcategoryName << ": " << result->passes << " passed, "
                  << result->failures << " failed\n";
        for (const auto& message : result->messages)
            std::cout << message << '\n';
    }
    if (argc == 2)
    {
        const juce::File folder (juce::String::fromUTF8 (argv[1]));
        folder.createDirectory();
        ForceSampleDelayPluginAudioProcessor processor;
        setLookahead (processor, 1000.0f);
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        for (bool bypassed : { false, true })
        {
            processor.getBypassParameter()->setValueNotifyingHost (bypassed ? 1.0f : 0.0f);
            auto snapshot = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
            juce::FileOutputStream output (folder.getChildFile (bypassed ? "bypassed.png" : "active.png"));
            juce::PNGImageFormat().writeImageToStream (snapshot, output);
        }
    }
    return failures == 0 ? 0 : 1;
}
