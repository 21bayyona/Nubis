// =============================================================================
//  NUBIS - GranularEngine.cpp
// =============================================================================
#include "GranularEngine.h"

#include <thread>

namespace nubis
{
namespace
{
    constexpr int   kLoopXfade    = 2048;
    constexpr int   kLiveMargin   = kSubBlock * 2 + 8;
    constexpr float kCaptureSecs  = 12.0f;
    constexpr float kInternalRoot = 261.6256f; // C4 -> MIDI 60

    /** Equal-power crossfade of the tail into pre-roll material, then drop the pre-roll.
        Result: buffers loop seamlessly from end back to index 0. */
    void makeSeamlessLoop (std::vector<float>& L, std::vector<float>& R, int xfade)
    {
        const int len = (int) L.size();
        xfade = std::min (xfade, len / 4);
        if (xfade < 16) return;
        for (int i = 0; i < xfade; ++i)
        {
            const float w  = (float) i / (float) xfade;
            const float gOut = std::cos (w * kPi * 0.5f), gIn = std::sin (w * kPi * 0.5f);
            const int t = len - xfade + i;
            L[(size_t) t] = L[(size_t) t] * gOut + L[(size_t) i] * gIn;
            R[(size_t) t] = R[(size_t) t] * gOut + R[(size_t) i] * gIn;
        }
        L.erase (L.begin(), L.begin() + xfade);
        R.erase (R.begin(), R.begin() + xfade);
    }

    void computeOverview (const std::vector<float>& L, const std::vector<float>& R, std::array<float, kOverviewBins>& ov)
    {
        const size_t len = L.size();
        float maxPeak = 1.0e-6f;
        for (int b = 0; b < kOverviewBins; ++b)
        {
            const size_t s0 = len * (size_t) b / kOverviewBins, s1 = std::max (s0 + 1, len * (size_t) (b + 1) / kOverviewBins);
            float pk = 0.0f;
            for (size_t i = s0; i < s1 && i < len; ++i) pk = std::max (pk, 0.5f * (std::abs (L[i]) + std::abs (R[i])));
            ov[(size_t) b] = pk;
            maxPeak = std::max (maxPeak, pk);
        }
        for (auto& v : ov) v = std::min (1.0f, v);
    }

    void normalise (std::vector<float>& L, std::vector<float>& R, float target)
    {
        float pk = 1.0e-9f;
        for (size_t i = 0; i < L.size(); ++i) pk = std::max ({ pk, std::abs (L[i]), std::abs (R[i]) });
        const float g = target / pk;
        for (size_t i = 0; i < L.size(); ++i) { L[i] *= g; R[i] *= g; }
    }

    inline float wrap01 (float x) noexcept { x -= std::floor (x); return x; }
}

//==============================================================================
void SampleData::finalise()
{
    if (R.size() != L.size()) R = L;
    makeSeamlessLoop (L, R, kLoopXfade);
    computeOverview (L, R, overview);
}

//==============================================================================
GranularEngine::GranularEngine()
{
    for (auto& v : liveOverview) v.store (0.0f);
}

GranularEngine::~GranularEngine()
{
    delete pendingSample.exchange (nullptr);
    delete garbageSample.exchange (nullptr);
    if (retiringSample != activeSample) delete retiringSample;
    delete activeSample;
}

void GranularEngine::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    maxBlock = maxBlockSize;

    capLen = (int) (sr * kCaptureSecs);
    capL.assign ((size_t) capLen, 0.0f);
    capR.assign ((size_t) capLen, 0.0f);

    if (std::abs (internalRenderedRate - sr) > 0.5) renderInternalSources();

    const int maxDiff = (int) (sr * 0.02) + 8;
    for (auto& d : diffusers) d.allocate (maxDiff);
    static constexpr float diffMs[8] = { 4.7f, 3.6f, 12.7f, 9.3f, 5.1f, 3.3f, 11.9f, 10.1f };
    for (size_t i = 0; i < 8; ++i) diffusers[i].setDelay ((int) (diffMs[i] * 0.001f * (float) sr));

    regenL.allocate ((int) (sr * 1.6));
    regenR.allocate ((int) (sr * 1.6));
    fbDcL.prepare (sr); fbDcR.prepare (sr);

    lastOutL.assign (kSubBlock, 0.0f);
    lastOutR.assign (kSubBlock, 0.0f);
    tmpL.assign (kSubBlock, 0.0f);
    tmpR.assign (kSubBlock, 0.0f);

    blur.prepare (sr);
    widthSm.reset (sr, 40.0f, 1.0f);
    diffSm.reset (sr, 40.0f, 0.0f);
    smoothSm.reset (sr, 40.0f, 0.0f);
    fbSm.reset (sr, 60.0f, 0.0f);
    reset();
}

