#include "PluginProcessor.h"
#include "PluginEditor.h"

ForceSampleDelayPluginAudioProcessor::ForceSampleDelayPluginAudioProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
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

    return { params.begin(), params.end() };
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
    auto* p = apvts.getRawParameterValue ("lookaheadMs");
    const float ms = (p != nullptr) ? p->load() : 0.0f;

    const int newLatencySamples = juce::jmax (0, (int) std::llround (ms * 0.001 * currentSampleRate));

    if (newLatencySamples != currentLatencySamples)
    {
        currentLatencySamples = newLatencySamples;

        // Tell the host our latency so it can compensate (PDC).
        setLatencySamples (currentLatencySamples);

        // Buffer size depends on latency, so refresh.
        ensureDelayBuffers();

        // Keep writePos valid; you can also choose to reset to avoid clicks.
        writePos %= juce::jmax (1, delayBufferSize);
    }
}

void ForceSampleDelayPluginAudioProcessor::ensureDelayBuffers()
{
    const int numCh = juce::jmax (1, getTotalNumInputChannels());

    // Need enough room to delay by currentLatencySamples plus block-sized safety.
    // We can use latency + 1 as minimum. Larger is fine.
    const int needed = juce::jmax (1, currentLatencySamples + 1);

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

    // Ensure buffer sizes if channels changed, etc.
    ensureDelayBuffers();

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
    apvts.state.writeToStream (mos);
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