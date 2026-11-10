// =============================================================================
//  NUBIS - Parameters.cpp
// =============================================================================
#include "Parameters.h"
#include "dsp/GranularEngine.h"

namespace nubis::pid
{
juce::String macro (int i)                  { return "macro" + juce::String (i + 1); }
juce::String lfo (int i, const char* field) { return "lfo" + juce::String (i + 1) + "_" + field; }
juce::String env (int i, const char* field) { return "env" + juce::String (i + 1) + "_" + field; }
juce::String step (int i)                   { return "seq_step" + juce::String (i + 1); }
juce::String modSrc (int s)                 { return "mod" + juce::String (s + 1) + "_src"; }
juce::String modDst (int s)                 { return "mod" + juce::String (s + 1) + "_dst"; }
juce::String modAmt (int s)                 { return "mod" + juce::String (s + 1) + "_amt"; }
juce::String fxOn (int fx)                  { return juce::String ("fx_") + kFxShort[fx] + "_on"; }
} // namespace nubis::pid

namespace nubis
{
using namespace pid;

const std::vector<ModDestination> kModDestinations = {
    { size, "Grain Size" },      { density, "Density" },        { position, "Position" },      { posRandom, "Pos Random" },
    { pitch, "Pitch" },          { pitchRandom, "Pitch Random" },{ jitter, "Jitter" },          { overlap, "Overlap" },
    { spread, "Spread" },        { pan, "Pan" },                { reverse, "Reverse" },        { direction, "Direction" },
    { texture, "Texture" },      { smoothness, "Smoothness" },  { blur, "Spectral Blur" },     { randomness, "Randomness" },
    { diffusion, "Diffusion" },  { thickness, "Tex Density" },  { width, "Stereo Width" },     { feedback, "Feedback" },
    { spectralAmount, "Spectral Amt" }, { chaos, "Chaos" },     { evolve, "Evolve" },          { reverseCloud, "Reverse Cloud" },
    { drone, "Drone" },          { filtCut, "Filter Cutoff" },  { filtRes, "Filter Reso" },    { revMix, "Reverb Mix" },
    { revSize, "Reverb Size" },  { dlyMix, "Delay Mix" },       { dlyFb, "Delay Feedback" },   { distDrive, "Drive" },
    { crushBits, "Crush Bits" }, { crushDown, "Crush Rate" },   { chMix, "Chorus Mix" },       { phMix, "Phaser Mix" },
    { fsShift, "Freq Shift" },   { imgWidth, "Imager Width" },  { dryWet, "Dry/Wet" },
};

int findModDestination (const juce::String& id)
{
    for (size_t i = 0; i < kModDestinations.size(); ++i)
        if (id == kModDestinations[i].paramId) return (int) i;
    return -1;
}

bool isGlobalContextParam (const juce::String& id)
{
    return id == source || id == playMode || id == inputGain || id == limiter;
}

//==============================================================================
namespace
{
    using APF = juce::AudioParameterFloat;
    using APC = juce::AudioParameterChoice;
    using APB = juce::AudioParameterBool;
    using API = juce::AudioParameterInt;
    using Attr = juce::AudioParameterFloatAttributes;

