// =============================================================================
//  NUBIS - Grain.h : a single grain voice + grain envelope tables
// =============================================================================
#pragma once

#include "DspUtils.h"

namespace nubis
{
//==============================================================================
enum class GrainEnvelope : int
{
    Hann = 0, Gauss, Triangle, Trapezoid, Percussive, Swell, Tukey, NumShapes
};

inline const char* const kEnvelopeNames[(int) GrainEnvelope::NumShapes] = {
    "Hann", "Gauss", "Triangle", "Trapezoid", "Perc", "Swell", "Tukey" };

/** Precomputed grain window tables (built once, read-only afterwards). */
class EnvelopeTables
{
public:
    static constexpr int kSize = 1024;
    EnvelopeTables();
    const float* get (int shape) const noexcept
    {
        return tables[(size_t) std::clamp (shape, 0, (int) GrainEnvelope::NumShapes - 1)].data();
    }
private:
    std::array<std::array<float, kSize + 2>, (size_t) GrainEnvelope::NumShapes> tables {};
};

//==============================================================================
/** Describes where a grain reads from. The region is a loop inside a buffer. */
struct GrainSource
{
    const float* L = nullptr;
    const float* R = nullptr;
    int  bufLen      = 0;   // full buffer length
    int  regionStart = 0;   // loop start inside the buffer
    int  regionLen   = 0;   // loop length (<= bufLen)
    const void* owner = nullptr; // identifies the buffer (for safe buffer swaps)
};

//==============================================================================
class Grain
{
public:
    struct StartInfo
    {
        GrainSource src;
        double startPos  = 0.0;  // region-relative position (source samples)
        double increment = 1.0;  // source samples per output sample (negative = reverse)
        int    length    = 1;    // output samples
        const float* env = nullptr;
        float  hardness  = 0.5f; // 0 = very soft, 0.5 = window as-is, 1 = hard edges
        float  gain      = 1.0f;
        float  pan       = 0.0f; // -1..1
        int    startDelay = 0;   // samples into the current block before it begins
    };

    void start (const StartInfo& s) noexcept;

    /** Adds this grain into outL/outR for n samples. */
    void render (float* outL, float* outR, int n) noexcept;

    bool isActive() const noexcept { return active; }
    void kill() noexcept { active = false; }
    const void* getOwner() const noexcept { return src.owner; }
    float getProgress() const noexcept { return length > 0 ? (float) age / (float) length : 1.0f; }
    double getPosition() const noexcept { return pos; }
    const GrainSource& getSource() const noexcept { return src; }

private:
    inline float shapeEnv (float e) const noexcept
    {
        if (hardness < 0.5f)  return lerpf (e * e, e, hardness * 2.0f);
        if (hardness > 0.5f)  return std::min (1.0f, e * (1.0f + (hardness - 0.5f) * 14.0f));
        return e;
    }

    GrainSource src;
    double pos = 0.0, inc = 1.0;
    int    length = 0, age = 0, delay = 0;
    const float* env = nullptr;
    float  envPos = 0.0f, envInc = 0.0f;
    float  midL = 0.0f, midR = 0.0f, side = 0.0f, hardness = 0.5f;
    bool   active = false;
};

} // namespace nubis