void GranularEngine::reset()
{
    for (auto& g : grains) g.kill();
    std::fill (capL.begin(), capL.end(), 0.0f);
    std::fill (capR.begin(), capR.end(), 0.0f);
    writeHead = 0;
    samplesWritten = 0;
    frozenState = false;
    freezeRegionStart = freezeRegionLen = 0;
    unfreezeFade = 0;
    samplesToNextGrain = 0.0;
    scanPhase = 0.0f;
    for (auto& f : smoothLP) f.reset();
    for (auto& d : diffusers) d.clear();
    regenL.clear(); regenR.clear();
    fbDcL.reset(); fbDcR.reset();
    std::fill (lastOutL.begin(), lastOutL.end(), 0.0f);
    std::fill (lastOutR.begin(), lastOutR.end(), 0.0f);
    blur.reset();
    for (auto& v : liveOverview) v.store (0.0f);
    overviewBin = -1; overviewPeak = 0.0f;
    isFrozen.store (false);
}

//==============================================================================
void GranularEngine::renderInternalSources()
{
    const int extra = kLoopXfade;
    const int len   = (int) (sr * 4.0) + extra;
    Random r (0xC0FFEEu);

    // band-limited single-cycle tables
    auto makeTable = [this] (float tilt, int maxPartials)
    {
        std::vector<float> t (4097, 0.0f);
        const int nP = std::max (1, std::min (maxPartials, (int) ((sr * 0.45) / (kInternalRoot * 1.06))));
        for (int k = 1; k <= nP; ++k)
        {
            const float a = 1.0f / std::pow ((float) k, tilt);
            for (int i = 0; i < 4096; ++i) t[(size_t) i] += a * std::sin (kTwoPi * (float) k * (float) i / 4096.0f);
        }
        t[4096] = t[0];
        return t;
    };
    auto readTable = [] (const std::vector<float>& t, double phase)
    {
        const double p = phase * 4096.0;
        const int i = (int) p; const float f = (float) (p - i);
        return t[(size_t) i] + (t[(size_t) i + 1] - t[(size_t) i]) * f;
    };

    for (int type = 0; type < (int) InternalSource::NumSources; ++type)
    {
        std::vector<float> L ((size_t) len, 0.0f), R ((size_t) len, 0.0f);
        switch ((InternalSource) type)
        {
            case InternalSource::Harmonic:
            {
                const auto table = makeTable (1.0f, 48);
                const double det[3] = { -0.07, 0.0, 0.07 };
                double ph[3] = { 0.0, 0.33, 0.66 };
                for (int i = 0; i < len; ++i)
                {
                    float v[3];
                    for (int k = 0; k < 3; ++k)
                    {
                        ph[k] += kInternalRoot * std::exp2 (det[k] / 12.0) / sr;
                        ph[k] -= std::floor (ph[k]);
                        v[k] = readTable (table, ph[k]);
                    }
                    L[(size_t) i] = v[0] * 0.8f + v[1] * 0.6f + v[2] * 0.3f;
                    R[(size_t) i] = v[2] * 0.8f + v[1] * 0.6f + v[0] * 0.3f;
                }
                break;
            }
            case InternalSource::Vox:
            {
                const auto table = makeTable (1.6f, 64);
                static constexpr float vowels[4][3] = { { 800, 1150, 2900 }, { 450, 800, 2830 }, { 270, 2140, 2950 }, { 400, 1600, 2700 } };
                SVF f[2][3];
                double ph[2] = { 0.0, 0.5 };
                for (int i = 0; i < len; ++i)
                {
                    const double t = (double) i / sr;
                    const float morph = (float) std::fmod (t / 1.0, 4.0);
                    const int v0 = (int) morph % 4, v1 = (v0 + 1) % 4;
                    const float fr = morph - std::floor (morph);
                    float* outs[2] = { &L[(size_t) i], &R[(size_t) i] };
                    for (int c = 0; c < 2; ++c)
                    {
                        const double vib = 0.25 * std::sin (2.0 * kPiD * (5.1 + c * 0.4) * t);
                        ph[c] += kInternalRoot * std::exp2 (vib / 12.0) / sr;
                        ph[c] -= std::floor (ph[c]);
                        const float src = readTable (table, ph[c]);
                        float acc = 0.0f;
                        if ((i & 15) == 0)
                            for (int k = 0; k < 3; ++k)
                                f[c][k].setParams (lerpf (vowels[v0][k], vowels[v1][k], fr), 6.0f + 2.0f * k, sr);
                        for (int k = 0; k < 3; ++k) acc += f[c][k].process (src, SVF::BP) * (k == 0 ? 1.0f : 0.6f / k);
                        *outs[c] = acc;
                    }
                }
                break;
            }
            case InternalSource::Glass:
            {
                static constexpr float ratios[6] = { 1.0f, 2.32f, 4.25f, 6.63f, 9.38f, 12.5f };
                static constexpr float decays[6] = { 1.2f, 1.8f, 2.6f, 3.5f, 4.8f, 6.0f };
                for (int i = 0; i < len; ++i)
                {
                    const double t  = (double) i / sr;
                    const double tl = std::fmod (t, 1.0);
                    float l = 0.0f, rr = 0.0f;
                    for (int k = 0; k < 6; ++k)
                    {
                        const double f = kInternalRoot * ratios[k];
                        if (f > sr * 0.45) continue;
                        const float e = (float) std::exp (-tl * decays[k]) / (1.0f + k * 0.6f);
                        l  += e * (float) std::sin (2.0 * kPiD * f * t);
                        rr += e * (float) std::sin (2.0 * kPiD * f * 1.0015 * t + 0.7);
                    }
                    // soft attack to avoid a click at each strike
                    const float att = (float) std::min (1.0, tl / 0.002);
                    L[(size_t) i] = l * att; R[(size_t) i] = rr * att;
                }
                break;
            }
            case InternalSource::Air:
            case InternalSource::NumSources:
            default:
            {
                SVF bp[2], tone[2];
                for (int c = 0; c < 2; ++c) tone[c].setParams (kInternalRoot * (c ? 2.0f : 1.0f), 18.0f, sr);
                float pink[2] = { 0, 0 };
                for (int i = 0; i < len; ++i)
                {
                    const double t = (double) i / sr;
                    const float sweep = 300.0f * std::pow (20.0f, 0.5f - 0.5f * (float) std::cos (2.0 * kPiD * t / 4.0));
                    float* outs[2] = { &L[(size_t) i], &R[(size_t) i] };
                    for (int c = 0; c < 2; ++c)
                    {
                        const float w = r.nextBipolar();
                        pink[c] = 0.97f * pink[c] + 0.03f * w;
                        if ((i & 15) == 0) bp[c].setParams (sweep * (c ? 1.1f : 0.9f), 1.4f, sr);
                        const float n = bp[c].process (w * 0.6f + pink[c] * 3.0f, SVF::BP);
                        const float tn = tone[c].process (w, SVF::BP);
                        *outs[c] = n + tn * 0.5f;
                    }
                }
                break;
            }
        }
        normalise (L, R, 0.7f);
        makeSeamlessLoop (L, R, extra);
        computeOverview (L, R, internalOverview[(size_t) type]);
        intL[(size_t) type] = std::move (L);
        intR[(size_t) type] = std::move (R);
    }
    internalRenderedRate = sr;
}

