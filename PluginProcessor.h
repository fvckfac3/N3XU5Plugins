#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "LUFSMeter.h"
#include "CorrelationMeter.h"

class NexusMeterAudioProcessor : public juce::AudioProcessor
{
public:
    NexusMeterAudioProcessor();
    ~NexusMeterAudioProcessor() override = default;

    // --- AudioProcessor overrides ---
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- Parameters ---
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // --- Metering data, read by the editor on a timer (NOT audio-thread safe to call
    //     into the UI directly — these atomics inside LUFSMeter/CorrelationMeter are
    //     the hand-off point) ---
    LUFSMeter lufsMeter;
    CorrelationMeter correlationMeter;

    // Simple spectrum: magnitude FFT data the editor can read for a spectrum analyzer
    static constexpr int fftOrder = 11; // 2048 points
    static constexpr int fftSize = 1 << fftOrder;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> fftWindow { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, fftSize * 2> fftData {};
    std::array<float, fftSize> fifo {};
    int fifoIndex = 0;
    std::atomic<bool> fftDataReady { false };
    juce::CriticalSection fftLock;

private:
    void pushNextSampleIntoFifo (float sample);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NexusMeterAudioProcessor)
};
