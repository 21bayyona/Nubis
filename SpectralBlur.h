// =============================================================================
//  NUBIS - SpectralBlur.h : STFT magnitude smearing + phase diffusion
//  Latency of the wet path: kN samples. The dry path inside is latency-aligned,
//  so mixing never comb-filters.
// =============================================================================
#pragma once

#include "DspUtils.h"

namespace nubis
{
class SpectralBlur
{
public:
    static constexpr int kOrder = 11;
    static constexpr int kN     = 1 << kOrder; // 2048
    static constexpr int kHop   = kN / 4;      // 75 % overlap
    static constexpr int kBins  = kN / 2 + 1;

    void prepare (double sampleRate);
    void reset();

    /** In-place stereo processing. amount 0..1 */
    void process (float* L, float* R, int n, float amount) noexcept;

private:
    struct Channel
    {
        std::vector<float> inRing, outRing, mag;
    };
    void processFrame (Channel& c, float amount) noexcept;

    FFT fft;
    std::array<Channel, 2> ch;
    std::vector<float> window, re, im;
    Random rng { 0xB10Bu };
    Smoother amountSmooth;
    int  pos = 0, hopCounter = 0;
    int  idleSamples = 0;
    bool running = false;
};

} // namespace nubis