//==============================================================================
void GranularEngine::noteOn (int note, float velocity) noexcept
{
    for (int i = 0; i < numHeld; ++i) if (heldNotes[(size_t) i] == note) { heldVel[(size_t) i] = velocity; return; }
    if (numHeld < kMaxNotes)
    {
        heldNotes[(size_t) numHeld] = note;
        heldVel[(size_t) numHeld] = velocity;
        ++numHeld;
    }
    else
    {
        // replace oldest
        for (int i = 1; i < kMaxNotes; ++i) { heldNotes[(size_t) i - 1] = heldNotes[(size_t) i]; heldVel[(size_t) i - 1] = heldVel[(size_t) i]; }
        heldNotes[kMaxNotes - 1] = note; heldVel[kMaxNotes - 1] = velocity;
    }
    numLast = numHeld;
    lastNotes = heldNotes;
}

void GranularEngine::noteOff (int note) noexcept
{
    for (int i = 0; i < numHeld; ++i)
    {
        if (heldNotes[(size_t) i] == note)
        {
            for (int j = i + 1; j < numHeld; ++j) { heldNotes[(size_t) j - 1] = heldNotes[(size_t) j]; heldVel[(size_t) j - 1] = heldVel[(size_t) j]; }
            --numHeld;
            break;
        }
    }
    // keep the last chord for the release tail
    if (numHeld > 0) { numLast = numHeld; lastNotes = heldNotes; }
}

void GranularEngine::allNotesOff() noexcept { numHeld = 0; }

//==============================================================================
void GranularEngine::submitSample (std::unique_ptr<SampleData> s)
{
    if (s == nullptr) return;
    delete pendingSample.exchange (s.release());
}

void GranularEngine::clearSample() { clearRequested.store (true); }

void GranularEngine::collectGarbage()
{
    delete garbageSample.exchange (nullptr);
}

void GranularEngine::handleSampleSwap() noexcept
{
    if (retiringSample != nullptr)
    {
        bool inUse = false;
        for (auto& g : grains) if (g.isActive() && g.getOwner() == retiringSample) { inUse = true; break; }
        if (! inUse && garbageSample.load() == nullptr)
        {
            garbageSample.store (retiringSample);
            retiringSample = nullptr;
        }
    }
    if (retiringSample == nullptr)
    {
        if (clearRequested.exchange (false))
        {
            retiringSample = activeSample;
            activeSample = nullptr;
        }
        else if (auto* p = pendingSample.exchange (nullptr))
        {
            retiringSample = activeSample;
            activeSample = p;
        }
    }
}

//==============================================================================
bool GranularEngine::copyFrozenAudio (std::vector<float>& L, std::vector<float>& R) const
{
    // The capture buffer is not written while frozen, so we can read it without
    // locking. We validate the frozen region before and after copying.
    if (! isFrozen.load() || capLen == 0) return false;
    const int start = freezeRegionStart, len = freezeRegionLen;
    if (len <= 0) return false;
    L.resize ((size_t) len); R.resize ((size_t) len);
    for (int i = 0; i < len; ++i)
    {
        int b = start + i; if (b >= capLen) b -= capLen;
        L[(size_t) i] = capL[(size_t) b];
        R[(size_t) i] = capR[(size_t) b];
    }
    return isFrozen.load() && start == freezeRegionStart && len == freezeRegionLen;
}

