#include "CorrelationMeter.h"

void CorrelationMeter::prepare (double sampleRate)
{
    sr = sampleRate;
    // ~100ms time constant for the running average
    smoothingCoeff = std::exp (-1.0 / (sr * 0.1));
    reset();
}

void CorrelationMeter::reset()
{
    runningLL = runningRR = runningLR = 0.0;
    correlation = 1.0f;
}

void CorrelationMeter::processBlock (const juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < 2)
        return;

    const auto* l = buffer.getReadPointer (0);
    const auto* r = buffer.getReadPointer (1);
    const int n = buffer.getNumSamples();

    for (int i = 0; i < n; ++i)
    {
        double L = l[i], R = r[i];
        runningLL = smoothingCoeff * runningLL + (1.0 - smoothingCoeff) * (L * L);
        runningRR = smoothingCoeff * runningRR + (1.0 - smoothingCoeff) * (R * R);
        runningLR = smoothingCoeff * runningLR + (1.0 - smoothingCoeff) * (L * R);
    }

    double denom = std::sqrt (juce::jmax (runningLL * runningRR, 1.0e-12));
    double corr = denom > 0.0 ? (runningLR / denom) : 1.0;
    correlation = (float) juce::jlimit (-1.0, 1.0, corr);
}
