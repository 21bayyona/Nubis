// =============================================================================
//  NUBIS - ModulationEngine.cpp
// =============================================================================
#include "ModulationEngine.h"

namespace nubis
{
void ModulationEngine::prepare (double sampleRate)
{
    sr = sampleRate;
    reset();
}

void ModulationEngine::reset()
{
    values.fill (0.0f);
    for (auto& l : lfos) l = {};
    for (auto& e : envs) e = {};
    rndCurrent = rndTarget = 0.0f; rndTimer = 0.0;
    seqFreePhase = 0.0; currentStep = 0; seqValue = 0.0f;
    audioGate = false; followLevel = 0.0f;
}

float ModulationEngine::lfoValue (LfoState& s, int shape, bool wrapped) noexcept
{
    const float ph = (float) s.phase;
    switch ((LfoShape) shape)
    {
        case LfoShape::Sine:     return std::sin (ph * kTwoPi);
        case LfoShape::Triangle: return 1.0f - 4.0f * std::abs (ph - 0.5f);
        case LfoShape::Saw:      return 2.0f * ph - 1.0f;
        case LfoShape::Square:   return ph < 0.5f ? 1.0f : -1.0f;
        case LfoShape::Random:
        {
            if (wrapped) { s.prev = s.next; s.next = rng.nextBipolar(); }
            const float w = 0.5f - 0.5f * std::cos (ph * kPi);   // cosine interpolation
            return lerpf (s.prev, s.next, w);
        }
        case LfoShape::SampleHold:
        case LfoShape::NumShapes:
        default:
            if (wrapped) s.held = rng.nextBipolar();
            return s.held;
    }
}

void ModulationEngine::advanceEnv (EnvState& e, const EnvParams& p, bool gate, bool retrigger, float dt) noexcept
{
    if ((gate && ! e.lastGate) || retrigger) e.stage = EnvState::Attack;
    if (! gate && e.lastGate && e.stage != EnvState::Idle) e.stage = EnvState::Release;
    e.lastGate = gate;

    const float a = std::max (0.0005f, p.attack), d = std::max (0.001f, p.decay), r = std::max (0.001f, p.release);
    switch (e.stage)
    {
        case EnvState::Attack:
            e.level += dt / a;
            if (e.level >= 1.0f) { e.level = 1.0f; e.stage = EnvState::Decay; }
            break;
        case EnvState::Decay:
        {
            const float coef = std::exp (-dt * 4.6f / d);   // reaches ~1 % of the way in 'decay' seconds
            e.level = p.sustain + (e.level - p.sustain) * coef;
            if (std::abs (e.level - p.sustain) < 1.0e-4f) { e.level = p.sustain; e.stage = EnvState::Sustain; }
            break;
        }
        case EnvState::Sustain: e.level = p.sustain; break;
        case EnvState::Release:
        {
            const float coef = std::exp (-dt * 4.6f / r);
            e.level *= coef;
            if (e.level < 1.0e-4f) { e.level = 0.0f; e.stage = EnvState::Idle; }
            break;
        }
        case EnvState::Idle:
        default: e.level = 0.0f; break;
    }
}

void ModulationEngine::process (int n, const ModParams& p, const TransportInfo& t, bool gate, bool noteOnEvent, float inputLevel) noexcept
{
    const float dt = (float) n / (float) sr;
    const double beatsPerSec = std::max (1.0, t.bpm) / 60.0;

    // ---- LFOs ---------------------------------------------------------------
    for (size_t i = 0; i < (size_t) kNumLfos; ++i)
    {
        auto& s = lfos[i];
        const auto& lp = p.lfo[i];
        const double prevPhase = s.phase;
        bool wrapped = false;
        if (lp.sync)
        {
            const double beats = divisionToBeats (lp.division);
            if (t.playing)
            {
                double np = t.ppq / beats + lp.phaseOffset;
                np -= std::floor (np);
                wrapped = np < prevPhase;
                s.phase = np;
            }
            else
            {
                s.phase += (double) dt * beatsPerSec / beats;
                if (s.phase >= 1.0) { s.phase -= std::floor (s.phase); wrapped = true; }
            }
        }
        else
        {
            s.phase += (double) dt * clampf (lp.rateHz, 0.005f, 40.0f);
            if (s.phase >= 1.0) { s.phase -= std::floor (s.phase); wrapped = true; }
        }
        values[(size_t) SrcLfo1 + i] = lfoValue (s, lp.shape, wrapped);
    }

    // ---- Envelopes ----------------------------------------------------------
    followLevel = std::max (inputLevel, followLevel * std::exp (-dt / 0.05f));
    if (! audioGate && followLevel > 0.03f) audioGate = true;           // ~ -30 dBFS
    else if (audioGate && followLevel < 0.012f) audioGate = false;      // hysteresis

    for (size_t i = 0; i < (size_t) kNumEnvs; ++i)
    {
        auto& e = envs[i];
        const auto& ep = p.env[i];
        bool g = gate, retrig = noteOnEvent;
        switch ((EnvTrigger) ep.trigger)
        {
            case EnvTrigger::Loop:
            {
                const double cycle = ep.attack + ep.decay + 0.15 + ep.release;
                e.loopTimer += dt;
                if (e.loopTimer >= cycle) e.loopTimer = 0.0;
                g = e.loopTimer < (ep.attack + ep.decay + 0.15);
                retrig = false;
                break;
            }
            case EnvTrigger::Audio: g = audioGate; retrig = false; break;
            case EnvTrigger::Midi:
            default: break;
        }
        advanceEnv (e, ep, g, retrig, dt);
        values[(size_t) SrcEnv1 + i] = e.level;
    }

    // ---- Random ----------------------------------------------------------------
    rndTimer += (double) dt * clampf (p.rndRate, 0.01f, 40.0f);
    if (rndTimer >= 1.0) { rndTimer -= std::floor (rndTimer); rndTarget = rng.nextBipolar(); }
    {
        const float smoothT = 0.001f + p.rndSmooth * p.rndSmooth * 2.0f / std::max (0.01f, p.rndRate);
        const float coef = 1.0f - std::exp (-dt / smoothT);
        rndCurrent += (rndTarget - rndCurrent) * coef;
    }
    values[SrcRandom] = rndCurrent;

    // ---- Step sequencer (always tempo-synced) --------------------------------------
    const double stepBeats = divisionToBeats (p.seqDivision);
    const int len = std::clamp (p.seqLength, 1, kNumSteps);
    double stepPos;
    if (t.playing) stepPos = t.ppq / stepBeats;
    else { seqFreePhase += (double) dt * beatsPerSec / stepBeats; stepPos = seqFreePhase; }
    currentStep = (int) std::fmod (std::floor (stepPos), (double) len);
    if (currentStep < 0) currentStep += len;
    {
        const float target = p.steps[(size_t) currentStep];
        const float stepSec = (float) (stepBeats / beatsPerSec);
        const float glideT = std::max (0.0005f, p.seqGlide * stepSec);
        const float coef = p.seqGlide <= 0.001f ? 1.0f : 1.0f - std::exp (-dt / glideT);
        seqValue += (target - seqValue) * coef;
    }
    values[SrcSeq] = seqValue;

    // ---- Macros ---------------------------------------------------------------------
    for (size_t i = 0; i < (size_t) kNumMacros; ++i) values[(size_t) SrcMacro1 + i] = p.macro[i];
}

void ModulationEngine::applyMatrix (const ModParams& p, float* destOffsets, int numDests) const noexcept
{
    for (const auto& s : p.slots)
    {
        if (s.source <= SrcNone || s.source >= NumModSources || s.dest < 0 || s.dest >= numDests) continue;
        destOffsets[s.dest] += s.amount * values[(size_t) s.source];
    }
}

} // namespace nubis
