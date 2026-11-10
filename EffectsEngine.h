// =============================================================================
//  NUBIS - EffectsEngine.h : 9 integrated effects with re-orderable routing.
//  No JUCE dependency.
// =============================================================================
#pragma once

#include "DspUtils.h"

namespace nubis
{
enum FxId : int { FxReverb = 0, FxDelay, FxDistortion, FxBitcrusher, FxFilter, FxChorus, FxPhaser, FxFreqShifter, FxImager, NumFx };
inline const char* const kFxNames[NumFx]  = { "Reverb", "Delay", "Distortion", "Bitcrusher", "Filter", "Chorus", "Phaser", "Freq Shift", "Imager" };
inline const char* const kFxShort[NumFx]  = { "REV", "DLY", "DIST", "CRUSH", "FILT", "CHOR", "PHAS", "SHIFT", "IMG" };
inline const char* const kDistTypeNames[4] = { "Soft", "Hard", "Fold", "Tube" };
inline const char* const kFilterModeNames[4] = { "Low Pass", "High Pass", "Band Pass", "Notch" };

struct FxParams
{
    std::array<bool, NumFx> enabled {};
    std::array<int, NumFx>  order { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
    double bpm = 120.0;

    float revSize = 0.6f, revDecay = 4.0f, revDamp = 0.4f, revPredelayMs = 20.0f, revMix = 0.3f;
    float dlyTimeMs = 350.0f; bool dlySync = true; int dlyDivision = 6;
    float dlyFeedback = 0.4f, dlyPingPong = 0.5f, dlyTone = 0.6f, dlyMix = 0.3f;
    float distDrive = 0.3f; int distType = 0; float distTone = 0.7f, distMix = 1.0f;
    float crushBits = 8.0f, crushDownsample = 4.0f, crushMix = 1.0f;
    float filtCutoff = 8000.0f, filtReso = 0.2f; int filtMode = 0; float filtDrive = 0.0f;
    float chRate = 0.6f, chDepth = 0.5f, chMix = 0.5f;
    float phRate = 0.3f, phDepth = 0.7f, phFeedback = 0.4f, phMix = 0.5f;
    float fsShiftHz = 0.0f, fsMix = 0.5f, fsFeedback = 0.0f;
    float imgWidth = 1.2f, imgMonoBassHz = 0.0f;
};

/** Packs/unpacks an effect order permutation into 4-bit nibbles. */
inline uint64_t packOrder (const std::array<int, NumFx>& o) noexcept
{
    uint64_t v = 0;
    for (int i = 0; i < NumFx; ++i) v |= (uint64_t) (o[(size_t) i] & 0xF) << (4 * i);
    return v;
}
inline std::array<int, NumFx> unpackOrder (uint64_t v) noexcept
{
    std::array<int, NumFx> o {};
    bool used[NumFx] = {};
    bool valid = true;
    for (int i = 0; i < NumFx; ++i)
    {
        const int e = (int) ((v >> (4 * i)) & 0xF);
        if (e >= NumFx || used[e]) { valid = false; break; }
        used[e] = true; o[(size_t) i] = e;
    }
    if (! valid) for (int i = 0; i < NumFx; ++i) o[(size_t) i] = i;
    return o;
}

//==============================================================================
class FxReverbUnit
{
public:
    void prepare (double sr);
    void reset();
    void process (float* L, float* R, int n, const FxParams& p) noexcept;
private:
    static constexpr int kLines = 8;
    double sr = 44100.0;
    std::array<DelayLine, kLines> lines;
    std::array<OnePoleLP, kLines> damp;
    std::array<AllpassDelay, 4> inDiff;
    DelayLine preL, preR;
    Smoother sizeSm, mixSm;
    float lfoPhase = 0.0f;
    std::array<float, kLines> baseMs { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 73.1f };
};

class FxDelayUnit
{
public:
    void prepare (double sr);
    void reset();
    void process (float* L, float* R, int n, const FxParams& p) noexcept;
private:
    double sr = 44100.0;
    DelayLine dL, dR;
    OnePoleLP toneL, toneR;
    DCBlocker dcL, dcR;
    Smoother timeSm, mixSm, fbSm;
};

class FxDistortionUnit
{
public:
    void prepare (double sr);
    void reset();
    void process (float* L, float* R, int n, const FxParams& p) noexcept;
private:
    float shape (float x, int type) const noexcept;
    double sr = 44100.0;
    std::array<Oversampler2x, 2> os;
    std::array<DelayLine, 2> dry;
    std::array<OnePoleLP, 2> tone;
    std::array<DCBlocker, 2> dc;
    Smoother driveSm, mixSm;
};

class FxBitcrusherUnit
{
public:
    void prepare (double sr);
    void reset();
    void process (float* L, float* R, int n, const FxParams& p) noexcept;
private:
    Smoother bitsSm, mixSm;
    float phase = 0.0f, heldL = 0.0f, heldR = 0.0f;
    double sr = 44100.0;
};

class FxFilterUnit
{
public:
    void prepare (double sr);
    void reset();
    void process (float* L, float* R, int n, const FxParams& p) noexcept;
private:
    double sr = 44100.0;
    std::array<SVF, 2> svf;
    Smoother cutSm, resSm, driveSm;
};

class FxChorusUnit
{
public:
    void prepare (double sr);
    void reset();
    void process (float* L, float* R, int n, const FxParams& p) noexcept;
private:
    double sr = 44100.0;
    DelayLine dL, dR;
    double phase = 0.0;
    Smoother depthSm, mixSm;
};

class FxPhaserUnit
{
public:
    void prepare (double sr);
    void reset();
    void process (float* L, float* R, int n, const FxParams& p) noexcept;
private:
    static constexpr int kStages = 6;
    double sr = 44100.0;
    std::array<std::array<float, kStages>, 2> z {};
    std::array<float, 2> fbState {};
    double phase = 0.0;
    Smoother mixSm;
};

class FxFreqShifterUnit
{
public:
    void prepare (double sr);
    void reset();
    void process (float* L, float* R, int n, const FxParams& p) noexcept;
private:
    double sr = 44100.0;
    std::array<HilbertPair, 2> hil;
    std::array<float, 2> fbState {};
    double phase = 0.0;
    Smoother shiftSm, mixSm;
};

class FxImagerUnit
{
public:
    void prepare (double sr);
    void reset();
    void process (float* L, float* R, int n, const FxParams& p) noexcept;
private:
    double sr = 44100.0;
    SVF sideHP;
    Smoother widthSm;
};

//==============================================================================
class EffectsEngine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    /** In-place stereo processing of up to kSubBlock samples. */
    void process (float* L, float* R, int n, const FxParams& p) noexcept;

private:
    void runFx (int id, float* L, float* R, int n, const FxParams& p) noexcept;

    double sr = 44100.0;
    FxReverbUnit reverb; FxDelayUnit delay; FxDistortionUnit dist; FxBitcrusherUnit crush;
    FxFilterUnit filter; FxChorusUnit chorus; FxPhaserUnit phaser; FxFreqShifterUnit shifter; FxImagerUnit imager;
    std::array<Smoother, NumFx> onSm;
    std::array<bool, NumFx> running {};
    std::array<float, kSubBlock> dryL {}, dryR {};
};

} // namespace nubis
