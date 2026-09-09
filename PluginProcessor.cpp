#include "PluginProcessor.h"
#include "PluginEditor.h"

NexusMeterAudioProcessor::NexusMeterAudioProcessor()
    : AudioProcessor (BusesProperties()
                         .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                         .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout NexusMeterAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Target loudness for the "match to target" readout (e.g. -14 LUFS streaming, -23 broadcast)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "targetLufs", "Target LUFS",
        juce::NormalisableRange<float> (-40.0f, -6.0f, 0.1f), -14.0f));

    // Integrated/short-term/momentary display mode toggle could be a choice param
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "displayMode", "Display Mode",
        juce::StringArray { "Momentary", "Short-Term", "Integrated" }, 2));

    // Bypass (metering plugins should still pass audio through unmodified —
    // this is really just here as a template for future processing plugins)
    params.push_back (std::make_unique<juce::AudioParameterBool> ("bypass", "Bypass", false));

    return { params.begin(), params.end() };
}

void NexusMeterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    lufsMeter.prepare (sampleRate, getTotalNumInputChannels(), samplesPerBlock);
    correlationMeter.prepare (sampleRate);
    fifoIndex = 0;
    fifo.fill (0.0f);
}

bool NexusMeterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mono = juce::AudioChannelSet::mono();
    const auto stereo = juce::AudioChannelSet::stereo();
    auto in = layouts.getMainInputChannelSet();
    auto out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == mono || in == stereo;
}

void NexusMeterAudioProcessor::pushNextSampleIntoFifo (float sample)
{
    if (fifoIndex == fftSize)
    {
        if (! fftDataReady.load())
        {
            juce::ScopedLock lock (fftLock);
            std::fill (fftData.begin(), fftData.end(), 0.0f);
            std::copy (fifo.begin(), fifo.end(), fftData.begin());
            fftDataReady = true;
        }
        fifoIndex = 0;
    }
    fifo[(size_t) fifoIndex++] = sample;
}

void NexusMeterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // This plugin is a metering/analysis tool — it observes the signal and passes
    // it through completely unmodified. That's the correct behaviour for a
    // meter and is also the template you'd extend with real DSP later
    // (insert processing between the metering calls below and the return).

    lufsMeter.processBlock (buffer);
    correlationMeter.processBlock (buffer);

    if (buffer.getNumChannels() > 0)
    {
        const auto* readPtr = buffer.getReadPointer (0);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            pushNextSampleIntoFifo (readPtr[i]);
    }

    // Audio passes through unchanged — no writes to `buffer` beyond this point.
}

juce::AudioProcessorEditor* NexusMeterAudioProcessor::createEditor()
{
    return new NexusMeterAudioProcessorEditor (*this);
}

void NexusMeterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void NexusMeterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// This creates new instances of the plugin — required JUCE entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NexusMeterAudioProcessor();
}
