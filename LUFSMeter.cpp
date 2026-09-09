#include "LUFSMeter.h"

void LUFSMeter::prepare (double sampleRate, int numChannels, int maxBlockSize)
{
    sr = sampleRate;
    numChans = numChannels;
    samplesPer100ms = (int) std::round (sr * 0.1);

    preFilters.resize ((size_t) numChans);
    rlbFilters.resize ((size_t) numChans);
    updateKWeightingCoefficients();

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) numChans, 2 /* 4x = 2 stages of 2x */,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    oversampler->initProcessing ((size_t) maxBlockSize);

    reset();
}

void LUFSMeter::updateKWeightingCoefficients()
{
    // BS.1770 K-weighting: high-shelf (~+4dB above ~1.5kHz) then RLB high-pass (~-3dB at 38Hz)
    auto highShelf = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 1681.0f, 0.7071f, juce::Decibels::decibelsToGain (4.0f));
    auto highPass  = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 38.0f, 0.5f);

    for (auto& f : preFilters) f.coefficients = highShelf;
    for (auto& f : rlbFilters) f.coefficients = highPass;
}

void LUFSMeter::reset()
{
    for (auto& f : preFilters) f.reset();
    for (auto& f : rlbFilters) f.reset();
    blockMeanSquares.clear();
    blockAccumulator = 0.0;
    blockSampleCount = 0;
    gatedSumSquares = 0.0;
    gatedBlockCount = 0;
    momentaryLUFS = -70.0f;
    shortTermLUFS = -70.0f;
    integratedLUFS = -70.0f;
    truePeakDb = -100.0f;
    rmsDb = -100.0f;
}

double LUFSMeter::meanSquareToLUFS (double meanSquare)
{
    if (meanSquare <= 1.0e-12) return -70.0;
    return -0.691 + 10.0 * std::log10 (meanSquare);
}

void LUFSMeter::processBlock (const juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int chansToUse = juce::jmin (numChans, buffer.getNumChannels());

    // --- K-weight a working copy, per-sample accumulate mean square ---
    juce::AudioBuffer<float> weighted;
    weighted.makeCopyOf (buffer, true);

    for (int ch = 0; ch < chansToUse; ++ch)
    {
        auto* data = weighted.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            float s = data[i];
            s = preFilters[(size_t) ch].processSample (s);
            s = rlbFilters[(size_t) ch].processSample (s);
            data[i] = s;
        }
    }

    // Peak (pre-weighting, plain sample peak) for RMS/peak readout
    float peakAbs = buffer.getMagnitude (0, numSamples);
    for (int ch = 1; ch < chansToUse; ++ch)
        peakAbs = juce::jmax (peakAbs, buffer.getMagnitude (ch, 0, numSamples));

    // --- True peak via oversampling ---
    juce::dsp::AudioBlock<float> block (const_cast<juce::AudioBuffer<float>&> (buffer));
    auto oversampledBlock = oversampler->processSamplesUp (block);
    float tpPeak = 0.0f;
    for (size_t ch = 0; ch < oversampledBlock.getNumChannels(); ++ch)
    {
        auto* d = oversampledBlock.getChannelPointer (ch);
        for (size_t i = 0; i < oversampledBlock.getNumSamples(); ++i)
            tpPeak = juce::jmax (tpPeak, std::abs (d[i]));
    }
    oversampler->reset();
    truePeakDb = juce::Decibels::gainToDecibels (juce::jmax (tpPeak, 1.0e-8f));
    rmsDb = juce::Decibels::gainToDecibels (juce::jmax (peakAbs, 1.0e-8f));

    // --- Accumulate mean-square in 100ms chunks (sum across channels per BS.1770 channel weights;
    //     simplified here to equal-weighted stereo — extend with per-channel G_i for 5.1 etc.) ---
    for (int i = 0; i < numSamples; ++i)
    {
        double sampleSumSq = 0.0;
        for (int ch = 0; ch < chansToUse; ++ch)
        {
            float s = weighted.getSample (ch, i);
            sampleSumSq += (double) s * (double) s;
        }
        blockAccumulator += sampleSumSq;
        blockSampleCount++;

        if (blockSampleCount >= samplesPer100ms)
        {
            double meanSq = blockAccumulator / (blockSampleCount * (double) chansToUse);
            blockMeanSquares.push_back (meanSq);
            if (blockMeanSquares.size() > 30) // keep 3s of 100ms blocks for short-term
                blockMeanSquares.pop_front();

            blockAccumulator = 0.0;
            blockSampleCount = 0;

            // Momentary = last 4 blocks (400ms)
            if (blockMeanSquares.size() >= 4)
            {
                double sum = 0.0;
                for (size_t k = blockMeanSquares.size() - 4; k < blockMeanSquares.size(); ++k)
                    sum += blockMeanSquares[k];
                momentaryLUFS = (float) meanSquareToLUFS (sum / 4.0);
            }

            // Short-term = last 30 blocks (3s)
            if (! blockMeanSquares.empty())
            {
                double sum = 0.0;
                for (double v : blockMeanSquares) sum += v;
                shortTermLUFS = (float) meanSquareToLUFS (sum / (double) blockMeanSquares.size());
            }

            // Integrated (relative-gated, simplified two-pass omitted — running absolute
            // gate at -70 LUFS applied here; add the -10dB relative gate pass for full spec)
            double instLUFS = meanSquareToLUFS (meanSq);
            if (instLUFS > -70.0)
            {
                gatedSumSquares += meanSq;
                gatedBlockCount++;
                integratedLUFS = (float) meanSquareToLUFS (gatedSumSquares / (double) gatedBlockCount);
            }
        }
    }
}
