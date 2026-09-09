#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "MeterComponent.h"

class NexusMeterAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit NexusMeterAudioProcessorEditor (NexusMeterAudioProcessor&);
    ~NexusMeterAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    NexusMeterAudioProcessor& audioProcessor;

    BarMeter momentaryMeter, shortTermMeter, integratedMeter, truePeakMeter;
    CorrelationDisplay correlationDisplay;
    SpectrumDisplay spectrumDisplay { audioProcessor };

    juce::ComboBox displayModeBox;
    juce::Slider targetLufsSlider;
    juce::Label targetLufsLabel { {}, "Target" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> displayModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> targetLufsAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NexusMeterAudioProcessorEditor)
};