    juce::NormalisableRange<float> skewed (float lo, float hi, float centre)
    {
        juce::NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    Attr pct()
    {
        return Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })
                     .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue() / 100.0f; });
    }
    Attr bip()
    {
        return Attr().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + juce::String (juce::roundToInt (v * 100.0f)) + " %"; })
                     .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue() / 100.0f; });
    }
    Attr unit (const juce::String& u, int decimals)
    {
        return Attr().withStringFromValueFunction ([u, decimals] (float v, int) { return juce::String (v, decimals) + " " + u; })
                     .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); });
    }
    Attr ms()
    {
        return Attr().withStringFromValueFunction ([] (float v, int) { return v < 10.0f ? juce::String (v, 1) + " ms" : v < 1000.0f ? juce::String (juce::roundToInt (v)) + " ms" : juce::String (v / 1000.0f, 2) + " s"; })
                     .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); });
    }
    Attr hz()
    {
        return Attr().withStringFromValueFunction ([] (float v, int) { return std::abs (v) < 10.0f ? juce::String (v, 2) + " Hz" : std::abs (v) < 1000.0f ? juce::String (v, 0) + " Hz" : juce::String (v / 1000.0f, 2) + " kHz"; })
                     .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); });
    }
    Attr st()
    {
        return Attr().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + juce::String (v, 2) + " st"; })
                     .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); });
    }
    Attr db()
    {
        return Attr().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; })
                     .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); });
    }

    juce::StringArray toArray (const char* const* names, int n)
    {
        juce::StringArray a;
        for (int i = 0; i < n; ++i) a.add (names[i]);
        return a;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (bool instrumentBuild)
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto add = [&p] (auto* param) { p.emplace_back (param); };
    auto fl = [&add] (const juce::String& id, const juce::String& name, juce::NormalisableRange<float> r, float def, Attr a = {})
    { add (new APF (juce::ParameterID { id, 1 }, name, r, def, a)); };
    auto lin = [] (float lo, float hi) { return juce::NormalisableRange<float> (lo, hi); };

    // ---- global ------------------------------------------------------------
    add (new APC ({ source, 1 }, "Source", toArray (kSourceNames, 3), instrumentBuild ? 2 : 0));
    add (new APC ({ internalType, 1 }, "Internal Source", toArray (kInternalNames, 4), 0));
    add (new APC ({ playMode, 1 }, "Play Mode", juce::StringArray { "Continuous", "MIDI" }, instrumentBuild ? 1 : 0));
    fl (inputGain, "Input Gain", lin (-24.0f, 24.0f), 0.0f, db());
    fl (dryWet, "Dry/Wet", lin (0.0f, 1.0f), instrumentBuild ? 1.0f : 0.6f, pct());
    fl (outputGain, "Output", lin (-36.0f, 12.0f), 0.0f, db());
    add (new APB ({ limiter, 1 }, "Limiter", true));

    // ---- grain engine ------------------------------------------------------
    fl (size, "Grain Size", skewed (1.0f, 2000.0f, 150.0f), 120.0f, ms());
    fl (density, "Density", skewed (0.5f, 400.0f, 25.0f), 18.0f, unit ("gr/s", 1));
    fl (position, "Position", lin (0.0f, 1.0f), 0.75f, pct());
    fl (posRandom, "Position Random", lin (0.0f, 1.0f), 0.12f, pct());
    fl (pitch, "Pitch", lin (-24.0f, 24.0f), 0.0f, st());
    fl (pitchRandom, "Pitch Random", skewed (0.0f, 24.0f, 3.0f), 0.0f, st());
    fl (jitter, "Grain Jitter", lin (0.0f, 1.0f), 0.3f, pct());
    fl (overlap, "Grain Overlap", lin (0.0f, 1.0f), 0.5f, pct());
    fl (spread, "Grain Spread", lin (0.0f, 1.0f), 0.4f, pct());
    fl (pan, "Grain Pan", lin (-1.0f, 1.0f), 0.0f, bip());
    fl (reverse, "Grain Reverse", lin (0.0f, 1.0f), 0.0f, pct());
    fl (direction, "Grain Direction", lin (-1.0f, 1.0f), 0.0f, bip());
    add (new APC ({ envelope, 1 }, "Grain Envelope", toArray (kEnvelopeNames, (int) GrainEnvelope::NumShapes), 0));
    add (new APB ({ freeze, 1 }, "Grain Freeze", false));

    // ---- texture -----------------------------------------------------------
    fl (texture, "Texture", lin (0.0f, 1.0f), 0.5f, pct());
    fl (smoothness, "Smoothness", lin (0.0f, 1.0f), 0.0f, pct());
    fl (blur, "Spectral Blur", lin (0.0f, 1.0f), 0.0f, pct());
    fl (randomness, "Randomness", lin (0.0f, 1.0f), 0.0f, pct());
    fl (diffusion, "Diffusion", lin (0.0f, 1.0f), 0.0f, pct());
    fl (thickness, "Texture Density", lin (0.0f, 1.0f), 0.0f, pct());
    fl (width, "Stereo Width", lin (0.0f, 2.0f), 1.0f, pct());
    fl (feedback, "Feedback", lin (0.0f, 0.95f), 0.0f, pct());

    // ---- special -----------------------------------------------------------
    add (new APB ({ infFreeze, 1 }, "Infinite Freeze", false));
    add (new APC ({ spectralMode, 1 }, "Spectral Play", toArray (kSpectralNames, 7), 0));
    fl (spectralAmount, "Spectral Amount", lin (0.0f, 1.0f), 0.7f, pct());
    fl (chaos, "Chaos", lin (0.0f, 1.0f), 0.0f, pct());
    fl (evolve, "Evolve", lin (0.0f, 1.0f), 0.0f, pct());
    fl (evolveRate, "Evolve Rate", skewed (0.01f, 2.0f, 0.15f), 0.1f, hz());
    fl (reverseCloud, "Reverse Cloud", lin (0.0f, 1.0f), 0.0f, pct());
    fl (drone, "Drone", lin (0.0f, 1.0f), 0.0f, pct());

    // ---- macros ------------------------------------------------------------
    for (int i = 0; i < kNumMacros; ++i) fl (macro (i), "Macro " + juce::String (i + 1), lin (0.0f, 1.0f), 0.0f, pct());

    // ---- LFOs --------------------------------------------------------------
    const auto divs = toArray (kDivisionNames, kNumDivisions);
    for (int i = 0; i < kNumLfos; ++i)
    {
        const auto n = "LFO " + juce::String (i + 1) + " ";
        fl (lfo (i, "rate"), n + "Rate", skewed (0.01f, 20.0f, 1.0f), 0.5f + (float) i * 0.7f, hz());
        add (new APB ({ lfo (i, "sync"), 1 }, n + "Sync", false));
        add (new APC ({ lfo (i, "div"), 1 }, n + "Division", divs, 9));
        add (new APC ({ lfo (i, "shape"), 1 }, n + "Shape", toArray (kLfoShapeNames, 6), i % 6));
    }
    // ---- envelopes ---------------------------------------------------------
    for (int i = 0; i < kNumEnvs; ++i)
    {
        const auto n = "ENV " + juce::String (i + 1) + " ";
        fl (env (i, "a"), n + "Attack", skewed (0.001f, 10.0f, 0.5f), 0.02f, unit ("s", 3));
        fl (env (i, "d"), n + "Decay", skewed (0.005f, 10.0f, 0.8f), 0.4f, unit ("s", 3));
        fl (env (i, "s"), n + "Sustain", lin (0.0f, 1.0f), 0.8f, pct());
        fl (env (i, "r"), n + "Release", skewed (0.005f, 15.0f, 1.0f), 1.2f, unit ("s", 3));
        add (new APC ({ env (i, "trig"), 1 }, n + "Trigger", toArray (kEnvTriggerNames, 3), instrumentBuild ? 0 : (i == 0 ? 0 : 2)));
    }
    // ---- random / seq ------------------------------------------------------
    fl (rndRate, "Random Rate", skewed (0.05f, 20.0f, 1.5f), 1.0f, hz());
    fl (rndSmooth, "Random Smooth", lin (0.0f, 1.0f), 0.5f, pct());
    for (int i = 0; i < kNumSteps; ++i)
        fl (step (i), "Seq Step " + juce::String (i + 1), lin (0.0f, 1.0f), (i % 4 == 0) ? 1.0f : (i % 2 == 0 ? 0.5f : 0.2f), pct());
    add (new API ({ seqLen, 1 }, "Seq Length", 1, kNumSteps, 16));
    add (new APC ({ seqDiv, 1 }, "Seq Division", divs, 3));
    fl (seqGlide, "Seq Glide", lin (0.0f, 1.0f), 0.0f, pct());

    // ---- modulation matrix -------------------------------------------------
    juce::StringArray dests { "None" };
    for (auto& d : kModDestinations) dests.add (d.label);
    for (int s = 0; s < kNumModSlots; ++s)
    {
        const auto n = "Mod " + juce::String (s + 1) + " ";
        add (new APC ({ modSrc (s), 1 }, n + "Source", toArray (kModSourceNames, NumModSources), 0));
        add (new APC ({ modDst (s), 1 }, n + "Destination", dests, 0));
        fl (modAmt (s), n + "Amount", lin (-1.0f, 1.0f), 0.0f, bip());
    }

    // ---- effects -----------------------------------------------------------
    for (int f = 0; f < NumFx; ++f)
        add (new APB ({ fxOn (f), 1 }, juce::String (kFxNames[f]) + " On", f == FxReverb));

    fl (revSize, "Reverb Size", lin (0.0f, 1.0f), 0.65f, pct());
    fl (revDecay, "Reverb Decay", skewed (0.2f, 20.0f, 3.0f), 4.0f, unit ("s", 2));
    fl (revDamp, "Reverb Damping", lin (0.0f, 1.0f), 0.4f, pct());
    fl (revPredelay, "Reverb Predelay", lin (0.0f, 200.0f), 15.0f, ms());
    fl (revMix, "Reverb Mix", lin (0.0f, 1.0f), 0.3f, pct());

    fl (dlyTime, "Delay Time", skewed (1.0f, 2000.0f, 350.0f), 350.0f, ms());
    add (new APB ({ dlySync, 1 }, "Delay Sync", true));
    add (new APC ({ dlyDiv, 1 }, "Delay Division", divs, 7));
    fl (dlyFb, "Delay Feedback", lin (0.0f, 0.97f), 0.4f, pct());
    fl (dlyPP, "Delay Ping-Pong", lin (0.0f, 1.0f), 0.6f, pct());
    fl (dlyTone, "Delay Tone", lin (0.0f, 1.0f), 0.6f, pct());
    fl (dlyMix, "Delay Mix", lin (0.0f, 1.0f), 0.3f, pct());

    fl (distDrive, "Drive", lin (0.0f, 1.0f), 0.3f, pct());
    add (new APC ({ distType, 1 }, "Distortion Type", toArray (kDistTypeNames, 4), 0));
    fl (distTone, "Distortion Tone", lin (0.0f, 1.0f), 0.7f, pct());
    fl (distMix, "Distortion Mix", lin (0.0f, 1.0f), 1.0f, pct());

    fl (crushBits, "Crush Bits", lin (1.0f, 16.0f), 8.0f, unit ("bit", 1));
    fl (crushDown, "Crush Rate", skewed (1.0f, 64.0f, 8.0f), 4.0f, unit ("x", 1));
    fl (crushMix, "Crush Mix", lin (0.0f, 1.0f), 1.0f, pct());

    fl (filtCut, "Filter Cutoff", skewed (20.0f, 20000.0f, 1000.0f), 6000.0f, hz());
    fl (filtRes, "Filter Resonance", lin (0.0f, 1.0f), 0.2f, pct());
    add (new APC ({ filtMode, 1 }, "Filter Mode", toArray (kFilterModeNames, 4), 0));
    fl (filtDrive, "Filter Drive", lin (0.0f, 1.0f), 0.0f, pct());

    fl (chRate, "Chorus Rate", skewed (0.02f, 8.0f, 0.8f), 0.6f, hz());
    fl (chDepth, "Chorus Depth", lin (0.0f, 1.0f), 0.5f, pct());
    fl (chMix, "Chorus Mix", lin (0.0f, 1.0f), 0.5f, pct());

    fl (phRate, "Phaser Rate", skewed (0.01f, 10.0f, 0.5f), 0.3f, hz());
    fl (phDepth, "Phaser Depth", lin (0.0f, 1.0f), 0.7f, pct());
    fl (phFb, "Phaser Feedback", lin (0.0f, 0.9f), 0.4f, pct());
    fl (phMix, "Phaser Mix", lin (0.0f, 1.0f), 0.5f, pct());

    {
        juce::NormalisableRange<float> shiftRange (-2000.0f, 2000.0f,
            [] (float lo, float hi, float n) { const float x = n * 2.0f - 1.0f; return juce::jmap (x * x * x, -1.0f, 1.0f, lo, hi); },
            [] (float lo, float hi, float v) { const float x = juce::jmap (v, lo, hi, -1.0f, 1.0f); return (std::cbrt (x) + 1.0f) * 0.5f; });
        fl (fsShift, "Freq Shift", shiftRange, 0.0f, hz());
    }
    fl (fsMix, "Freq Shift Mix", lin (0.0f, 1.0f), 0.5f, pct());
    fl (fsFb, "Freq Shift Feedback", lin (0.0f, 0.9f), 0.0f, pct());

    fl (imgWidth, "Imager Width", lin (0.0f, 2.0f), 1.2f, pct());
    fl (imgBass, "Imager Mono Bass", lin (0.0f, 400.0f), 0.0f, hz());

    return { p.begin(), p.end() };
}

} // namespace nubis
