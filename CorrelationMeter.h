#pragma once
#include <juce_dsp/juce_dsp.h>

/**
    CorrelationMeter
    -----------------
    Running phase-correlation coefficient between L and R:
       +1.0 = perfectly in phase (mono-compatible)
        0.0 = decorrelated (wide stereo / independent channels)
       -1.0 = out of phase (mono-cancels to silence)

    Uses an exponentially-weighted moving average so the needle behaves
    like a hardware phase meter rather than jittering sample to sample.
*/
class CorrelationMeter
{
public:
    void prepare (double sampleRate);
    void reset();
    void processBlock (const juce::AudioBuffer<float>& buffer);

    float getCorrelation() const { return correlation.load(); }

private:
    double sr = 44100.0;
    double smoothingCoeff = 0.99;

    double runningLL = 0.0, runningRR = 0.0, runningLR = 0.0;
    std::atomic<float> correlation { 1.0f };
};
