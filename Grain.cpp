// =============================================================================
//  NUBIS - Grain.cpp
// =============================================================================
#include "Grain.h"

namespace nubis
{
EnvelopeTables::EnvelopeTables()
{
    for (int s = 0; s < (int) GrainEnvelope::NumShapes; ++s)
    {
        auto& t = tables[(size_t) s];
        for (int i = 0; i <= kSize; ++i)
        {
            const double x = (double) i / kSize; // 0..1
            double v = 0.0;
            switch ((GrainEnvelope) s)
            {
                case GrainEnvelope::Hann:      v = 0.5 - 0.5 * std::cos (2.0 * kPiD * x); break;
                case GrainEnvelope::Gauss:     { const double d = (x - 0.5) / 0.15; v = std::exp (-0.5 * d * d); break; }
                case GrainEnvelope::Triangle:  v = 1.0 - std::abs (2.0 * x - 1.0); break;
                case GrainEnvelope::Trapezoid: v = std::min (1.0, std::min (x, 1.0 - x) * 6.0); break;
                case GrainEnvelope::Percussive:
                {
                    const double att = 0.015;
                    v = x < att ? x / att : std::exp (-(x - att) * 6.0);
                    break;
                }
                case GrainEnvelope::Swell:
                {
                    const double rel = 0.02;
                    v = x > 1.0 - rel ? (1.0 - x) / rel : std::exp (-((1.0 - rel) - x) * 6.0);
                    break;
                }
                case GrainEnvelope::Tukey:
                {
                    const double a = 0.25;
                    if (x < a * 0.5)            v = 0.5 * (1.0 - std::cos (2.0 * kPiD * x / a));
                    else if (x > 1.0 - a * 0.5) v = 0.5 * (1.0 - std::cos (2.0 * kPiD * (1.0 - x) / a));
                    else                        v = 1.0;
                    break;
                }
                case GrainEnvelope::NumShapes:
                default: v = 0.0; break;
            }
            t[(size_t) i] = (float) v;
        }
        // force true zero at both ends to avoid clicks, guard sample for interpolation
        t[0] = 0.0f;
        t[(size_t) kSize] = 0.0f;
        t[(size_t) kSize + 1] = 0.0f;
    }
}

//==============================================================================
void Grain::start (const StartInfo& s) noexcept
{
    src      = s.src;
    if (src.L == nullptr || src.regionLen < 8 || s.length < 2) { active = false; return; }

    pos      = s.startPos;
    inc      = s.increment;
    length   = s.length;
    age      = 0;
    delay    = std::max (0, s.startDelay);
    env      = s.env;
    envPos   = 0.0f;
    envInc   = (float) EnvelopeTables::kSize / (float) length;
    hardness = clampf (s.hardness, 0.0f, 1.0f);

    // equal-power pan of the mid signal, side preserved when centred
    const float p     = clampf (s.pan, -1.0f, 1.0f);
    const float theta = (p + 1.0f) * kPi * 0.25f;
    midL = std::cos (theta) * 1.41421356f * s.gain;
    midR = std::sin (theta) * 1.41421356f * s.gain;
    side = (1.0f - std::abs (p)) * s.gain;

    const double rl = (double) src.regionLen;
    pos = std::fmod (pos, rl);
    if (pos < 0.0) pos += rl;
    active = true;
}

void Grain::render (float* outL, float* outR, int n) noexcept
{
    if (! active) return;

    const float* L = src.L;
    const float* R = src.R;
    const int bufLen = src.bufLen, rStart = src.regionStart, rLen = src.regionLen;
    const double rl = (double) rLen;

    auto idx = [=] (int i) noexcept
    {
        // pos is kept in [0, rLen) so i is always within [-1, rLen + 2)
        if (i < 0) i += rLen; else if (i >= rLen) i -= rLen;
        int b = rStart + i;
        if (b >= bufLen) b -= bufLen;
        return b;
    };

    int i = delay;
    delay = 0;
    for (; i < n; ++i)
    {
        if (age >= length) { active = false; return; }

        // envelope (linear interpolation in table)
        const int   ei = (int) envPos;
        const float ef = envPos - (float) ei;
        float e = env[ei] + (env[ei + 1] - env[ei]) * ef;
        e = shapeEnv (e);

        // 4-point hermite read
        const int   i1 = (int) std::floor (pos);
        const float f  = (float) (pos - (double) i1);
        const int bm = idx (i1 - 1), b0 = idx (i1), b1 = idx (i1 + 1), b2 = idx (i1 + 2);
        const float sL = hermite4 (L[bm], L[b0], L[b1], L[b2], f);
        const float sR = hermite4 (R[bm], R[b0], R[b1], R[b2], f);

        const float m = 0.5f * (sL + sR);
        const float s = 0.5f * (sL - sR);
        outL[i] += e * (m * midL + s * side);
        outR[i] += e * (m * midR - s * side);

        pos += inc;
        if (pos >= rl) pos -= rl;
        else if (pos < 0.0) pos += rl;
        envPos += envInc;
        if (envPos > (float) EnvelopeTables::kSize) envPos = (float) EnvelopeTables::kSize;
        ++age;
    }
}

} // namespace nubis
