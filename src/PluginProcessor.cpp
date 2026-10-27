#include "PluginProcessor.h"
#include "PluginEditor.h"

ForceSampleDelayPluginAudioProcessor::ForceSampleDelayPluginAudioProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    lookaheadMsValue = apvts.getRawParameterValue ("lookaheadMs");
    bypassValue = apvts.getRawParameterValue ("bypass");
}

juce::AudioProcessorValueTreeState::ParameterLayout
ForceSampleDelayPluginAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // 0..2000 ms is arbitrary; adjust as needed.
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "lookaheadMs", 1 },
        "Lookahead (ms)",
        juce::NormalisableRange<float> (0.0f, 5000.0f, 0.1f),
        0.0f
    ));

    params.push_back (std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "bypass", 1 }, "Bypass", false));

    return { params.begin(), params.end() };
}

juce::AudioProcessorParameter* ForceSampleDelayPluginAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter ("bypass");
}

bool ForceSampleDelayPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Require same input/output layout and at least 1 channel
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return (! in.isDisabled())
        && (in == out)
        && (in.size() <= 2 || in.size() <= 8); // not strict; just sane
}

void ForceSampleDelayPluginAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    writePos = 0;

    updateLatencyFromParam();
    ensureDelayBuffers();
    delayBuffer.clear();
}

void ForceSampleDelayPluginAudioProcessor::updateLatencyFromParam()
{
    const float ms = lookaheadMsValue->load();
    const bool bypassed = bypassValue->load() >= 0.5f;

    const int newLatencySamples = bypassed ? 0
        : juce::jmax (0, (int) std::llround (ms * 0.001 * currentSampleRate));

    if (newLatencySamples != currentLatencySamples)
    {
        currentLatencySamples = newLatencySamples;

        // Tell the host our latency so it can compensate (PDC).
        setLatencySamples (currentLatencySamples);

        // Discard pending audio when changing delay or entering/leaving bypass.
        delayBuffer.clear();
        writePos = 0;
    }
}

void ForceSampleDelayPluginAudioProcessor::ensureDelayBuffers()
{
    const int numCh = juce::jmax (1, getTotalNumInputChannels());

    // Reserve the full range so bypass and delay changes do not allocate on the audio thread.
    const float maxDelayMs = apvts.getParameter ("lookaheadMs")->getNormalisableRange().end;
    const int needed = juce::jmax (1, (int) std::ceil (maxDelayMs * 0.001 * currentSampleRate) + 1);

    if (needed != delayBufferSize || delayBuffer.getNumChannels() != numCh)
    {
        delayBufferSize = needed;
        delayBuffer.setSize (numCh, delayBufferSize, false, true, true);
        delayBuffer.clear();
        writePos = 0;
    }
}

void ForceSampleDelayPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                    juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);

    // If parameter automated, update in realtime.
    // setLatencySamples is safe to call here in JUCE; host may reconfigure delay.
    updateLatencyFromParam();

    const int numCh = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    if (currentLatencySamples == 0)
        return; // pass-through

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* inOut = buffer.getWritePointer (ch);
        auto* d     = delayBuffer.getWritePointer (ch);

        int localWritePos = writePos;

        // Read position is writePos - latency (wrapped).
        int readPos = localWritePos - currentLatencySamples;
        if (readPos < 0)
            readPos += delayBufferSize;

        for (int i = 0; i < numSamples; ++i)
        {
            // Write current sample into delay buffer
            d[localWritePos] = inOut[i];

            // Read delayed sample out
            inOut[i] = d[readPos];

            // increment + wrap
            if (++localWritePos >= delayBufferSize) localWritePos = 0;
            if (++readPos       >= delayBufferSize) readPos       = 0;
        }
    }

    // Advance global write position by block size
    writePos += numSamples;
    writePos %= delayBufferSize;
}

juce::AudioProcessorEditor* ForceSampleDelayPluginAudioProcessor::createEditor()
{
    return new ForceSampleDelayPluginAudioProcessorEditor (*this);
}

void ForceSampleDelayPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mos (destData, true);
    apvts.copyState().writeToStream (mos);
}

void ForceSampleDelayPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto tree = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);
    if (tree.isValid())
        apvts.replaceState (tree);

    // After restoring, update latency/buffers if we already have SR.
    updateLatencyFromParam();
    ensureDelayBuffers();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ForceSampleDelayPluginAudioProcessor();
}