void GranularEngine::restoreFrozenAudio (const std::vector<float>& L, const std::vector<float>& R)
{
    if (capLen == 0 || L.empty() || R.size() != L.size()) return;
    while (bufferLock.exchange (true, std::memory_order_acquire)) std::this_thread::yield();

    const int len = std::min ((int) L.size(), capLen);
    std::fill (capL.begin(), capL.end(), 0.0f);
    std::fill (capR.begin(), capR.end(), 0.0f);
    std::copy (L.begin(), L.begin() + len, capL.begin());
    std::copy (R.begin(), R.begin() + len, capR.begin());
    for (auto& g : grains) g.kill();
    writeHead = len % capLen;
    samplesWritten = len;
    freezeRegionStart = 0;
    freezeRegionLen = len;
    frozenState = true;
    isFrozen.store (true);
    for (int b = 0; b < kOverviewBins; ++b)
    {
        const int s0 = (int) ((int64_t) capLen * b / kOverviewBins), s1 = (int) ((int64_t) capLen * (b + 1) / kOverviewBins);
        float pk = 0.0f;
        for (int i = s0; i < s1; ++i) pk = std::max (pk, 0.5f * (std::abs (capL[(size_t) i]) + std::abs (capR[(size_t) i])));
        liveOverview[(size_t) b].store (std::min (1.0f, pk));
    }
    bufferLock.store (false, std::memory_order_release);
}

//==============================================================================
void GranularEngine::engageFreeze() noexcept
{
    const int history = (int) std::min<int64_t> (samplesWritten, capLen);
    int xf = std::min (kLoopXfade, history / 4);
    int loopLen, loopStart;
    if (history < 2048)
    {
        xf = 0;
        loopLen = std::max (history, 512);
        loopStart = writeHead - loopLen;
    }
    else
    {
        loopLen = history - xf;
        loopStart = writeHead - loopLen;
    }
    if (loopStart < 0) loopStart += capLen;

    for (int i = 0; i < xf; ++i)
    {
        const float w = (float) i / (float) xf;
        const float gOut = std::cos (w * kPi * 0.5f), gIn = std::sin (w * kPi * 0.5f);
        int t = loopStart + loopLen - xf + i; t %= capLen;
        int p = loopStart - xf + i; if (p < 0) p += capLen; p %= capLen;
        capL[(size_t) t] = capL[(size_t) t] * gOut + capL[(size_t) p] * gIn;
        capR[(size_t) t] = capR[(size_t) t] * gOut + capR[(size_t) p] * gIn;
    }
    freezeRegionStart = loopStart;
    freezeRegionLen   = loopLen;
    frozenState = true;
    isFrozen.store (true);
}

void GranularEngine::writeInput (const float* inL, const float* inR, int n, const EngineParams& p) noexcept
{
    const bool liveFeedback = p.source == SourceMode::Live;
    const float fb = liveFeedback ? fbSm.getCurrent() : 0.0f;

    for (int i = 0; i < n; ++i)
    {
        float l = inL != nullptr ? inL[i] : 0.0f;
        float r = inR != nullptr ? inR[i] : l;
        if (fb > 0.0001f)
        {
            l += fb * softClip (fbDcL.process (lastOutL[(size_t) i]));
            r += fb * softClip (fbDcR.process (lastOutR[(size_t) i]));
        }
        if (unfreezeFade > 0)
        {
            const float w = 1.0f - (float) unfreezeFade / 1024.0f;
            l = capL[(size_t) writeHead] * (1.0f - w) + l * w;
            r = capR[(size_t) writeHead] * (1.0f - w) + r * w;
            --unfreezeFade;
        }
        capL[(size_t) writeHead] = l;
        capR[(size_t) writeHead] = r;

        // live overview
        const int bin = (int) ((int64_t) writeHead * kOverviewBins / capLen);
        if (bin != overviewBin)
        {
            if (overviewBin >= 0) liveOverview[(size_t) overviewBin].store (std::min (1.0f, overviewPeak));
            overviewBin = bin; overviewPeak = 0.0f;
        }
        overviewPeak = std::max (overviewPeak, 0.5f * (std::abs (l) + std::abs (r)));

        if (++writeHead >= capLen) writeHead = 0;
    }
    if (overviewBin >= 0) liveOverview[(size_t) overviewBin].store (std::min (1.0f, overviewPeak));
    samplesWritten += n;
}

//==============================================================================
float GranularEngine::quantisePitch (float semis, int mode) noexcept
{
    static const int major[] = { 0, 2, 4, 5, 7, 9, 11 };
    static const int minor[] = { 0, 2, 3, 5, 7, 8, 10 };
    static const int penta[] = { 0, 3, 5, 7, 10 };
    static const int fifth[] = { 0, 7 };
    const int* set = nullptr; int count = 0;
    switch ((SpectralMode) mode)
    {
        case SpectralMode::Chromatic:     return std::round (semis);
        case SpectralMode::Major:         set = major; count = 7; break;
        case SpectralMode::Minor:         set = minor; count = 7; break;
        case SpectralMode::Pentatonic:    set = penta; count = 5; break;
        case SpectralMode::FifthsOctaves: set = fifth; count = 2; break;
        case SpectralMode::Off:
        case SpectralMode::Harmonics:
        case SpectralMode::NumModes:
        default: return semis;
    }
    const float oct = std::floor (semis / 12.0f);
    float best = semis, bestDist = 1.0e9f;
    for (int o = -1; o <= 1; ++o)
        for (int k = 0; k < count; ++k)
        {
            const float cand = (oct + (float) o) * 12.0f + (float) set[k];
            const float d = std::abs (cand - semis);
            if (d < bestDist) { bestDist = d; best = cand; }
        }
    return best;
}

