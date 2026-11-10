// =============================================================================
//  NUBIS - HostTest.cpp : loads the compiled .vst3 like a DAW would and stress
//  tests it: every factory preset, odd buffer sizes, sample-rate changes, MIDI,
//  automation, state save/restore and editor creation.
//  Usage: NubisHostTest <path-to-Nubis.vst3>
// =============================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdio>

static int failures = 0;
#define CHECK(c, ...) do { if (! (c)) { ++failures; std::printf ("  FAIL: " __VA_ARGS__); std::printf ("\n"); } } while (0)

struct Stats { float peak = 0; bool finite = true; double rms = 0; };

static Stats run (juce::AudioPluginInstance& p, int blocks, int blockSize, double sr, bool withMidi, int seed)
{
    Stats s;
    juce::AudioBuffer<float> buf (2, blockSize);
    juce::Random r (seed);
    double acc = 0; int64_t n = 0;
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
        if (withMidi && b % 40 == 0)  midi.addEvent (juce::MidiMessage::noteOn (1, 48 + r.nextInt (24), 0.8f), r.nextInt (blockSize));
        if (withMidi && b % 40 == 30) midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        for (int i = 0; i < blockSize; ++i)
        {
            const float t = (float) ((int64_t) b * blockSize + i) / (float) sr;
            const float v = 0.4f * std::sin (6.2831853f * 220.0f * t) * (0.5f + 0.5f * std::sin (6.2831853f * 0.5f * t)) + 0.05f * r.nextFloat();
            buf.setSample (0, i, v); buf.setSample (1, i, v * 0.9f);
        }
        p.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < blockSize; ++i)
            {
                const float v = buf.getSample (c, i);
                if (! std::isfinite (v)) s.finite = false;
                s.peak = std::max (s.peak, std::abs (v));
                acc += v * v; ++n;
            }
    }
    s.rms = std::sqrt (acc / std::max<int64_t> (1, n));
    return s;
}

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage: NubisHostTest <plugin.vst3>\n"); return 2; }
    juce::ScopedJuceInitialiser_GUI gui;

    juce::VST3PluginFormat fmt;
    juce::OwnedArray<juce::PluginDescription> types;
    fmt.findAllTypesForFile (types, juce::File (juce::String (argv[1])).getFullPathName());
    if (types.isEmpty()) { std::printf ("FAIL: no plugin found in %s\n", argv[1]); return 1; }
    auto& desc = *types[0];
    std::printf ("Loaded: %s by %s  (%s, %d in / %d out, instrument=%d)\n", desc.name.toRawUTF8(), desc.manufacturerName.toRawUTF8(),
                 desc.pluginFormatName.toRawUTF8(), desc.numInputChannels, desc.numOutputChannels, (int) desc.isInstrument);

    juce::String err;
    auto plugin = fmt.createInstanceFromDescription (desc, 48000.0, 512, err);
    if (plugin == nullptr) { std::printf ("FAIL: could not instantiate: %s\n", err.toRawUTF8()); return 1; }

    std::printf ("Parameters: %d   Programs: %d   Latency: %d\n", plugin->getParameters().size(), plugin->getNumPrograms(), plugin->getLatencySamples());
    CHECK (plugin->getParameters().size() > 100, "too few parameters exposed");
    CHECK (plugin->getNumPrograms() >= 30, "expected >= 30 factory presets");

    plugin->setPlayConfigDetails (desc.isInstrument ? 0 : 2, 2, 48000.0, 512);
    plugin->prepareToPlay (48000.0, 512);

    // --- every factory preset --------------------------------------------------
    float worst = 0; double quietest = 1e9; juce::String quietName;
    for (int prog = 0; prog < plugin->getNumPrograms(); ++prog)
    {
        plugin->setCurrentProgram (prog);
        const auto st = run (*plugin, 200, 512, 48000.0, true, prog);
        worst = std::max (worst, st.peak);
        if (st.rms < quietest) { quietest = st.rms; quietName = plugin->getProgramName (prog); }
        CHECK (st.finite, "preset %d (%s) produced NaN/inf", prog, plugin->getProgramName (prog).toRawUTF8());
        CHECK (st.rms > 1.0e-4, "preset %d (%s) is silent", prog, plugin->getProgramName (prog).toRawUTF8());
        std::printf ("  %-22s rms %.4f  peak %.3f\n", plugin->getProgramName (prog).toRawUTF8(), st.rms, st.peak);
    }
    std::printf ("Presets: worst peak %.3f (limiter ceiling 0.966), quietest '%s' rms %.4f\n", worst, quietName.toRawUTF8(), quietest);
    CHECK (worst <= 0.97f, "output exceeded limiter ceiling");

    // --- odd block sizes ----------------------------------------------------------
    for (int bs : { 1, 7, 31, 33, 100, 1024, 4096 })
    {
        plugin->releaseResources();
        plugin->prepareToPlay (48000.0, bs);
        const auto st = run (*plugin, std::max (20, 48000 / bs / 2), bs, 48000.0, false, bs);
        CHECK (st.finite, "block size %d invalid output", bs);
        std::printf ("  block %4d -> rms %.4f\n", bs, st.rms);
    }

    // --- sample-rate changes -------------------------------------------------------
    for (double sr : { 44100.0, 88200.0, 96000.0, 192000.0, 48000.0 })
    {
        plugin->releaseResources();
        plugin->setPlayConfigDetails (desc.isInstrument ? 0 : 2, 2, sr, 512);
        plugin->prepareToPlay (sr, 512);
        const auto st = run (*plugin, 100, 512, sr, false, (int) sr);
        CHECK (st.finite && st.peak < 1.0f, "sample rate %.0f failed", sr);
        std::printf ("  %6.0f Hz -> rms %.4f peak %.3f\n", sr, st.rms, st.peak);
    }

    // --- random automation of every parameter ---------------------------------------
    {
        juce::Random r (99);
        auto& params = plugin->getParameters();
        bool fin = true; float pk = 0;
        for (int round = 0; round < 30; ++round)
        {
            for (auto* p : params)
            {
                // host bypass and the safety limiter are left alone: this test checks the DSP, not a disabled limiter
                const auto n = p->getName (64);
                if (n == "Bypass" || n == "Limiter") continue;
                if (r.nextFloat() < 0.3f) p->setValueNotifyingHost (r.nextFloat());
            }
            const auto st = run (*plugin, 20, 512, 48000.0, true, round);
            fin &= st.finite; pk = std::max (pk, st.peak);
        }
        std::printf ("Random automation: finite %d, peak %.3f\n", (int) fin, pk);
        for (auto* p : params) p->setValueNotifyingHost (p->getDefaultValue());   // clean slate for the next tests
        run (*plugin, 4, 512, 48000.0, false, 0);
        CHECK (fin, "random automation produced NaN/inf");
        CHECK (pk <= 0.97f, "random automation exceeded ceiling");
    }

    // --- state save / restore --------------------------------------------------------
    {
        plugin->setCurrentProgram (5);
        juce::MemoryBlock state;
        plugin->getStateInformation (state);
        juce::StringArray before;
        for (auto* p : plugin->getParameters()) before.add (p->getCurrentValueAsText());
        plugin->setCurrentProgram (20);
        plugin->setStateInformation (state.getData(), (int) state.getSize());
        // compare displayed values (host-side normalised values of choice params may be unquantised)
        int mismatches = 0, i = 0;
        for (auto* p : plugin->getParameters())
        {
            const auto now = p->getCurrentValueAsText();
            if (p->getName (64) != "Bypass" && now != before[i]) { ++mismatches; std::printf ("    mismatch: %s '%s' vs '%s'\n", p->getName (64).toRawUTF8(), now.toRawUTF8(), before[i].toRawUTF8()); }
            ++i;
        }
        std::printf ("State: %d bytes, %d mismatching params after restore\n", (int) state.getSize(), mismatches);
        CHECK (mismatches == 0, "state restore mismatch");
    }

    // --- infinite freeze persistence -------------------------------------------------
    if (! desc.isInstrument)
    {
        plugin->setCurrentProgram (0);
        auto& params = plugin->getParameters();
        juce::AudioProcessorParameter* inf = nullptr;
        for (auto* p : params)
        {
            if (p->getName (64) == "Infinite Freeze") inf = p;
            if (p->getName (64) == "Source" || p->getName (64) == "Play Mode") p->setValueNotifyingHost (0.0f);   // live input, continuous
        }
        CHECK (inf != nullptr, "Infinite Freeze parameter not found");
        if (inf != nullptr)
        {
            run (*plugin, 300, 512, 48000.0, false, 1);
            inf->setValueNotifyingHost (1.0f);
            run (*plugin, 10, 512, 48000.0, false, 2);
            juce::MemoryBlock state;
            plugin->getStateInformation (state);
            std::printf ("State with frozen audio: %d bytes\n", (int) state.getSize());
            CHECK (state.getSize() > 200000, "frozen audio not stored in state");

            auto second = fmt.createInstanceFromDescription (desc, 48000.0, 512, err);
            second->setPlayConfigDetails (2, 2, 48000.0, 512);
            second->prepareToPlay (48000.0, 512);
            second->setStateInformation (state.getData(), (int) state.getSize());
            for (auto* p : second->getParameters()) if (juce::StringArray { "Infinite Freeze", "Dry/Wet", "Source", "Input Gain", "Output", "Grain Freeze", "Density", "Grain Size", "Play Mode", "Limiter", "Position" }.contains (p->getName (64))) std::printf ("    second: %s = %s\n", p->getName (64).toRawUTF8(), p->getCurrentValueAsText().toRawUTF8());
            // silent input: sound must come only from the restored frozen buffer
            juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi; double acc = 0;
            for (int b = 0; b < 200; ++b) { buf.clear(); second->processBlock (buf, midi); acc += buf.getRMSLevel (0, 0, 512); }
            std::printf ("Restored frozen instance, silent input -> mean rms %.4f\n", acc / 200.0);
            CHECK (acc / 200.0 > 0.005, "restored frozen audio is silent");
            second->releaseResources();
        }
    }

    // --- editor ---------------------------------------------------------------------
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (plugin->createEditorIfNeeded());
        CHECK (ed != nullptr, "no editor");
        if (ed != nullptr) std::printf ("Editor: %d x %d\n", ed->getWidth(), ed->getHeight());
    }

    plugin->releaseResources();
    plugin.reset();
    std::printf ("\n%s (%d failures)\n", failures == 0 ? "HOST TEST PASSED" : "HOST TEST FAILED", failures);
    return failures == 0 ? 0 : 1;
}
