// =============================================================================
//  NUBIS - GranularEngine.h
//  Real-time granular engine: capture buffer (live input), loaded sample or
//  internal sources, grain scheduler, creative functions and texture stage.
//  No JUCE dependency: fully testable standalone.
// =============================================================================
#pragma once

#include "DspUtils.h"
#include "Grain.h"
#include "SpectralBlur.h"

#include <memory>
#include <string>

namespace nubis
{
//==============================================================================
enum class SourceMode : int { Live = 0, Sample, Internal };
inline const char* const kSourceNames[3] = { "Live Input", "Sample", "Internal" };

enum class InternalSource : int { Harmonic = 0, Vox, Glass, Air, NumSources };
inline const char* const kInternalNames[4] = { "Harmonic", "Vox", "Glass", "Air" };

enum class SpectralMode : int { Off = 0, Chromatic, Major, Minor, Pentatonic, FifthsOctaves, Harmonics, NumModes };
inline const char* const kSpectralNames[7] = { "Off", "Chromatic", "Major", "Minor", "Pentatonic", "5ths+8ves", "Harmonics" };

constexpr int kOverviewBins = 512;
constexpr int kMaxGrains    = 192;
constexpr int kMaxNotes     = 8;

//==============================================================================
/** A user sample, decoded and prepared on the message thread. */
struct SampleData
{
    std::vector<float> L, R;
    double sampleRate = 44100.0;
    std::string path;
    std::array<float, kOverviewBins> overview {};

    /** Builds overview and a seamless loop crossfade. Call on a non-audio thread. */
    void finalise();
};

//==============================================================================
/** Fully resolved (already modulated) engine parameters, refreshed per sub-block. */
struct EngineParams
{
    SourceMode source  = SourceMode::Live;
    int   internalType = 0;
    bool  midiMode     = false;   // grains gated + transposed by MIDI notes

    // GRAIN ENGINE
    float sizeMs      = 120.0f;   // 1 .. 2000
    float density     = 20.0f;    // grains / s, 0.5 .. 400
    float position    = 0.5f;     // 0..1
    float posRandom   = 0.1f;     // 0..1
    float pitch       = 0.0f;     // semitones -24..24
    float pitchRandom = 0.0f;     // semitones 0..24
    float jitter      = 0.3f;     // 0..1
    float overlap     = 0.5f;     // 0..1
    float spread      = 0.5f;     // 0..1
    float pan         = 0.0f;     // -1..1
    float reverse     = 0.0f;     // probability 0..1
    float direction   = 0.0f;     // scan speed -1..1 (x2 realtime)
    int   envelope    = 0;
    bool  freeze      = false;    // momentary/latched capture freeze (or scan hold)

    // TEXTURE ENGINE
    float texture     = 0.5f;     // envelope edge hardness
    float smoothness  = 0.0f;     // low-pass smoothing of the cloud
    float blur        = 0.0f;     // spectral blur
    float randomness  = 0.0f;     // per-grain variation
    float diffusion   = 0.0f;     // allpass diffusion
    float thickness   = 0.0f;     // companion grains (texture density)
    float width       = 1.0f;     // 0..2
    float feedback    = 0.0f;     // 0..0.95

    // SPECIAL
    bool  infiniteFreeze = false;
    int   spectralMode   = 0;
    float spectralAmount = 0.0f;
    float chaos          = 0.0f;
    float evolve         = 0.0f;
    float evolveRate     = 0.1f;  // Hz
    float reverseCloud   = 0.0f;
    float drone          = 0.0f;

    // externally computed
    float ampEnv = 1.0f;          // ADSR 1 level when midiMode
};

/** Grain event for the visualiser. */
struct GrainEvent
{
    float position;   // 0..1 absolute within displayed buffer
    float pitch;      // semitones
    float pan;        // -1..1
    float lengthSec;
    float gain;
    uint8_t reverse;
    uint8_t source;   // SourceMode
};

//==============================================================================
class GranularEngine
{
public:
    GranularEngine();
    ~GranularEngine();

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    /** Processes up to kSubBlock samples. inL/inR may be nullptr (instrument). */
    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const EngineParams& p) noexcept;

    // MIDI (audio thread)
    void noteOn (int note, float velocity) noexcept;
    void noteOff (int note) noexcept;
    void allNotesOff() noexcept;
    bool anyNoteHeld() const noexcept { return numHeld > 0; }

