#pragma once
#include <juce_dsp/juce_dsp.h>
#include <deque>

/**
    LUFSMeter
    ----------
    Implements ITU-R BS.1770-4 style loudness measurement:
      - K-weighting filter (pre-filter shelf + RLB high-pass)
      - Mean-square gating blocks (400ms window, 100ms hop = 75% overlap)
      - Momentary (400ms), Short-term (3s), Integrated (gated, whole-programme) loudness
      - True peak estimate via 4x oversampled peak hold (simple version — a full
        BS.1770 true-peak needs a proper polyphase FIR oversampler; this uses
        juce::dsp::Oversampling as a reasonable approximation)

    This is a starting point, not a certified-accurate implementation — validate
    against a reference meter (e.g. Youlean Loudness Meter) before shipping.
*/
class LUFSMeter
{
public:
    LUFSMeter() = default;

    void prepare (double sampleRate, int numChannels, int maxBlockSize);
    void reset();

    /** Feed one block of audio (post K-weighting is applied internally). */
    void processBlock (const juce::AudioBuffer<float>& buffer);

    float getMomentaryLUFS() const   { return momentaryLUFS.load(); }
    float getShortTermLUFS() const   { return shortTermLUFS.load(); }
    float getIntegratedLUFS() const  { return integratedLUFS.load(); }
    float getTruePeakDb() const      { return truePeakDb.load(); }
    float getRmsDb() const           { return rmsDb.load(); }

private:
    double sr = 44100.0;
    int numChans = 2;

    // K-weighting filters, one pair (shelf + high-pass) per channel
    std::vector<juce::dsp::IIR::Filter<float>> preFilters;   // high-shelf stage
    std::vector<juce::dsp::IIR::Filter<float>> rlbFilters;   // RLB high-pass stage

    // Oversampler for true-peak estimation
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    // Rolling mean-square history for gating (100ms blocks)
    std::deque<double> blockMeanSquares;
    double blockAccumulator = 0.0;
    int blockSampleCount = 0;
    int samplesPer100ms = 4410;

    std::atomic<float> momentaryLUFS { -70.0f };
    std::atomic<float> shortTermLUFS { -70.0f };
    std::atomic<float> integratedLUFS { -70.0f };
    std::atomic<float> truePeakDb { -100.0f };
    std::atomic<float> rmsDb { -100.0f };

    double gatedSumSquares = 0.0;
    int gatedBlockCount = 0;

    static double meanSquareToLUFS (double meanSquare);
    void updateKWeightingCoefficients();
};
