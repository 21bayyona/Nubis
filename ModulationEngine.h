// =============================================================================
//  NUBIS - ModulationEngine.h
//  4 LFOs, 2 ADSR, smooth random, 16-step sequencer, 4 macros, 16-slot matrix.
//  Control-rate (per sub-block). No JUCE dependency.
// =============================================================================
#pragma once

#include "DspUtils.h"

namespace nubis
{
enum ModSource : int
{
    SrcNone = 0, SrcLfo1, SrcLfo2, SrcLfo3, SrcLfo4, SrcEnv1, SrcEnv2, SrcRandom, SrcSeq,
    SrcMacro1, SrcMacro2, SrcMacro3, SrcMacro4, NumModSources
};
inline const char* const kModSourceNames[NumModSources] = {
    "None", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "ENV 1", "ENV 2", "RANDOM", "SEQ",
    "MACRO 1", "MACRO 2", "MACRO 3", "MACRO 4" };

enum class LfoShape : int { Sine = 0, Triangle, Saw, Square, Random, SampleHold, NumShapes };
inline const char* const kLfoShapeNames[6] = { "Sine", "Triangle", "Saw", "Square", "Random", "S&H" };

enum class EnvTrigger : int { Midi = 0, Loop, Audio };
inline const char* const kEnvTriggerNames[3] = { "MIDI", "Loop", "Audio" };

constexpr int kNumLfos = 4, kNumEnvs = 2, kNumMacros = 4, kNumSteps = 16, kNumModSlots = 16;

struct LfoParams  { float rateHz = 1.0f; bool sync = false; int division = 9; int shape = 0; float phaseOffset = 0.0f; };
struct EnvParams  { float attack = 0.01f, decay = 0.3f, sustain = 0.7f, release = 0.5f; int trigger = 0; };
struct ModSlot    { int source = 0; int dest = -1; float amount = 0.0f; };

struct ModParams
{
    std::array<LfoParams, kNumLfos> lfo;
    std::array<EnvParams, kNumEnvs> env;
    float rndRate = 1.0f, rndSmooth = 0.5f;
    std::array<float, kNumSteps> steps {};
    int   seqLength = 16, seqDivision = 3;
    float seqGlide = 0.0f;
    std::array<float, kNumMacros> macro {};
    std::array<ModSlot, kNumModSlots> slots;
};

struct TransportInfo { bool playing = false; double bpm = 120.0; double ppq = 0.0; };

class ModulationEngine
{
public:
    void prepare (double sampleRate);
    void reset();

    /** Advance by n samples (n <= kSubBlock) and compute all sources. */
    void process (int n, const ModParams& p, const TransportInfo& t, bool gate, bool noteOnEvent, float inputLevel) noexcept;

    float getSource (int s) const noexcept { return (s > 0 && s < NumModSources) ? values[(size_t) s] : 0.0f; }

    /** Sum matrix contributions into destOffsets (size numDests). */
    void applyMatrix (const ModParams& p, float* destOffsets, int numDests) const noexcept;

    int getCurrentStep() const noexcept { return currentStep; }
    float getEnvLevel (int i) const noexcept { return envs[(size_t) i].level; }

private:
    struct LfoState { double phase = 0.0; float held = 0.0f, prev = 0.0f, next = 0.0f; };
    struct EnvState
    {
        enum Stage { Idle, Attack, Decay, Sustain, Release } stage = Idle;
        float level = 0.0f; bool lastGate = false; double loopTimer = 0.0;
    };

    float lfoValue (LfoState& s, int shape, bool wrapped) noexcept;
    void  advanceEnv (EnvState& e, const EnvParams& p, bool gate, bool retrigger, float dt) noexcept;

    double sr = 44100.0;
    std::array<float, NumModSources> values {};
    std::array<LfoState, kNumLfos> lfos;
    std::array<EnvState, kNumEnvs> envs;
    Random rng { 0xD1CEu };
    float rndCurrent = 0.0f, rndTarget = 0.0f; double rndTimer = 0.0;
    double seqFreePhase = 0.0;
    int currentStep = 0;
    float seqValue = 0.0f;
    bool audioGate = false;
    float followLevel = 0.0f;
};

} // namespace nubis