void GranularEngine::spawnGrain (int offset, const EngineParams& p, float sizeMs, float rate, float pos, float posRandom,
                                 float pitchSemis, float pitchRandom, float panCentre, float extraPitch, bool companion) noexcept
{
    Grain* g = nullptr;
    for (auto& gr : grains) if (! gr.isActive()) { g = &gr; break; }
    if (g == nullptr) return;

    const float rnd = p.randomness;

    // ---- length --------------------------------------------------------------
    float ms = sizeMs * (1.0f + rnd * 0.5f * rng.nextBipolar());
    ms = clampf (ms, 1.0f, 2000.0f);
    int length = std::max (16, (int) (ms * 0.001f * (float) sr));

    // ---- pitch ---------------------------------------------------------------
    float semis = pitchSemis + extraPitch + pitchRandom * rng.nextBipolar();
    if (p.spectralMode != (int) SpectralMode::Off && rng.nextFloat() < p.spectralAmount)
    {
        if (p.spectralMode == (int) SpectralMode::Harmonics)
        {
            const int maxK = 1 + (int) (p.spectralAmount * 15.0f);
            const int k = 1 + rng.nextInt (maxK);
            semis = pitchSemis + extraPitch + 12.0f * std::log2 ((float) k) - 12.0f; // centred one octave down
        }
        else
            semis = quantisePitch (semis, p.spectralMode);
    }
    if (p.chaos > 0.0f && rng.nextFloat() < p.chaos * 0.15f) semis += rng.nextFloat() < 0.5f ? -12.0f : 12.0f;

    float velGain = 1.0f;
    if (p.midiMode)
    {
        int note = 60; float vel = 1.0f;
        if (numHeld > 0) { const int k = roundRobin % numHeld; note = heldNotes[(size_t) k]; vel = heldVel[(size_t) k]; }
        else if (numLast > 0) { note = lastNotes[(size_t) (roundRobin % numLast)]; vel = 0.8f; }
        ++roundRobin;
        semis += (float) (note - 60);
        velGain = 0.3f + 0.7f * vel;
    }
    semis = clampf (semis, -48.0f, 48.0f);

    // ---- source --------------------------------------------------------------
    GrainSource src;
    double srcRate = sr;
    const auto mode = p.source;
    if (mode == SourceMode::Sample)
    {
        if (activeSample == nullptr || activeSample->L.size() < 64) return;
        src.L = activeSample->L.data(); src.R = activeSample->R.data();
        src.bufLen = src.regionLen = (int) activeSample->L.size();
        src.owner = activeSample;
        srcRate = activeSample->sampleRate;
    }
    else if (mode == SourceMode::Internal)
    {
        const size_t t = (size_t) std::clamp (p.internalType, 0, (int) InternalSource::NumSources - 1);
        if (intL[t].empty()) return;
        src.L = intL[t].data(); src.R = intR[t].data();
        src.bufLen = src.regionLen = (int) intL[t].size();
        src.owner = &intL[t];
    }
    else
    {
        src.L = capL.data(); src.R = capR.data();
        src.bufLen = capLen;
        src.owner = &capL;
        if (frozenState) { src.regionStart = freezeRegionStart; src.regionLen = freezeRegionLen; }
        else             { src.regionStart = 0; src.regionLen = capLen; }
    }

    double ratio = std::exp2 ((double) semis / 12.0) * (srcRate / sr);
    const float revProb = std::max (std::max (p.reverse, p.reverseCloud), p.chaos * 0.2f);
    const bool reversed = rng.nextFloat() < revProb;
    double inc = reversed ? -ratio : ratio;

    const float posN = wrap01 (pos + posRandom * 0.5f * rng.nextBipolar());
    double startPos = 0.0;
    float displayPos = 0.0f;

    if (mode == SourceMode::Live && ! frozenState)
    {
        const double history = (double) std::min<int64_t> (samplesWritten, capLen);
        if (history < 4096.0) return;
        const double rs = inc;
        const double fwd = std::max (0.0, rs - 1.0), back = std::max (0.0, 1.0 - rs);
        double L = (double) length;
        double lo = kLiveMargin + L * fwd;
        double hi = history - kLiveMargin - L * back;
        if (lo > hi)
        {
            L = (history - 2.0 * kLiveMargin) / std::max (1.0e-6, fwd + back) - 1.0;
            if (L < 16.0) return;
            length = (int) L;
            lo = kLiveMargin + L * fwd;
            hi = history - kLiveMargin - L * back;
        }
        const double d0 = std::clamp ((1.0 - (double) posN) * history, lo, std::max (lo, hi));
        double abs = (double) writeHead - d0;
        while (abs < 0.0) abs += capLen;
        startPos = abs;
        displayPos = (float) (abs / capLen);
    }
    else
    {
        startPos = (double) posN * src.regionLen;
        if (mode == SourceMode::Live)
        {
            double a = src.regionStart + startPos; if (a >= capLen) a -= capLen;
            displayPos = (float) (a / capLen);
        }
        else displayPos = posN;
    }

    // ---- envelope / texture --------------------------------------------------
    int shape = p.envelope;
    if (p.reverseCloud > 0.0f && rng.nextFloat() < p.reverseCloud) shape = (int) GrainEnvelope::Swell;
    if (rnd > 0.0f && rng.nextFloat() < rnd * 0.35f) shape = rng.nextInt ((int) GrainEnvelope::NumShapes);

    const float lenSec = (float) length / (float) sr;
    const float overlapCount = std::max (1.0f, rate * lenSec);
    float gain = 1.15f / std::sqrt (overlapCount);
    gain *= (1.0f - rnd * 0.7f * rng.nextFloat()) * velGain * (companion ? 0.7f : 1.0f);

    Grain::StartInfo si;
    si.src = src;
    si.startPos = startPos;
    si.increment = inc;
    si.length = length;
    si.env = envTables.get (shape);
    si.hardness = clampf (p.texture + rnd * 0.3f * rng.nextBipolar(), 0.0f, 1.0f);
    si.gain = gain;
    si.pan = clampf (panCentre + p.spread * rng.nextBipolar(), -1.0f, 1.0f);
    si.startDelay = offset;
    g->start (si);

    if (g->isActive())
        grainEvents.push ({ displayPos, semis, si.pan, lenSec, std::min (1.0f, gain), (uint8_t) (reversed ? 1 : 0), (uint8_t) mode });
}

