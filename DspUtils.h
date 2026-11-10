// =============================================================================
//  NUBIS - Granular Texture Engine
//  DspUtils.h : small, allocation-free DSP building blocks (no JUCE dependency)
// =============================================================================
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace nubis
{
constexpr float  kPi       = 3.14159265358979323846f;
constexpr float  kTwoPi    = 6.28318530717958647692f;
constexpr double kPiD      = 3.14159265358979323846;
constexpr int    kSubBlock = 32;   // control-rate resolution (samples)

inline float clampf (float x, float lo, float hi) noexcept { return std::min (hi, std::max (lo, x)); }
inline float lerpf  (float a, float b, float t) noexcept   { return a + (b - a) * t; }
inline float dbToGain (float db) noexcept                  { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept                   { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float semisToRatio (float s) noexcept               { return std::exp2 (s * (1.0f / 12.0f)); }
inline bool  isFiniteF (float x) noexcept                  { return std::isfinite (x); }

/** Rational tanh approximation, bounded to [-1, 1]. */
inline float fastTanh (float x) noexcept
{
    x = clampf (x, -4.9f, 4.9f);
    const float x2 = x * x;
    const float a  = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    const float b  = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    return clampf (a / b, -1.0f, 1.0f);
}

/** Gentle saturator: linear-ish below ~0.5, asymptotic to +-1. */
inline float softClip (float x) noexcept { return fastTanh (x); }

//==============================================================================
/** xorshift32 PRNG: deterministic, lock-free, realtime safe. */
class Random
{
public:
    explicit Random (uint32_t seed = 0x9E3779B9u) noexcept : state (seed ? seed : 1u) {}
    void     seed (uint32_t s) noexcept { state = s ? s : 1u; }
    uint32_t nextU32() noexcept
    {
        uint32_t x = state;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        return state = x;
    }
    float nextFloat() noexcept   { return (float) (nextU32() >> 8) * (1.0f / 16777216.0f); } // [0,1)
    float nextBipolar() noexcept { return nextFloat() * 2.0f - 1.0f; }
    int   nextInt (int maxExclusive) noexcept { return maxExclusive > 0 ? (int) (nextU32() % (uint32_t) maxExclusive) : 0; }
private:
    uint32_t state;
};

//==============================================================================
/** One-pole parameter smoother. */
class Smoother
{
public:
    void reset (double sampleRate, float timeMs, float initial) noexcept
    {
        const double t = std::max (0.01, (double) timeMs) * 0.001;
        coeff   = (float) std::exp (-1.0 / (t * sampleRate));
        current = target = initial;
    }
    void  setTarget (float t) noexcept { target = t; }
    void  snap (float v) noexcept      { current = target = v; }
    float next() noexcept              { current = target + (current - target) * coeff; return current; }
    float getCurrent() const noexcept  { return current; }
    float getTarget() const noexcept   { return target; }
    /** advance n samples at once (control-rate use) */
    float skip (int n) noexcept
    {
        current = target + (current - target) * std::pow (coeff, (float) n);
        return current;
    }
private:
    float coeff = 0.0f, current = 0.0f, target = 0.0f;
};

//==============================================================================
class OnePoleLP
{
public:
    void setCutoff (float hz, double sr) noexcept
    {
        hz = clampf (hz, 5.0f, (float) sr * 0.49f);
        a  = (float) std::exp (-2.0 * kPiD * hz / sr);
    }
    float process (float x) noexcept { z = x + (z - x) * a; return z; }
    void  reset() noexcept { z = 0.0f; }
private:
    float a = 0.0f, z = 0.0f;
};

class DCBlocker
{
public:
    void  prepare (double sr) noexcept { r = (float) (1.0 - (2.0 * kPiD * 20.0 / sr)); }
    float process (float x) noexcept  { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
    void  reset() noexcept { x1 = y1 = 0.0f; }
private:
    float r = 0.995f, x1 = 0.0f, y1 = 0.0f;
};

//==============================================================================
/** Topology-preserving-transform state variable filter (Zavalishin). */
class SVF
{
public:
    enum Mode { LP = 0, HP, BP, Notch, Peak };

    void setParams (float cutoffHz, float q, double sr) noexcept
    {
        cutoffHz = clampf (cutoffHz, 10.0f, (float) sr * 0.47f);
        g  = (float) std::tan (kPiD * cutoffHz / sr);
        k  = 1.0f / std::max (0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    inline void tick (float x, float& lp, float& bp, float& hp) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
    float process (float x, int mode) noexcept
    {
        float lp, bp, hp;
        tick (x, lp, bp, hp);
        switch (mode)
        {
            case LP:    return lp;
            case HP:    return hp;
            case BP:    return bp * k;          // unity peak gain
            case Notch: return lp + hp;
            case Peak:  return lp - hp;
            default:    return lp;
        }
    }
    void reset() noexcept { ic1 = ic2 = 0.0f; }
    float getK() const noexcept { return k; }
private:
    float g = 0.0f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, ic1 = 0.0f, ic2 = 0.0f;
};

//==============================================================================
/** Power-of-two circular delay line with fractional reads. Allocate in prepare only. */
class DelayLine
{
public:
    void allocate (int maxSamples)
    {
        int size = 1;
        while (size < maxSamples + 4) size <<= 1;
        buffer.assign ((size_t) size, 0.0f);
        mask = size - 1;
        writePos = 0;
    }
    void clear() noexcept { std::fill (buffer.begin(), buffer.end(), 0.0f); }
    inline void push (float x) noexcept { buffer[(size_t) writePos] = x; writePos = (writePos + 1) & mask; }
    /** delay in samples (>= 1). Linear interpolation. */
    inline float readLinear (float delay) const noexcept
    {
        const float rp = (float) writePos - delay;
        int   i0 = (int) std::floor (rp);
        const float f = rp - (float) i0;
        const float a = buffer[(size_t) (i0 & mask)];
        const float b = buffer[(size_t) ((i0 + 1) & mask)];
        return a + (b - a) * f;
    }
    /** delay in samples (>= 2). 4-point Hermite interpolation. */
    inline float readCubic (float delay) const noexcept
    {
        const float rp = (float) writePos - delay;
        const int   i1 = (int) std::floor (rp);
        const float f  = rp - (float) i1;
        const float xm = buffer[(size_t) ((i1 - 1) & mask)];
        const float x0 = buffer[(size_t) (i1 & mask)];
        const float x1 = buffer[(size_t) ((i1 + 1) & mask)];
        const float x2 = buffer[(size_t) ((i1 + 2) & mask)];
        const float c1 = 0.5f * (x1 - xm);
        const float c2 = xm - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
    inline float readInt (int delay) const noexcept { return buffer[(size_t) ((writePos - delay) & mask)]; }
    int capacity() const noexcept { return mask + 1; }
private:
    std::vector<float> buffer;
    int mask = 0, writePos = 0;
};

inline float hermite4 (float xm, float x0, float x1, float x2, float f) noexcept
{
    const float c1 = 0.5f * (x1 - xm);
    const float c2 = xm - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm) + 1.5f * (x0 - x1);
    return ((c3 * f + c2) * f + c1) * f + x0;
}

//==============================================================================
/** Schroeder allpass with fixed integer delay (used in diffusers / reverbs). */
class AllpassDelay
{
public:
    void allocate (int maxDelay) { line.allocate (maxDelay + 2); }
    void setDelay (int d) noexcept { delay = std::max (1, d); }
    void clear() noexcept { line.clear(); }
    inline float process (float x, float g) noexcept
    {
        const float d = line.readInt (delay);
        const float v = x + g * d;
        line.push (v);
        return d - g * v;
    }
    inline float processMod (float x, float g, float fracDelay) noexcept
    {
        const float d = line.readLinear (fracDelay);
        const float v = x + g * d;
        line.push (v);
        return d - g * v;
    }
private:
    DelayLine line;
    int delay = 1;
};

//==============================================================================
/** In-place iterative radix-2 complex FFT. Tables built in prepare(). */
class FFT
{
public:
    void prepare (int order)
    {
        n = 1 << order;
        cosT.resize ((size_t) n / 2);
        sinT.resize ((size_t) n / 2);
        for (int i = 0; i < n / 2; ++i)
        {
            cosT[(size_t) i] = (float) std::cos (2.0 * kPiD * i / n);
            sinT[(size_t) i] = (float) -std::sin (2.0 * kPiD * i / n);
        }
        rev.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < order; ++b) if (i & (1 << b)) r |= 1 << (order - 1 - b);
            rev[(size_t) i] = r;
        }
    }
    int size() const noexcept { return n; }

    /** forward if inverse == false. Inverse is unnormalised. */
    void perform (float* re, float* im, bool inverse) const noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const int j = rev[(size_t) i];
            if (j > i) { std::swap (re[i], re[j]); std::swap (im[i], im[j]); }
        }
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len >> 1, step = n / len;
            for (int i = 0; i < n; i += len)
            {
                for (int k = 0; k < half; ++k)
                {
                    const float wr = cosT[(size_t) (k * step)];
                    const float wi = inverse ? -sinT[(size_t) (k * step)] : sinT[(size_t) (k * step)];
                    const int a = i + k, b = a + half;
                    const float tr = re[b] * wr - im[b] * wi;
                    const float ti = re[b] * wi + im[b] * wr;
                    re[b] = re[a] - tr; im[b] = im[a] - ti;
                    re[a] += tr;        im[a] += ti;
                }
            }
        }
    }
private:
    int n = 0;
    std::vector<float> cosT, sinT;
    std::vector<int> rev;
};

//==============================================================================
/** 2x oversampler using a windowed-sinc half-band FIR (linear phase). */
class Oversampler2x
{
public:
    static constexpr int kTaps = 47;
    static constexpr int kLatency = (kTaps - 1) / 2;   // up + down filters, in base-rate samples (= 23)

    Oversampler2x()
    {
        const int M = kTaps - 1;
        double sum = 0.0;
        for (int i = 0; i < kTaps; ++i)
        {
            const double x = i - M * 0.5;
            const double sinc = (std::abs (x) < 1.0e-9) ? 0.5 : std::sin (kPiD * 0.5 * x) / (kPiD * x);
            const double w = 0.42 - 0.5 * std::cos (2.0 * kPiD * i / M) + 0.08 * std::cos (4.0 * kPiD * i / M); // Blackman
            h[(size_t) i] = (float) (sinc * w);
            sum += h[(size_t) i];
        }
        for (auto& c : h) c = (float) (c / sum);
        reset();
    }
    void reset() noexcept { upHist.fill (0.0f); downHist.fill (0.0f); upPos = downPos = 0; }

    /** produce two oversampled samples from one input sample */
    inline void upsample (float x, float& y0, float& y1) noexcept
    {
        // zero-stuffed input: x, 0
        y0 = pushUp (x * 2.0f);
        y1 = pushUp (0.0f);
    }
    /** consume two oversampled samples, return one decimated sample */
    inline float downsample (float x0, float x1) noexcept
    {
        const float y = pushDownAndFilter (x0);   // keep even phase -> integer latency
        pushDown (x1);
        return y;
    }
private:
    inline float pushUp (float x) noexcept
    {
        upHist[(size_t) upPos] = x;
        upHist[(size_t) (upPos + kTaps)] = x;
        upPos = (upPos + 1) % kTaps;
        float acc = 0.0f;
        const float* src = &upHist[(size_t) upPos];
        for (int i = 0; i < kTaps; ++i) acc += src[i] * h[(size_t) i];
        return acc;
    }
    inline void pushDown (float x) noexcept
    {
        downHist[(size_t) downPos] = x;
        downHist[(size_t) (downPos + kTaps)] = x;
        downPos = (downPos + 1) % kTaps;
    }
    inline float pushDownAndFilter (float x) noexcept
    {
        pushDown (x);
        float acc = 0.0f;
        const float* src = &downHist[(size_t) downPos];
        for (int i = 0; i < kTaps; ++i) acc += src[i] * h[(size_t) i];
        return acc;
    }
    std::array<float, kTaps> h {};
    std::array<float, kTaps * 2> upHist {}, downHist {};
    int upPos = 0, downPos = 0;
};

//==============================================================================
/** 90-degree phase-difference allpass network (O. Niemitalo coefficients).
    Produces an analytic pair (re, im) for frequency shifting. */
class HilbertPair
{
public:
    void reset() noexcept { for (auto& s : st) s = {}; delayed = 0.0f; }
    inline void process (float x, float& outRe, float& outIm) noexcept
    {
        static constexpr float c[8] = { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f,
                                        0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };
        float a = x, b = x;
        for (int i = 0; i < 4; ++i) a = stage (st[(size_t) i], a, c[i] * c[i]);
        for (int i = 4; i < 8; ++i) b = stage (st[(size_t) i], b, c[i] * c[i]);
        outRe = delayed;   // first path is delayed by one sample (~49 dB image rejection)
        outIm = b;
        delayed = a;
    }
private:
    struct State { float x1 = 0, x2 = 0, y1 = 0, y2 = 0; };
    static inline float stage (State& s, float x, float a2) noexcept
    {
        const float y = a2 * (x + s.y2) - s.x2;
        s.x2 = s.x1; s.x1 = x; s.y2 = s.y1; s.y1 = y;
        return y;
    }
    std::array<State, 8> st {};
    float delayed = 0.0f;
};

//==============================================================================
/** Single-producer / single-consumer lock-free ring (trivially copyable T). */
template <typename T, size_t Capacity>
class SpscRing
{
public:
    bool push (const T& v) noexcept
    {
        const size_t w = write.load (std::memory_order_relaxed);
        const size_t next = (w + 1) % Capacity;
        if (next == read.load (std::memory_order_acquire)) return false;
        data[w] = v;
        write.store (next, std::memory_order_release);
        return true;
    }
    bool pop (T& v) noexcept
    {
        const size_t r = read.load (std::memory_order_relaxed);
        if (r == write.load (std::memory_order_acquire)) return false;
        v = data[r];
        read.store ((r + 1) % Capacity, std::memory_order_release);
        return true;
    }
private:
    std::array<T, Capacity> data {};
    std::atomic<size_t> write { 0 }, read { 0 };
};

//==============================================================================
/** Tempo divisions shared by LFOs, sequencer and delay (in quarter-note beats). */
inline constexpr int kNumDivisions = 15;
inline const char* const kDivisionNames[kNumDivisions] = {
    "1/64", "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D",
    "1/4T", "1/4", "1/4D", "1/2", "1 bar", "2 bars", "4 bars" };
inline double divisionToBeats (int idx) noexcept
{
    static constexpr double beats[kNumDivisions] = {
        0.0625, 0.125, 1.0 / 6.0, 0.25, 0.375, 1.0 / 3.0, 0.5, 0.75,
        2.0 / 3.0, 1.0, 1.5, 2.0, 4.0, 8.0, 16.0 };
    return beats[std::clamp (idx, 0, kNumDivisions - 1)];
}

} // namespace nubis
