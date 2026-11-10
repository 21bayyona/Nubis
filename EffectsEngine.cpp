// =============================================================================
//  NUBIS - EffectsEngine.cpp
// =============================================================================
#include "EffectsEngine.h"

namespace nubis
{
//==============================================================================
//  REVERB : 8-line feedback delay network, Hadamard mixing, damped, modulated
//==============================================================================
void FxReverbUnit::prepare (double sampleRate)
{
    sr = sampleRate;
    const int maxLen = (int) (73.1 * 1.6 * 0.001 * sr) + 64;
    for (auto& l : lines) l.allocate (maxLen);
    static constexpr float diffMs[4] = { 4.77f, 3.59f, 5.31f, 2.93f };
    for (size_t i = 0; i < 4; ++i) { inDiff[i].allocate ((int) (0.01 * sr)); inDiff[i].setDelay ((int) (diffMs[i] * 0.001f * (float) sr)); }
    preL.allocate ((int) (0.25 * sr));
    preR.allocate ((int) (0.25 * sr));
    sizeSm.reset (sr, 300.0f, 0.6f);
    mixSm.reset (sr, 30.0f, 0.0f);
    reset();
}

void FxReverbUnit::reset()
{
    for (auto& l : lines) l.clear();
    for (auto& d : damp) d.reset();
    for (auto& a : inDiff) a.clear();
    preL.clear(); preR.clear();
    lfoPhase = 0.0f;
}

void FxReverbUnit::process (float* L, float* R, int n, const FxParams& p) noexcept
{
    sizeSm.setTarget (clampf (p.revSize, 0.0f, 1.0f));
    mixSm.setTarget (clampf (p.revMix, 0.0f, 1.0f));
    const float size  = sizeSm.skip (n);
    const float scale = 0.4f + size * 1.2f;
    const float T     = clampf (p.revDecay, 0.2f, 30.0f);
    const float dampHz = 800.0f + 19000.0f * (1.0f - p.revDamp) * (1.0f - p.revDamp);
    const float pre = std::max (1.0f, p.revPredelayMs * 0.001f * (float) sr);

    std::array<float, kLines> len {}, g {};
    for (size_t i = 0; i < (size_t) kLines; ++i)
    {
        len[i] = baseMs[i] * scale * 0.001f * (float) sr;
        g[i]   = std::pow (10.0f, -3.0f * len[i] / (T * (float) sr));
        damp[i].setCutoff (dampHz, sr);
    }

    const float lfoInc = 0.37f / (float) sr;
    for (int s = 0; s < n; ++s)
    {
        const float mix = mixSm.next();
        preL.push (L[s]); preR.push (R[s]);
        float il = preL.readLinear (pre), ir = preR.readLinear (pre);
        il = inDiff[1].process (inDiff[0].process (il, 0.6f), 0.6f);
        ir = inDiff[3].process (inDiff[2].process (ir, 0.6f), 0.6f);

        lfoPhase += lfoInc; if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
        const float ms = std::sin (lfoPhase * kTwoPi), mc = std::cos (lfoPhase * kTwoPi);

        std::array<float, kLines> y {};
        for (size_t i = 0; i < (size_t) kLines; ++i)
        {
            const float mod = (i & 1 ? ms : mc) * (i < 4 ? 6.0f : 4.0f);
            y[i] = damp[i].process (lines[i].readLinear (len[i] + mod + 8.0f)) * g[i];
        }
        const float wl = (y[0] - y[2] + y[4] - y[6]) * 0.5f;
        const float wr = (y[1] - y[3] + y[5] - y[7]) * 0.5f;

        // fast Hadamard transform
        for (int h = 1; h < kLines; h <<= 1)
            for (int i = 0; i < kLines; i += h * 2)
                for (int j = i; j < i + h; ++j)
                {
                    const float a = y[(size_t) j], b = y[(size_t) (j + h)];
                    y[(size_t) j] = a + b; y[(size_t) (j + h)] = a - b;
                }
        for (size_t i = 0; i < (size_t) kLines; ++i)
            lines[i].push (y[i] * 0.35355339f + ((i & 1) ? ir : il) * 0.5f);

        const float dg = std::cos (mix * kPi * 0.5f), wg = std::sin (mix * kPi * 0.5f);
        L[s] = L[s] * dg + wl * wg;
        R[s] = R[s] * dg + wr * wg;
    }
}

//==============================================================================
//  DELAY : stereo / ping-pong, tempo sync, tone in the loop
//==============================================================================
void FxDelayUnit::prepare (double sampleRate)
{
    sr = sampleRate;
    dL.allocate ((int) (2.1 * sr));
    dR.allocate ((int) (2.1 * sr));
    dcL.prepare (sr); dcR.prepare (sr);
    timeSm.reset (sr, 180.0f, 350.0f);
    mixSm.reset (sr, 30.0f, 0.0f);
    fbSm.reset (sr, 30.0f, 0.0f);
    reset();
}

void FxDelayUnit::reset()
{
    dL.clear(); dR.clear(); toneL.reset(); toneR.reset(); dcL.reset(); dcR.reset();
}

void FxDelayUnit::process (float* L, float* R, int n, const FxParams& p) noexcept
{
    float ms = p.dlyTimeMs;
    if (p.dlySync) ms = (float) (60000.0 / std::max (20.0, p.bpm) * divisionToBeats (p.dlyDivision));
    ms = clampf (ms, 1.0f, 2000.0f);
    timeSm.setTarget (ms * 0.001f * (float) sr);
    mixSm.setTarget (clampf (p.dlyMix, 0.0f, 1.0f));
    fbSm.setTarget (clampf (p.dlyFeedback, 0.0f, 0.97f));
    const float toneHz = 600.0f * std::pow (30.0f, clampf (p.dlyTone, 0.0f, 1.0f));
    toneL.setCutoff (toneHz, sr); toneR.setCutoff (toneHz, sr);
    const float pp = clampf (p.dlyPingPong, 0.0f, 1.0f);

    for (int s = 0; s < n; ++s)
    {
        const float t = std::max (2.0f, timeSm.next());
        const float mix = mixSm.next(), fb = fbSm.next();
        const float yl = dL.readCubic (t), yr = dR.readCubic (t);
        const float fbL = lerpf (yl, yr, pp), fbR = lerpf (yr, yl, pp);
        const float inL = lerpf (L[s], 0.5f * (L[s] + R[s]), pp);
        const float inR = lerpf (R[s], 0.0f, pp);
        dL.push (softClip (dcL.process (inL + fb * toneL.process (fbL))));
        dR.push (softClip (dcR.process (inR + fb * toneR.process (fbR))));
        L[s] = L[s] * (1.0f - mix * 0.5f) + yl * mix;
        R[s] = R[s] * (1.0f - mix * 0.5f) + yr * mix;
    }
}

//==============================================================================
//  DISTORTION : 2x oversampled waveshaper, latency-aligned dry path
//==============================================================================
void FxDistortionUnit::prepare (double sampleRate)
{
    sr = sampleRate;
    for (auto& d : dry) d.allocate (64);
    for (auto& d : dc) d.prepare (sr);
    driveSm.reset (sr, 30.0f, 0.3f);
    mixSm.reset (sr, 30.0f, 1.0f);
    reset();
}

void FxDistortionUnit::reset()
{
    for (auto& o : os) o.reset();
    for (auto& d : dry) d.clear();
    for (auto& t : tone) t.reset();
    for (auto& d : dc) d.reset();
}

float FxDistortionUnit::shape (float x, int type) const noexcept
{
    switch (type)
    {
        case 1:  return clampf (x, -1.0f, 1.0f);
        case 2:  return std::sin (clampf (x, -12.0f, 12.0f) * 1.25f);
        case 3:  return fastTanh (x + 0.25f) - 0.244918662f; // tanh(0.25)
        case 0:
        default: return fastTanh (x);
    }
}

void FxDistortionUnit::process (float* L, float* R, int n, const FxParams& p) noexcept
{
    driveSm.setTarget (clampf (p.distDrive, 0.0f, 1.0f));
    mixSm.setTarget (clampf (p.distMix, 0.0f, 1.0f));
    const float toneHz = 1200.0f * std::pow (15.0f, clampf (p.distTone, 0.0f, 1.0f));
    for (auto& t : tone) t.setCutoff (toneHz, sr);
    float* io[2] = { L, R };

    for (int s = 0; s < n; ++s)
    {
        const float d = driveSm.next(), mix = mixSm.next();
        const float gain = dbToGain (d * 36.0f);
        const float makeup = 1.0f / std::sqrt (gain);
        for (size_t c = 0; c < 2; ++c)
        {
            const float x = io[c][s];
            float u0, u1;
            os[c].upsample (x * gain, u0, u1);
            const float y = os[c].downsample (shape (u0, p.distType), shape (u1, p.distType));
            const float wet = dc[c].process (tone[c].process (y)) * makeup * 1.4f;
            dry[c].push (x);
            const float dr = dry[c].readInt (Oversampler2x::kLatency + 1);
            io[c][s] = dr + (wet - dr) * mix;
        }
    }
}

//==============================================================================
//  BITCRUSHER
//==============================================================================
void FxBitcrusherUnit::prepare (double sampleRate)
{
    sr = sampleRate;
    bitsSm.reset (sr, 20.0f, 8.0f);
    mixSm.reset (sr, 20.0f, 1.0f);
    reset();
}
void FxBitcrusherUnit::reset() { phase = 0.0f; heldL = heldR = 0.0f; }

void FxBitcrusherUnit::process (float* L, float* R, int n, const FxParams& p) noexcept
{
    bitsSm.setTarget (clampf (p.crushBits, 1.0f, 16.0f));
    mixSm.setTarget (clampf (p.crushMix, 0.0f, 1.0f));
    const float factor = clampf (p.crushDownsample, 1.0f, 64.0f) * (float) (sr / 44100.0 > 1.5 ? sr / 44100.0 : 1.0);
    for (int s = 0; s < n; ++s)
    {
        const float levels = std::exp2 (bitsSm.next() - 1.0f);
        const float mix = mixSm.next();
        phase += 1.0f;
        if (phase >= factor)
        {
            phase -= factor;
            heldL = std::round (clampf (L[s], -1.0f, 1.0f) * levels) / levels;
            heldR = std::round (clampf (R[s], -1.0f, 1.0f) * levels) / levels;
        }
        L[s] += (heldL - L[s]) * mix;
        R[s] += (heldR - R[s]) * mix;
    }
}

//==============================================================================
//  FILTER : multimode TPT state-variable filter with drive
//==============================================================================
void FxFilterUnit::prepare (double sampleRate)
{
    sr = sampleRate;
    cutSm.reset (sr, 25.0f, std::log2 (8000.0f));
    resSm.reset (sr, 25.0f, 0.2f);
    driveSm.reset (sr, 25.0f, 0.0f);
    reset();
}
void FxFilterUnit::reset() { for (auto& f : svf) f.reset(); }

void FxFilterUnit::process (float* L, float* R, int n, const FxParams& p) noexcept
{
    cutSm.setTarget (std::log2 (clampf (p.filtCutoff, 20.0f, 20000.0f)));
    resSm.setTarget (clampf (p.filtReso, 0.0f, 1.0f));
    driveSm.setTarget (clampf (p.filtDrive, 0.0f, 1.0f));
    float* io[2] = { L, R };
    for (int s = 0; s < n; ++s)
    {
        const float lc = cutSm.next(), rs = resSm.next(), dr = driveSm.next();
        if ((s & 3) == 0)
        {
            const float q = 0.5f * std::pow (40.0f, rs);
            for (auto& f : svf) f.setParams (std::exp2 (lc), q, sr);
        }
        const float g = 1.0f + dr * 7.0f;
        for (size_t c = 0; c < 2; ++c)
        {
            float x = io[c][s];
            if (dr > 0.001f) x = fastTanh (x * g) / std::sqrt (g);
            io[c][s] = svf[c].process (x, p.filtMode);
        }
    }
}

//==============================================================================
//  CHORUS : 3 modulated voices per channel
//==============================================================================
void FxChorusUnit::prepare (double sampleRate)
{
    sr = sampleRate;
    dL.allocate ((int) (0.05 * sr));
    dR.allocate ((int) (0.05 * sr));
    depthSm.reset (sr, 40.0f, 0.5f);
    mixSm.reset (sr, 30.0f, 0.5f);
    reset();
}
void FxChorusUnit::reset() { dL.clear(); dR.clear(); phase = 0.0; }

void FxChorusUnit::process (float* L, float* R, int n, const FxParams& p) noexcept
{
    depthSm.setTarget (clampf (p.chDepth, 0.0f, 1.0f));
    mixSm.setTarget (clampf (p.chMix, 0.0f, 1.0f));
    const double inc = clampf (p.chRate, 0.02f, 8.0f) / sr;
    const float msToS = 0.001f * (float) sr;
    static constexpr float rateMul[3] = { 1.0f, 1.13f, 0.87f };
    static constexpr float off[3] = { 0.0f, 0.33f, 0.66f };
    for (int s = 0; s < n; ++s)
    {
        const float depth = depthSm.next(), mix = mixSm.next();
        dL.push (L[s]); dR.push (R[s]);
        phase += inc; if (phase >= 100.0) phase -= 100.0;
        float wl = 0.0f, wr = 0.0f;
        for (int v = 0; v < 3; ++v)
        {
            const float ph = (float) (phase * rateMul[v]) + off[v];
            const float ml = std::sin (ph * kTwoPi), mr = std::sin ((ph + 0.25f) * kTwoPi);
            wl += dL.readLinear ((9.0f + depth * 7.0f * (1.0f + ml)) * msToS);
            wr += dR.readLinear ((9.0f + depth * 7.0f * (1.0f + mr)) * msToS);
        }
        wl *= 0.4f; wr *= 0.4f;
        L[s] = L[s] * (1.0f - mix * 0.5f) + wl * mix;
        R[s] = R[s] * (1.0f - mix * 0.5f) + wr * mix;
    }
}

//==============================================================================
//  PHASER : 6 first-order allpass stages with feedback
//==============================================================================
void FxPhaserUnit::prepare (double sampleRate)
{
    sr = sampleRate;
    mixSm.reset (sr, 30.0f, 0.5f);
    reset();
}
void FxPhaserUnit::reset() { for (auto& c : z) c.fill (0.0f); fbState.fill (0.0f); phase = 0.0; }

void FxPhaserUnit::process (float* L, float* R, int n, const FxParams& p) noexcept
{
    mixSm.setTarget (clampf (p.phMix, 0.0f, 1.0f));
    const double inc = clampf (p.phRate, 0.01f, 10.0f) / sr;
    const float depth = clampf (p.phDepth, 0.0f, 1.0f);
    const float fb = clampf (p.phFeedback, 0.0f, 0.9f);
    float* io[2] = { L, R };
    float a[2] = { 0.0f, 0.0f };
    for (int s = 0; s < n; ++s)
    {
        phase += inc; if (phase >= 1.0) phase -= 1.0;
        if ((s & 7) == 0)
        {
            for (int c = 0; c < 2; ++c)
            {
                const float lfo = std::sin ((float) (phase + c * 0.25) * kTwoPi) * 0.5f + 0.5f;
                const float f = 180.0f * std::exp2 (lfo * depth * 5.0f);
                const float t = std::tan (kPi * std::min (f, (float) sr * 0.45f) / (float) sr);
                a[c] = (t - 1.0f) / (t + 1.0f);
            }
        }
        const float mix = mixSm.next();
        for (size_t c = 0; c < 2; ++c)
        {
            const float dry = io[c][s];
            float x = dry + fb * fbState[c];
            for (size_t k = 0; k < (size_t) kStages; ++k)
            {
                const float y = a[c] * x + z[c][k];
                z[c][k] = x - a[c] * y;
                x = y;
            }
            fbState[c] = softClip (x);
            io[c][s] = dry + (x - dry) * mix;
        }
    }
}

//==============================================================================
//  FREQUENCY SHIFTER : Hilbert analytic signal * complex oscillator
//==============================================================================
void FxFreqShifterUnit::prepare (double sampleRate)
{
    sr = sampleRate;
    shiftSm.reset (sr, 40.0f, 0.0f);
    mixSm.reset (sr, 30.0f, 0.5f);
    reset();
}
void FxFreqShifterUnit::reset() { for (auto& h : hil) h.reset(); fbState.fill (0.0f); phase = 0.0; }

void FxFreqShifterUnit::process (float* L, float* R, int n, const FxParams& p) noexcept
{
    shiftSm.setTarget (clampf (p.fsShiftHz, -5000.0f, 5000.0f));
    mixSm.setTarget (clampf (p.fsMix, 0.0f, 1.0f));
    const float fb = clampf (p.fsFeedback, 0.0f, 0.9f);
    float* io[2] = { L, R };
    for (int s = 0; s < n; ++s)
    {
        const float shift = shiftSm.next(), mix = mixSm.next();
        phase += shift / sr;
        phase -= std::floor (phase);
        const float cs = std::cos ((float) phase * kTwoPi), sn = std::sin ((float) phase * kTwoPi);
        for (size_t c = 0; c < 2; ++c)
        {
            const float dry = io[c][s];
            float re, im;
            hil[c].process (dry + fb * fbState[c], re, im);
            const float y = re * cs + im * sn;   // upper sideband (positive shift = up)
            fbState[c] = softClip (y);
            io[c][s] = dry + (y - dry) * mix;
        }
    }
}

//==============================================================================
//  IMAGER : M/S width + mono bass
//==============================================================================
void FxImagerUnit::prepare (double sampleRate)
{
    sr = sampleRate;
    widthSm.reset (sr, 30.0f, 1.0f);
    reset();
}
void FxImagerUnit::reset() { sideHP.reset(); }

void FxImagerUnit::process (float* L, float* R, int n, const FxParams& p) noexcept
{
    widthSm.setTarget (clampf (p.imgWidth, 0.0f, 2.0f));
    const bool monoBass = p.imgMonoBassHz > 20.0f;
    if (monoBass) sideHP.setParams (p.imgMonoBassHz, 0.707f, sr);
    for (int s = 0; s < n; ++s)
    {
        const float w = widthSm.next();
        const float m = 0.5f * (L[s] + R[s]);
        float sd = 0.5f * (L[s] - R[s]);
        if (monoBass) sd = sideHP.process (sd, SVF::HP);
        sd *= w;
        L[s] = m + sd; R[s] = m - sd;
    }
}

//==============================================================================
//  ENGINE
//==============================================================================
void EffectsEngine::prepare (double sampleRate, int)
{
    sr = sampleRate;
    reverb.prepare (sr); delay.prepare (sr); dist.prepare (sr); crush.prepare (sr);
    filter.prepare (sr); chorus.prepare (sr); phaser.prepare (sr); shifter.prepare (sr); imager.prepare (sr);
    for (auto& s : onSm) s.reset (sr, 15.0f, 0.0f);
    running.fill (false);
}

void EffectsEngine::reset()
{
    reverb.reset(); delay.reset(); dist.reset(); crush.reset();
    filter.reset(); chorus.reset(); phaser.reset(); shifter.reset(); imager.reset();
}

void EffectsEngine::runFx (int id, float* L, float* R, int n, const FxParams& p) noexcept
{
    switch (id)
    {
        case FxReverb:      reverb.process (L, R, n, p); break;
        case FxDelay:       delay.process (L, R, n, p); break;
        case FxDistortion:  dist.process (L, R, n, p); break;
        case FxBitcrusher:  crush.process (L, R, n, p); break;
        case FxFilter:      filter.process (L, R, n, p); break;
        case FxChorus:      chorus.process (L, R, n, p); break;
        case FxPhaser:      phaser.process (L, R, n, p); break;
        case FxFreqShifter: shifter.process (L, R, n, p); break;
        case FxImager:      imager.process (L, R, n, p); break;
        default: break;
    }
}

void EffectsEngine::process (float* L, float* R, int n, const FxParams& p) noexcept
{
    n = std::min (n, kSubBlock);
    for (int k = 0; k < NumFx; ++k)
    {
        const int id = p.order[(size_t) k];
        if (id < 0 || id >= NumFx) continue;
        auto& sm = onSm[(size_t) id];
        sm.setTarget (p.enabled[(size_t) id] ? 1.0f : 0.0f);
        const float startOn = sm.getCurrent();
        const float endOn = sm.skip (n);
        if (startOn < 1.0e-4f && endOn < 1.0e-4f)
        {
            if (! p.enabled[(size_t) id]) { running[(size_t) id] = false; sm.snap (0.0f); }
            continue;
        }
        if (! running[(size_t) id])
        {
            switch (id)
            {
                case FxReverb: reverb.reset(); break;      case FxDelay: delay.reset(); break;
                case FxDistortion: dist.reset(); break;    case FxBitcrusher: crush.reset(); break;
                case FxFilter: filter.reset(); break;      case FxChorus: chorus.reset(); break;
                case FxPhaser: phaser.reset(); break;      case FxFreqShifter: shifter.reset(); break;
                case FxImager: imager.reset(); break;      default: break;
            }
            running[(size_t) id] = true;
        }
        std::copy (L, L + n, dryL.begin());
        std::copy (R, R + n, dryR.begin());
        runFx (id, L, R, n, p);
        if (startOn < 0.9999f || endOn < 0.9999f)
        {
            for (int i = 0; i < n; ++i)
            {
                const float on = lerpf (startOn, endOn, (float) (i + 1) / (float) n);
                L[i] = dryL[(size_t) i] + (L[i] - dryL[(size_t) i]) * on;
                R[i] = dryR[(size_t) i] + (R[i] - dryR[(size_t) i]) * on;
            }
        }
    }
}

} // namespace nubis