void GranularEngine::scheduleGrains (int n, const EngineParams& p, float effSize, float effDensity, float effPos, float effJitter,
                                     float effOverlap, float effPitchOffset, float effPan, float effPosRandom, float effPitchRandom) noexcept
{
    const float rate = clampf (effDensity, 0.25f, 1000.0f);
    const float intervalMs = 1000.0f / rate;
    const float sizeMs = clampf (std::max (effSize, effOverlap * 4.0f * intervalMs), 1.0f, 2000.0f);

    bool canSpawn = true;
    if (p.midiMode && numHeld == 0 && p.ampEnv < 1.0e-4f) canSpawn = false;

    const float pitchBase = p.pitch + effPitchOffset;

    while (samplesToNextGrain < (double) n)
    {
        const int off = std::max (0, (int) samplesToNextGrain);
        if (canSpawn)
        {
            spawnGrain (off, p, sizeMs, rate, effPos, effPosRandom, pitchBase, effPitchRandom, effPan, 0.0f, false);

            if (p.thickness > 0.0f)
            {
                if (rng.nextFloat() < p.thickness)
                    spawnGrain (off, p, sizeMs, rate, effPos + 0.004f * rng.nextBipolar(), effPosRandom, pitchBase, effPitchRandom, effPan, (0.05f + 0.12f * rng.nextFloat()) * (rng.nextFloat() < 0.5f ? -1.0f : 1.0f), true);
                if (rng.nextFloat() < p.thickness * 0.5f)
                    spawnGrain (off, p, sizeMs, rate, effPos + 0.01f * rng.nextBipolar(), effPosRandom, pitchBase, effPitchRandom, -effPan, 12.0f * (rng.nextFloat() < 0.5f ? -1.0f : 1.0f) * (p.thickness > 0.7f ? 1.0f : 0.0f) + 0.03f * rng.nextBipolar(), true);
            }
            if (p.drone > 0.0f && rng.nextFloat() < p.drone * 0.6f)
            {
                static constexpr float intervals[4] = { -12.0f, 7.0f, 12.0f, 19.0f };
                spawnGrain (off, p, sizeMs, rate, effPos, effPosRandom, pitchBase, effPitchRandom * 0.2f, effPan, intervals[rng.nextInt (4)], true);
            }
        }
        const double base = sr / (double) rate;
        const double j = 1.0 + (double) effJitter * 0.95 * (double) rng.nextBipolar();
        samplesToNextGrain += std::max (1.0, base * j);
    }
    samplesToNextGrain -= (double) n;
}

