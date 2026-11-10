// =============================================================================
//  NUBIS - SpectralBlur.cpp
// =============================================================================
#include "SpectralBlur.h"

namespace nubis
{
void SpectralBlur::prepare (double sampleRate)
{
    fft.prepare (kOrder);
    window.resize (kN);
    for (int i = 0; i < kN; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * kPi * (float) i / (float) kN); // periodic Hann
    re.assign (kN, 0.0f);
    im.assign (kN, 0.0f);
    for (auto& c : ch)
    {
        c.inRing.assign (kN, 0.0f);
        c.outRing.assign (kN, 0.0f);
        c.mag.assign (kBins, 0.0f);
    }
    amountSmooth.reset (sampleRate, 60.0f, 0.0f);
    reset();
}

void SpectralBlur::reset()
{
    for (auto& c : ch)
    {
        std::fill (c.inRing.begin(), c.inRing.end(), 0.0f);
        std::fill (c.outRing.begin(), c.outRing.end(), 0.0f);
        std::fill (c.mag.begin(), c.mag.end(), 0.0f);
    }
    pos = hopCounter = 0;
    idleSamples = 0;
    running = false;
    amountSmooth.snap (0.0f);
}

void SpectralBlur::processFrame (Channel& c, float amount) noexcept
{
    for (int k = 0; k < kN; ++k)
    {
        re[(size_t) k] = c.inRing[(size_t) ((pos + k) & (kN - 1))] * window[(size_t) k];
        im[(size_t) k] = 0.0f;
    }
    fft.perform (re.data(), im.data(), false);

    const float smear = std::pow (amount, 0.6f) * 0.985f;   // temporal magnitude smoothing
    const float phaseRand = amount * amount * kPi;           // phase diffusion

    for (int b = 0; b < kBins; ++b)
    {
        const float r = re[(size_t) b], i = im[(size_t) b];
        const float m = std::sqrt (r * r + i * i);
        float& ms = c.mag[(size_t) b];
        ms = smear * ms + (1.0f - smear) * m;

        float outR, outI;
        if (phaseRand > 1.0e-5f && b > 0 && b < kBins - 1)
        {
            const float ph = std::atan2 (i, r) + rng.nextBipolar() * phaseRand;
            outR = ms * std::cos (ph);
            outI = ms * std::sin (ph);
        }
        else
        {
            const float scale = m > 1.0e-12f ? ms / m : 0.0f;
            outR = r * scale;
            outI = i * scale;
        }
        re[(size_t) b] = outR;
        im[(size_t) b] = outI;
        if (b > 0 && b < kBins - 1) // hermitian mirror
        {
            re[(size_t) (kN - b)] = outR;
            im[(size_t) (kN - b)] = -outI;
        }
    }
    fft.perform (re.data(), im.data(), true);

    // Hann^2 at 75 % overlap sums to 1.5; inverse FFT is unnormalised (N)
    const float scale = 1.0f / (1.5f * (float) kN);
    for (int k = 0; k < kN; ++k)
        c.outRing[(size_t) ((pos + k) & (kN - 1))] += re[(size_t) k] * window[(size_t) k] * scale;
}

void SpectralBlur::process (float* L, float* R, int n, float amount) noexcept
{
    amountSmooth.setTarget (clampf (amount, 0.0f, 1.0f));
    float* io[2] = { L, R };

    for (int s = 0; s < n; ++s)
    {
        const float a   = amountSmooth.next();
        const float mix = clampf (a * 2.5f, 0.0f, 1.0f);

        if (a > 1.0e-4f) { running = true; idleSamples = 0; }
        else if (running && ++idleSamples > kN * 2) { running = false; for (auto& c : ch) { std::fill (c.outRing.begin(), c.outRing.end(), 0.0f); std::fill (c.mag.begin(), c.mag.end(), 0.0f); } }

        for (int c = 0; c < 2; ++c)
        {
            auto& chan = ch[(size_t) c];
            const float dryDelayed = chan.inRing[(size_t) pos];   // input from kN samples ago
            const float wet = chan.outRing[(size_t) pos];
            chan.outRing[(size_t) pos] = 0.0f;
            chan.inRing[(size_t) pos] = io[c][s];
            io[c][s] = running ? dryDelayed + (wet - dryDelayed) * mix : dryDelayed;
        }
        pos = (pos + 1) & (kN - 1);

        if (++hopCounter >= kHop)
        {
            hopCounter = 0;
            if (running)
            {
                const float a2 = amountSmooth.getCurrent();
                processFrame (ch[0], a2);
                processFrame (ch[1], a2);
            }
        }
    }
}

} // namespace nubis
