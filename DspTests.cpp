// =============================================================================
//  NUBIS - DspTests.cpp : standalone tests for the DSP core (no JUCE needed)
//  Build:  cmake -S . -B build -DNUBIS_BUILD_TESTS=ON && cmake --build build --target NubisDspTests
// =============================================================================
#include "../Source/dsp/GranularEngine.h"
#include "../Source/dsp/ModulationEngine.h"
#include "../Source/dsp/EffectsEngine.h"

#include <chrono>
#include <cstdio>
#include <functional>

using namespace nubis;

static int failures = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++failures; std::printf ("  FAIL: " __VA_ARGS__); std::printf ("\n"); } } while (0)

static double goertzel (const std::vector<float>& x, double freq, double sr, size_t start)
{
    const double w = 2.0 * kPiD * freq / sr, c = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0;
    for (size_t i = start; i < x.size(); ++i) { const double s = x[i] + c * s1 - s2; s2 = s1; s1 = s; }
    return std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2) / (double) (x.size() - start);
}

struct RunStats { float peak = 0; float maxStep = 0; bool finite = true; double rms = 0; };

static RunStats runEngine (GranularEngine& e, const EngineParams& p, double sr, double seconds,
                           std::function<float (int64_t)> input, bool useInput = true)
{
    RunStats st;
    const int64_t total = (int64_t) (seconds * sr);
    float inL[kSubBlock], inR[kSubBlock], oL[kSubBlock], oR[kSubBlock];
    float prev = 0;
    double acc = 0;
    for (int64_t pos = 0; pos < total; pos += kSubBlock)
    {
        const int n = (int) std::min<int64_t> (kSubBlock, total - pos);
        for (int i = 0; i < n; ++i) { inL[i] = inR[i] = input (pos + i); }
        e.process (useInput ? inL : nullptr, useInput ? inR : nullptr, oL, oR, n, p);
        for (int i = 0; i < n; ++i)
        {
            if (! std::isfinite (oL[i]) || ! std::isfinite (oR[i])) st.finite = false;
            st.peak = std::max ({ st.peak, std::abs (oL[i]), std::abs (oR[i]) });
            st.maxStep = std::max (st.maxStep, std::abs (oL[i] - prev));
            prev = oL[i];
            acc += oL[i] * oL[i];
        }
    }
    st.rms = std::sqrt (acc / (double) total);
    return st;
}