//==============================================================================
void GranularEngine::textureStage (float* L, float* R, int n, const EngineParams& p) noexcept
{
    smoothSm.setTarget (p.smoothness);
    diffSm.setTarget (p.diffusion);
    widthSm.setTarget (p.width);
    const float sm = smoothSm.skip (n);
    const float cutoff = 18000.0f * std::pow (0.05f, sm);
    for (auto& f : smoothLP) f.setCutoff (cutoff, sr);

    const float dStart = diffSm.getCurrent();
    const float dEnd   = diffSm.skip (n);
    const float wStart = widthSm.getCurrent();
    const float wEnd   = widthSm.skip (n);

    for (int i = 0; i < n; ++i)
    {
        const float t = (float) (i + 1) / (float) n;
        float l = smoothLP[1].process (smoothLP[0].process (L[i]));
        float r = smoothLP[3].process (smoothLP[2].process (R[i]));

        const float d = lerpf (dStart, dEnd, t);
        const float g = 0.45f + 0.25f * d;
        float dl = l, dr = r;
        for (size_t k = 0; k < 4; ++k) dl = diffusers[k].process (dl, g);
        for (size_t k = 4; k < 8; ++k) dr = diffusers[k].process (dr, g);
        l = lerpf (l, dl, d);
        r = lerpf (r, dr, d);
        L[i] = l; R[i] = r;
    }

    blur.process (L, R, n, p.blur);

    for (int i = 0; i < n; ++i)
    {
        const float w = lerpf (wStart, wEnd, (float) (i + 1) / (float) n);
        const float m = 0.5f * (L[i] + R[i]);
        const float s = 0.5f * (L[i] - R[i]) * w;
        L[i] = m + s; R[i] = m - s;
    }

    // regeneration delay (non-live sources or frozen live buffer)
    const bool liveWrites = p.source == SourceMode::Live && ! frozenState;
    const float fb = fbSm.getCurrent();
    if (! liveWrites && fb > 0.0001f)
    {
        const float dt = clampf (p.sizeMs, 20.0f, 1500.0f) * 0.001f * (float) sr;
        for (int i = 0; i < n; ++i)
        {
            const float yl = regenL.readLinear (dt), yr = regenR.readLinear (dt * 1.07f);
            const float ol = L[i] + yl * fb, orr = R[i] + yr * fb;
            regenL.push (softClip (fbDcL.process (ol)));
            regenR.push (softClip (fbDcR.process (orr)));
            L[i] = ol; R[i] = orr;
        }
    }
    else
    {
        for (int i = 0; i < n; ++i) { regenL.push (0.0f); regenR.push (0.0f); }
    }
}

