#include "PluginEditor.h"

NexusMeterAudioProcessorEditor::NexusMeterAudioProcessorEditor (NexusMeterAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    momentaryMeter.setRange (-40.0f, 0.0f);
    momentaryMeter.setLabel ("Momentary");
    shortTermMeter.setRange (-40.0f, 0.0f);
    shortTermMeter.setLabel ("Short-Term");
    integratedMeter.setRange (-40.0f, 0.0f);
    integratedMeter.setLabel ("Integrated");
    truePeakMeter.setRange (-60.0f, 6.0f);
    truePeakMeter.setLabel ("True Peak");

    for (auto* m : { &momentaryMeter, &shortTermMeter, &integratedMeter, &truePeakMeter })
        addAndMakeVisible (m);

    addAndMakeVisible (correlationDisplay);
    addAndMakeVisible (spectrumDisplay);

    displayModeBox.addItemList ({ "Momentary", "Short-Term", "Integrated" }, 1);
    addAndMakeVisible (displayModeBox);
    displayModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.apvts, "displayMode", displayModeBox);

    targetLufsSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    targetLufsSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 20);
    addAndMakeVisible (targetLufsSlider);
    addAndMakeVisible (targetLufsLabel);
    targetLufsAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.apvts, "targetLufs", targetLufsSlider);

    setSize (640, 420);
    setResizable (true, true);
    setResizeLimits (480, 320, 1200, 800);

    startTimerHz (30);
}

void NexusMeterAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0a0a0f));

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (20.0f)).withTypefaceStyle ("Bold"));
    g.drawText ("NEXUS METER", getLocalBounds().removeFromTop (36).reduced (12, 0),
                juce::Justification::centredLeft);
}

void NexusMeterAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (12);
    bounds.removeFromTop (30); // title space

    auto topControls = bounds.removeFromTop (28);
    displayModeBox.setBounds (topControls.removeFromLeft (160));
    topControls.removeFromLeft (12);
    targetLufsLabel.setBounds (topControls.removeFromLeft (50));
    targetLufsSlider.setBounds (topControls.removeFromLeft (220));

    bounds.removeFromTop (12);

    auto meterRow = bounds.removeFromTop (180);
    int meterWidth = meterRow.getWidth() / 4;
    momentaryMeter.setBounds (meterRow.removeFromLeft (meterWidth).reduced (6));
    shortTermMeter.setBounds (meterRow.removeFromLeft (meterWidth).reduced (6));
    integratedMeter.setBounds (meterRow.removeFromLeft (meterWidth).reduced (6));
    truePeakMeter.setBounds (meterRow.removeFromLeft (meterWidth).reduced (6));

    bounds.removeFromTop (12);
    correlationDisplay.setBounds (bounds.removeFromTop (40));

    bounds.removeFromTop (12);
    spectrumDisplay.setBounds (bounds);
}

void NexusMeterAudioProcessorEditor::timerCallback()
{
    momentaryMeter.setValue (audioProcessor.lufsMeter.getMomentaryLUFS());
    shortTermMeter.setValue (audioProcessor.lufsMeter.getShortTermLUFS());
    integratedMeter.setValue (audioProcessor.lufsMeter.getIntegratedLUFS());
    truePeakMeter.setValue (audioProcessor.lufsMeter.getTruePeakDb());
    correlationDisplay.setCorrelation (audioProcessor.correlationMeter.getCorrelation());
    spectrumDisplay.updateFromProcessor();
}