    // Sample hand-off (message thread)
    void submitSample (std::unique_ptr<SampleData> s);
    void collectGarbage();
    void clearSample();

    // Frozen-buffer persistence (message thread, guarded by try-lock on audio thread)
    bool copyFrozenAudio (std::vector<float>& L, std::vector<float>& R) const;
    void restoreFrozenAudio (const std::vector<float>& L, const std::vector<float>& R);

    // ---- visualisation (any thread, lock-free) ------------------------------
    SpscRing<GrainEvent, 1024> grainEvents;
    std::array<std::atomic<float>, kOverviewBins> liveOverview;
    std::atomic<float> writeHeadNorm { 0.0f }, playheadNorm { 0.0f }, regionStartNorm { 0.0f }, regionLenNorm { 1.0f };
    std::atomic<bool>  isFrozen { false };
    std::atomic<int>   activeGrainCount { 0 };
    std::atomic<int>   displaySource { 0 };
    std::atomic<float> displayRateScale { 0.1f };  // view fraction travelled per second at unity pitch
    const std::array<float, kOverviewBins>& getInternalOverview (int type) const noexcept
    {
        return internalOverview[(size_t) std::clamp (type, 0, (int) InternalSource::NumSources - 1)];
    }

    double getSampleRate() const noexcept { return sr; }
    int    getCaptureLength() const noexcept { return capLen; }

private:
    void renderInternalSources();
    void writeInput (const float* inL, const float* inR, int n, const EngineParams& p) noexcept;
    void engageFreeze() noexcept;
    void scheduleGrains (int n, const EngineParams& p, float effSize, float effDensity, float effPos, float effJitter,
                         float effOverlap, float effPitchOffset, float effPan, float effPosRandom, float effPitchRandom) noexcept;
    void spawnGrain (int offset, const EngineParams& p, float sizeMs, float rate, float pos, float posRandom,
                     float pitchSemis, float pitchRandom, float panCentre, float extraPitch, bool companion) noexcept;
    float quantisePitch (float semis, int mode) noexcept;
    void handleSampleSwap() noexcept;
    void textureStage (float* L, float* R, int n, const EngineParams& p) noexcept;

    double sr = 44100.0;
    int    maxBlock = 512;

    // capture (live) buffer
    std::vector<float> capL, capR;
    int  capLen = 0, writeHead = 0;
    int64_t samplesWritten = 0;
    bool frozenState = false;
    int  freezeRegionStart = 0, freezeRegionLen = 0;
    int  unfreezeFade = 0;
    mutable std::atomic<bool> bufferLock { false };

    // internal sources
    std::array<std::vector<float>, (size_t) InternalSource::NumSources> intL, intR;
    std::array<std::array<float, kOverviewBins>, (size_t) InternalSource::NumSources> internalOverview {};
    double internalRenderedRate = 0.0;

    // user sample (lock-free swap)
    std::atomic<SampleData*> pendingSample { nullptr };
    std::atomic<SampleData*> garbageSample { nullptr };
    SampleData* activeSample   = nullptr;
    SampleData* retiringSample = nullptr;
    std::atomic<bool> clearRequested { false };

    // grains
    EnvelopeTables envTables;
    std::array<Grain, kMaxGrains> grains;
    double samplesToNextGrain = 0.0;
    float  scanPhase = 0.0f, scanHoldPos = 0.0f;
    int    roundRobin = 0;

    // MIDI
    std::array<int, kMaxNotes> heldNotes {};
    std::array<float, kMaxNotes> heldVel {};
    int numHeld = 0;
    std::array<int, kMaxNotes> lastNotes {};
    int numLast = 0;

    // creative modulators
    Random rng { 0x51A7u };
    std::array<float, 5> chaosWalk {}, chaosTarget {};
    double evolvePhase = 0.0, evolveDrift = 0.0;

    // texture
    std::array<OnePoleLP, 4> smoothLP;
    std::array<AllpassDelay, 8> diffusers;
    SpectralBlur blur;
    DelayLine regenL, regenR;
    DCBlocker fbDcL, fbDcR;
    std::vector<float> lastOutL, lastOutR;
    Smoother widthSm, diffSm, smoothSm, fbSm;
    float overviewPeak = 0.0f;
    int   overviewBin = -1;

    // scratch
    std::vector<float> tmpL, tmpR;
};

} // namespace nubis