//==============================================================================
void GranularEngine::process (const float* inL, const float* inR, float* outL, float* outR, int n, const EngineParams& pIn) noexcept
{
    n = std::min (n, kSubBlock);
    if (capLen == 0 || bufferLock.exchange (true, std::memory_order_acquire))
    {
        std::fill (outL, outL + n, 0.0f);
        std::fill (outR, outR + n, 0.0f);
        return;
    }

    handleSampleSwap();
    EngineParams p = pIn;
    const float dt = (float) n / (float) sr;

    // ---- freeze ---------------------------------------------------------------
    const bool wantFreeze = (p.freeze || p.infiniteFreeze);
    if (wantFreeze && ! frozenState)       engageFreeze();
    else if (! wantFreeze && frozenState)  { frozenState = false; isFrozen.store (false); unfreezeFade = 1024; }

    fbSm.setTarget (clampf (p.feedback, 0.0f, 0.95f));
    fbSm.skip (n);
    if (! frozenState) writeInput (inL, inR, n, p);

    // ---- creative modulators -------------------------------------------------
    const float ch = clampf (p.chaos, 0.0f, 1.0f);
    {
        const float changeProb = dt / 0.3f;
        const float slew = 1.0f - std::exp (-dt / 0.08f);
        for (size_t k = 0; k < chaosWalk.size(); ++k)
        {
            if (rng.nextFloat() < changeProb) chaosTarget[k] = rng.nextBipolar();
            chaosWalk[k] += (chaosTarget[k] - chaosWalk[k]) * slew;
        }
    }
    const float ev = clampf (p.evolve, 0.0f, 1.0f);
    evolvePhase += (double) dt * std::max (0.001f, p.evolveRate);
    if (evolvePhase > 1.0e6) evolvePhase = 0.0;
    evolveDrift += (double) dt * p.evolveRate * 0.05 * ev;
    evolveDrift -= std::floor (evolveDrift);
    const float ph = (float) (evolvePhase * 2.0 * kPiD);
    const float s1 = std::sin (ph), s2 = std::sin (ph * 1.618f + 1.0f), s3 = std::sin (ph * 0.707f + 2.0f), s4 = std::sin (ph * 2.414f + 3.0f);

    const float dr = clampf (p.drone, 0.0f, 1.0f);
    const float rc = clampf (p.reverseCloud, 0.0f, 1.0f);

    float effSize = p.sizeMs * std::exp2 (ch * 1.5f * chaosWalk[2] + ev * 1.2f * s2);
    effSize = lerpf (effSize, std::max (effSize, 900.0f), dr) * (1.0f + rc * 0.8f);
    const float effDensity = p.density * std::exp2 (ch * 1.5f * chaosWalk[3]);
    const float effJitter  = std::max (p.jitter, dr * 0.6f);
    const float effOverlap = lerpf (p.overlap, 1.0f, dr);
    const float effPitchOffset = ch * 7.0f * chaosWalk[1];
    const float effPan = clampf (p.pan + ch * 0.6f * chaosWalk[4], -1.0f, 1.0f);
    const float effPosRandom = clampf (p.posRandom + rc * 0.2f + ev * 0.1f * (s3 * 0.5f + 0.5f), 0.0f, 1.0f);
    const float effPitchRandom = p.pitchRandom * (1.0f - dr * 0.8f) + ev * 0.6f * (s4 * 0.5f + 0.5f);

    // scan (Direction). In Sample/Internal modes "Freeze" holds the scan.
    int regionLenNow = capLen;
    if (p.source == SourceMode::Sample)        regionLenNow = activeSample != nullptr ? (int) activeSample->L.size() : 0;
    else if (p.source == SourceMode::Internal) regionLenNow = (int) intL[(size_t) std::clamp (p.internalType, 0, 3)].size();
    else if (frozenState)                      regionLenNow = freezeRegionLen;
    else                                       regionLenNow = (int) std::min<int64_t> (samplesWritten, capLen);

    float scanSpeed = p.direction * 2.0f * (1.0f - dr * 0.85f);
    if (p.source != SourceMode::Live && wantFreeze) scanSpeed = 0.0f;
    if (regionLenNow > 0) scanPhase = wrap01 (scanPhase + scanSpeed * (float) n / (float) regionLenNow);

    const float effPos = wrap01 (p.position + scanPhase + ch * 0.35f * chaosWalk[0] + ev * 0.3f * s1 + (float) evolveDrift);

    p.blur       = clampf (p.blur + ev * 0.35f * (s3 * 0.5f + 0.5f), 0.0f, 1.0f);
    p.diffusion  = clampf (p.diffusion + dr * 0.4f + ev * 0.2f * (s2 * 0.5f + 0.5f), 0.0f, 1.0f);
    p.smoothness = clampf (p.smoothness + dr * 0.25f, 0.0f, 1.0f);
    p.chaos = ch; p.drone = dr; p.reverseCloud = rc;

    // ---- grains -----------------------------------------------------------------
    std::fill (tmpL.begin(), tmpL.begin() + n, 0.0f);
    std::fill (tmpR.begin(), tmpR.begin() + n, 0.0f);

    scheduleGrains (n, p, effSize, effDensity, effPos, effJitter, effOverlap, effPitchOffset, effPan, effPosRandom, effPitchRandom);

    int active = 0;
    for (auto& g : grains)
        if (g.isActive()) { g.render (tmpL.data(), tmpR.data(), n); ++active; }

    textureStage (tmpL.data(), tmpR.data(), n, p);

    // ---- safety + output --------------------------------------------------------
    float energy = 0.0f;
    for (int i = 0; i < n; ++i) energy += tmpL[(size_t) i] + tmpR[(size_t) i];
    if (! std::isfinite (energy))
    {
        for (auto& g : grains) g.kill();
        std::fill (tmpL.begin(), tmpL.end(), 0.0f);
        std::fill (tmpR.begin(), tmpR.end(), 0.0f);
        blur.reset(); regenL.clear(); regenR.clear();
        for (auto& d : diffusers) d.clear();
        for (auto& f : smoothLP) f.reset();
    }
    std::copy (tmpL.begin(), tmpL.begin() + n, outL);
    std::copy (tmpR.begin(), tmpR.begin() + n, outR);
    std::copy (tmpL.begin(), tmpL.begin() + n, lastOutL.begin());
    std::copy (tmpR.begin(), tmpR.begin() + n, lastOutR.begin());

    // ---- visual state -----------------------------------------------------------
    activeGrainCount.store (active, std::memory_order_relaxed);
    displaySource.store ((int) p.source, std::memory_order_relaxed);
    {
        double scale = sr / std::max (1, capLen);
        if (p.source == SourceMode::Sample && activeSample != nullptr) scale = activeSample->sampleRate / std::max<size_t> (1, activeSample->L.size());
        else if (p.source == SourceMode::Internal) scale = sr / std::max (1, regionLenNow);
        displayRateScale.store ((float) scale, std::memory_order_relaxed);
    }
    writeHeadNorm.store ((float) writeHead / (float) capLen, std::memory_order_relaxed);
    if (p.source == SourceMode::Live)
    {
        if (frozenState)
        {
            regionStartNorm.store ((float) freezeRegionStart / (float) capLen, std::memory_order_relaxed);
            regionLenNorm.store ((float) freezeRegionLen / (float) capLen, std::memory_order_relaxed);
            playheadNorm.store (wrap01 (((float) freezeRegionStart + effPos * (float) freezeRegionLen) / (float) capLen), std::memory_order_relaxed);
        }
        else
        {
            const float hist = (float) std::min<int64_t> (samplesWritten, capLen);
            regionStartNorm.store (wrap01 (((float) writeHead - hist) / (float) capLen), std::memory_order_relaxed);
            regionLenNorm.store (hist / (float) capLen, std::memory_order_relaxed);
            playheadNorm.store (wrap01 (((float) writeHead - (1.0f - effPos) * hist) / (float) capLen), std::memory_order_relaxed);
        }
    }
    else
    {
        regionStartNorm.store (0.0f, std::memory_order_relaxed);
        regionLenNorm.store (1.0f, std::memory_order_relaxed);
        playheadNorm.store (effPos, std::memory_order_relaxed);
    }

    bufferLock.store (false, std::memory_order_release);
}

} // namespace nubis