int main()
{
    const double sr = 48000.0;
    std::printf ("NUBIS DSP tests @ %.0f Hz\n", sr);

    //--------------------------------------------------------------------------
    std::printf ("[1] Oversampler latency\n");
    {
        Oversampler2x os;
        int peakIdx = -1; float peak = 0;
        for (int i = 0; i < 100; ++i)
        {
            float a, b; os.upsample (i == 0 ? 1.0f : 0.0f, a, b);
            const float y = os.downsample (a, b);
            if (std::abs (y) > peak) { peak = std::abs (y); peakIdx = i; }
        }
        std::printf ("  impulse peak at %d (expected %d), gain %.3f\n", peakIdx, Oversampler2x::kLatency, peak);
        CHECK (peakIdx == Oversampler2x::kLatency, "oversampler latency mismatch");
        // DC gain
        Oversampler2x os2; float y = 0;
        for (int i = 0; i < 200; ++i) { float a, b; os2.upsample (1.0f, a, b); y = os2.downsample (a, b); }
        CHECK (std::abs (y - 1.0f) < 0.01f, "oversampler DC gain %.4f", y);
    }

    //--------------------------------------------------------------------------
    std::printf ("[2] Hilbert / frequency shifter sideband suppression\n");
    {
        FxFreqShifterUnit fs; fs.prepare (sr);
        FxParams p; p.fsShiftHz = 200.0f; p.fsMix = 1.0f;
        std::vector<float> out; out.reserve ((size_t) sr);
        float L[kSubBlock], R[kSubBlock];
        for (int pos = 0; pos < (int) sr; pos += kSubBlock)
        {
            for (int i = 0; i < kSubBlock; ++i) L[i] = R[i] = 0.5f * std::sin (2.0f * kPi * 1000.0f * (float) (pos + i) / (float) sr);
            fs.process (L, R, kSubBlock, p);
            out.insert (out.end(), L, L + kSubBlock);
        }
        const double up = goertzel (out, 1200.0, sr, 8000), img = goertzel (out, 800.0, sr, 8000), orig = goertzel (out, 1000.0, sr, 8000);
        std::printf ("  1200Hz %.4f  800Hz(image) %.5f  1000Hz %.5f  -> suppression %.1f dB\n", up, img, orig, 20 * std::log10 (up / std::max (1e-9, img)));
        CHECK (up > 10.0 * img, "image not suppressed");
        CHECK (up > 10.0 * orig, "carrier leaks");
    }

    //--------------------------------------------------------------------------
    std::printf ("[3] Spectral blur: identity at 0 (latency-aligned), stable at 1\n");
    {
        SpectralBlur b; b.prepare (sr);
        std::vector<float> inp, outp;
        float L[kSubBlock], R[kSubBlock];
        Random r;
        for (int pos = 0; pos < 48000; pos += kSubBlock)
        {
            for (int i = 0; i < kSubBlock; ++i) { L[i] = R[i] = r.nextBipolar() * 0.5f; inp.push_back (L[i]); }
            b.process (L, R, kSubBlock, 0.0f);
            outp.insert (outp.end(), L, L + kSubBlock);
        }
        double err = 0;
        for (size_t i = SpectralBlur::kN; i < outp.size(); ++i) err = std::max (err, (double) std::abs (outp[i] - inp[i - SpectralBlur::kN]));
        std::printf ("  max error vs delayed input: %.2e\n", err);
        CHECK (err < 1e-5, "blur bypass not transparent");

        float pk = 0; bool fin = true;
        for (int pos = 0; pos < 96000; pos += kSubBlock)
        {
            for (int i = 0; i < kSubBlock; ++i) L[i] = R[i] = 0.5f * std::sin (2.0f * kPi * 440.0f * (float) (pos + i) / (float) sr);
            b.process (L, R, kSubBlock, 1.0f);
            for (int i = 0; i < kSubBlock; ++i) { pk = std::max (pk, std::abs (L[i])); fin &= std::isfinite (L[i]); }
        }
        std::printf ("  full blur peak %.3f\n", pk);
        CHECK (fin && pk < 2.0f, "blur unstable");
        CHECK (pk > 0.05f, "blur silent");
    }

    //--------------------------------------------------------------------------
    std::printf ("[4] Granular engine - live input, default settings\n");
    GranularEngine eng;
    eng.prepare (sr, 512);
    auto sine = [sr] (int64_t i) { return 0.5f * std::sin (2.0f * kPi * 220.0f * (float) i / (float) sr); };
    {
        EngineParams p;
        auto st = runEngine (eng, p, sr, 4.0, sine);
        std::printf ("  peak %.3f rms %.4f finite %d grains %d\n", st.peak, st.rms, st.finite, eng.activeGrainCount.load());
        CHECK (st.finite, "non finite output");
        CHECK (st.rms > 0.01, "engine silent");
        CHECK (st.peak < 2.0f, "engine too loud");
    }

    //--------------------------------------------------------------------------
    std::printf ("[5] Extreme settings sweep (size/density/pitch/feedback)\n");
    {
        const float sizes[] = { 1.0f, 20.0f, 300.0f, 2000.0f };
        const float dens[]  = { 0.5f, 30.0f, 400.0f };
        const float pitches[] = { -24.0f, 0.0f, 24.0f };
        float worst = 0; bool fin = true; double minRms = 1e9;
        for (float s : sizes) for (float d : dens) for (float pt : pitches)
        {
            EngineParams p; p.sizeMs = s; p.density = d; p.pitch = pt; p.reverse = 0.5f; p.feedback = 0.9f;
            p.posRandom = 1.0f; p.pitchRandom = 24.0f; p.overlap = 1.0f; p.randomness = 1.0f; p.thickness = 1.0f;
            auto st = runEngine (eng, p, sr, 0.6, sine);
            worst = std::max (worst, st.peak); fin &= st.finite; minRms = std::min (minRms, st.rms);
        }
        std::printf ("  worst peak %.3f finite %d min rms %.5f\n", worst, fin, minRms);
        CHECK (fin, "non finite in sweep");
        CHECK (worst < 4.0f, "excessive level in sweep");
    }

    //--------------------------------------------------------------------------
    std::printf ("[6] Infinite freeze keeps sounding after input stops\n");
    {
        GranularEngine e2; e2.prepare (sr, 512);
        EngineParams p; p.sizeMs = 200; p.density = 30; p.overlap = 1.0f;
        runEngine (e2, p, sr, 3.0, sine);
        p.infiniteFreeze = true;
        auto st = runEngine (e2, p, sr, 3.0, [] (int64_t) { return 0.0f; });
        std::printf ("  frozen rms with silent input: %.4f (frozen=%d)\n", st.rms, (int) e2.isFrozen.load());
        CHECK (st.rms > 0.02, "freeze lost the audio");
        std::vector<float> fl, fr;
        CHECK (e2.copyFrozenAudio (fl, fr) && fl.size() > 1000, "frozen audio copy failed");
        GranularEngine e3; e3.prepare (sr, 512);
        e3.restoreFrozenAudio (fl, fr);
        auto st3 = runEngine (e3, p, sr, 1.0, [] (int64_t) { return 0.0f; });
        std::printf ("  restored frozen rms: %.4f\n", st3.rms);
        CHECK (st3.rms > 0.02, "restored freeze silent");
        p.infiniteFreeze = false;
        auto st4 = runEngine (e2, p, sr, 1.0, sine);
        CHECK (st4.finite && st4.peak < 2.0f, "unfreeze unstable");
    }

    //--------------------------------------------------------------------------
    std::printf ("[7] Internal sources + MIDI mode + creative functions\n");
    {
        GranularEngine e4; e4.prepare (sr, 512);
        for (int t = 0; t < 4; ++t)
        {
            EngineParams p; p.source = SourceMode::Internal; p.internalType = t; p.midiMode = true;
            p.spectralMode = (t % 6) + 1; p.spectralAmount = 0.8f; p.chaos = 0.5f; p.evolve = 0.7f; p.drone = t == 2 ? 1.0f : 0.0f; p.reverseCloud = t == 3 ? 1.0f : 0.0f;
            p.blur = 0.5f; p.diffusion = 0.6f;
            e4.noteOn (60 + t, 0.9f);
            auto st = runEngine (e4, p, sr, 1.5, [] (int64_t) { return 0.0f; }, false);
            e4.noteOff (60 + t);
            std::printf ("  internal %-8s rms %.4f peak %.3f\n", kInternalNames[t], st.rms, st.peak);
            CHECK (st.finite && st.rms > 0.01 && st.peak < 3.0f, "internal source %d failed", t);
        }
        // gate closed -> silence eventually
        EngineParams p; p.source = SourceMode::Internal; p.midiMode = true; p.ampEnv = 0.0f; p.sizeMs = 50;
        runEngine (e4, p, sr, 3.0, [] (int64_t) { return 0.0f; }, false);
        auto st = runEngine (e4, p, sr, 0.5, [] (int64_t) { return 0.0f; }, false);
        std::printf ("  gated (no notes) rms %.6f\n", st.rms);
        CHECK (st.rms < 1e-3, "MIDI gate does not stop grains");
    }

    //--------------------------------------------------------------------------
    std::printf ("[8] Sample source hand-off\n");
    {
        GranularEngine e5; e5.prepare (sr, 512);
        auto s = std::make_unique<SampleData>();
        s->sampleRate = 44100.0;
        for (int i = 0; i < 44100 * 2; ++i) { const float v = 0.5f * std::sin (2.0f * kPi * 330.0f * i / 44100.0f); s->L.push_back (v); s->R.push_back (v); }
        s->finalise();
        e5.submitSample (std::move (s));
        EngineParams p; p.source = SourceMode::Sample; p.direction = 0.3f;
        auto st = runEngine (e5, p, sr, 2.0, [] (int64_t) { return 0.0f; }, false);
        std::printf ("  sample rms %.4f\n", st.rms);
        CHECK (st.rms > 0.01, "sample source silent");
        // swap again while grains are playing
        auto s2 = std::make_unique<SampleData>(); s2->L.assign (30000, 0.1f); s2->R.assign (30000, 0.1f); s2->sampleRate = sr; s2->finalise();
        e5.submitSample (std::move (s2));
        auto st2 = runEngine (e5, p, sr, 1.0, [] (int64_t) { return 0.0f; }, false);
        e5.collectGarbage();
        CHECK (st2.finite, "sample swap produced invalid output");
    }

    //--------------------------------------------------------------------------
    std::printf ("[9] Modulation engine\n");
    {
        ModulationEngine m; m.prepare (sr);
        ModParams mp; mp.lfo[0].rateHz = 2.0f; mp.lfo[1].shape = (int) LfoShape::SampleHold; mp.lfo[2].sync = true; mp.lfo[2].division = 9;
        for (int i = 0; i < 16; ++i) mp.steps[(size_t) i] = (float) i / 15.0f;
        mp.slots[0] = { SrcLfo1, 3, 0.5f };
        TransportInfo t; t.playing = true; t.bpm = 120;
        float mn = 1, mx = -1; int crossings = 0; float prev = 0;
        for (int b = 0; b < (int) (sr / kSubBlock); ++b)
        {
            t.ppq += kSubBlock / sr * 2.0;
            m.process (kSubBlock, mp, t, b < 500, b == 0, 0.0f);
            const float v = m.getSource (SrcLfo1);
            mn = std::min (mn, v); mx = std::max (mx, v);
            if (prev < 0 && v >= 0) ++crossings;
            prev = v;
        }
        std::printf ("  LFO1 range [%.2f, %.2f], cycles/s %d, env1 %.3f\n", mn, mx, crossings, m.getEnvLevel (0));
        CHECK (mn < -0.95f && mx > 0.95f, "LFO range");
        CHECK (crossings == 2, "LFO rate");
        float off[8] = {};
        m.applyMatrix (mp, off, 8);
        CHECK (std::abs (off[3]) <= 0.5f, "matrix amount");
    }

    //--------------------------------------------------------------------------
    std::printf ("[10] Effects: every unit, random orders, extreme parameters\n");
    {
        EffectsEngine fx; fx.prepare (sr, 512);
        Random r (77);
        float worst = 0; bool fin = true;
        for (int trial = 0; trial < 40; ++trial)
        {
            FxParams p;
            for (auto& e : p.enabled) e = r.nextFloat() < 0.7f;
            std::array<int, NumFx> o { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
            for (int i = NumFx - 1; i > 0; --i) std::swap (o[(size_t) i], o[(size_t) r.nextInt (i + 1)]);
            p.order = unpackOrder (packOrder (o));
            p.revSize = r.nextFloat(); p.revDecay = 0.2f + r.nextFloat() * 29.0f; p.revMix = r.nextFloat();
            p.dlyFeedback = r.nextFloat() * 0.97f; p.dlyMix = r.nextFloat(); p.dlySync = r.nextFloat() < 0.5f;
            p.distDrive = r.nextFloat(); p.distType = r.nextInt (4);
            p.crushBits = 1 + r.nextFloat() * 15; p.crushDownsample = 1 + r.nextFloat() * 63;
            p.filtCutoff = 20 + r.nextFloat() * 19980; p.filtReso = r.nextFloat(); p.filtMode = r.nextInt (4); p.filtDrive = r.nextFloat();
            p.phFeedback = r.nextFloat() * 0.9f; p.fsShiftHz = r.nextBipolar() * 2000; p.fsFeedback = r.nextFloat() * 0.9f;
            p.imgWidth = r.nextFloat() * 2; p.imgMonoBassHz = r.nextFloat() * 400;
            float L[kSubBlock], R[kSubBlock];
            for (int pos = 0; pos < (int) (sr * 0.5); pos += kSubBlock)
            {
                for (int i = 0; i < kSubBlock; ++i) { L[i] = 0.7f * std::sin (2 * kPi * 150 * (pos + i) / (float) sr); R[i] = r.nextBipolar() * 0.3f; }
                fx.process (L, R, kSubBlock, p);
                for (int i = 0; i < kSubBlock; ++i) { fin &= std::isfinite (L[i]) && std::isfinite (R[i]); worst = std::max ({ worst, std::abs (L[i]), std::abs (R[i]) }); }
            }
        }
        std::printf ("  worst peak %.3f finite %d\n", worst, fin);
        CHECK (fin, "effects produced NaN/inf");
        CHECK (worst < 20.0f, "effects runaway");
    }

    //--------------------------------------------------------------------------
    std::printf ("[11] CPU benchmark (heavy patch, 10 s audio)\n");
    {
        GranularEngine e6; e6.prepare (sr, 512);
        EffectsEngine fx; fx.prepare (sr, 512);
        EngineParams p; p.density = 150; p.sizeMs = 400; p.overlap = 1; p.thickness = 1; p.blur = 0.6f; p.diffusion = 0.5f; p.drone = 0.5f;
        FxParams fp; fp.enabled.fill (true);
        float inL[kSubBlock], oL[kSubBlock], oR[kSubBlock];
        const auto t0 = std::chrono::steady_clock::now();
        const int64_t total = (int64_t) (sr * 10);
        for (int64_t pos = 0; pos < total; pos += kSubBlock)
        {
            for (int i = 0; i < kSubBlock; ++i) inL[i] = sine (pos + i);
            e6.process (inL, inL, oL, oR, kSubBlock, p);
            fx.process (oL, oR, kSubBlock, fp);
        }
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  10 s rendered in %.3f s -> %.1f %% of one core (grains active %d)\n", secs, secs * 10.0, e6.activeGrainCount.load());
        CHECK (secs < 10.0, "slower than realtime");
    }

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", failures);
    return failures == 0 ? 0 : 1;
}
