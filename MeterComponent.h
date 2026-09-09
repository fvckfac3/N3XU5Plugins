#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

/** Vertical bar meter with a numeric readout, used for LUFS/peak/RMS. */
class BarMeter : public juce::Component
{
public:
    void setRange (float minDb, float maxDb) { rangeMin = minDb; rangeMax = maxDb; }
    void setLabel (const juce::String& l) { label = l; }
    void setValue (float newValueDb) { valueDb = newValueDb; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto meterArea = bounds.removeFromTop (bounds.getHeight() - 34.0f);

        g.setColour (juce::Colour (0xff141420));
        g.fillRoundedRectangle (meterArea, 4.0f);

        float norm = juce::jlimit (0.0f, 1.0f, (valueDb - rangeMin) / (rangeMax - rangeMin));
        auto fillArea = meterArea.reduced (3.0f);
        float fillHeight = fillArea.getHeight() * norm;
        auto fillRect = fillArea.removeFromBottom (fillHeight);

        juce::ColourGradient grad (juce::Colour (0xff00e5c7), fillRect.getBottomLeft(),
                                    juce::Colour (0xffff2d78), fillRect.getTopLeft(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fillRect, 3.0f);

        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.setFont (juce::Font (juce::FontOptions (13.0f)).withTypefaceStyle ("Bold"));
        auto textArea = bounds.removeFromBottom (34.0f);
        g.drawFittedText (label, textArea.removeFromTop (16.0f).toNearestInt(),
                           juce::Justification::centred, 1);
        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::plain)));
        g.drawFittedText (juce::String (valueDb, 1) + " dB", textArea.toNearestInt(),
                           juce::Justification::centred, 1);
    }

private:
    float rangeMin = -60.0f, rangeMax = 0.0f;
    float valueDb = -60.0f;
    juce::String label;
};

/** Horizontal needle showing stereo phase correlation, -1..+1. */
class CorrelationDisplay : public juce::Component
{
public:
    void setCorrelation (float c) { value = c; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (4.0f);
        g.setColour (juce::Colour (0xff141420));
        g.fillRoundedRectangle (bounds, 4.0f);

        float centreX = bounds.getCentreX();
        float halfWidth = bounds.getWidth() * 0.5f;
        float needleX = centreX + value * halfWidth;

        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawLine (centreX, bounds.getY(), centreX, bounds.getBottom(), 1.0f);

        juce::Colour needleColour = value < 0.0f ? juce::Colour (0xffff2d78) : juce::Colour (0xff00e5c7);
        g.setColour (needleColour);
        g.fillRoundedRectangle (needleX - 2.0f, bounds.getY(), 4.0f, bounds.getHeight(), 2.0f);

        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText ("-1", bounds.removeFromLeft (20.0f).toNearestInt(), juce::Justification::centredLeft);
        g.drawText ("+1", getLocalBounds().removeFromRight (20.0f), juce::Justification::centredRight);
    }

private:
    float value = 1.0f;
};

/** Simple log-frequency magnitude spectrum analyzer fed from the processor's FFT fifo. */
class SpectrumDisplay : public juce::Component
{
public:
    explicit SpectrumDisplay (NexusMeterAudioProcessor& p) : processor (p) {}

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff141420));
        g.fillRoundedRectangle (bounds, 4.0f);

        if (magnitudes.empty()) return;

        juce::Path path;
        const int numPoints = (int) magnitudes.size();
        for (int i = 0; i < numPoints; ++i)
        {
            float x = bounds.getX() + bounds.getWidth() * ((float) i / (float) numPoints);
            float dbVal = juce::Decibels::gainToDecibels (magnitudes[(size_t) i], -100.0f);
            float y = juce::jmap (dbVal, -100.0f, 0.0f, bounds.getBottom(), bounds.getY());
            if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        }
        g.setColour (juce::Colour (0xff00e5c7));
        g.strokePath (path, juce::PathStrokeType (1.5f));
    }

    void updateFromProcessor()
    {
        if (! processor.fftDataReady.load()) return;

        std::array<float, NexusMeterAudioProcessor::fftSize * 2> localCopy;
        {
            juce::ScopedLock lock (processor.fftLock);
            localCopy = processor.fftData;
            processor.fftDataReady = false;
        }

        processor.fftWindow.multiplyWithWindowingTable (localCopy.data(), NexusMeterAudioProcessor::fftSize);
        processor.fft.performFrequencyOnlyForwardTransform (localCopy.data());

        const int numBins = NexusMeterAudioProcessor::fftSize / 2;
        magnitudes.resize ((size_t) numBins);
        for (int i = 0; i < numBins; ++i)
            magnitudes[(size_t) i] = localCopy[(size_t) i] / (float) NexusMeterAudioProcessor::fftSize;

        repaint();
    }

private:
    NexusMeterAudioProcessor& processor;
    std::vector<float> magnitudes;
};
