// =============================================================================
//  NUBIS - PluginProcessor.h
// =============================================================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "Parameters.h"
#include "PresetManager.h"
#include "dsp/GranularEngine.h"
#include "dsp/ModulationEngine.h"
#include "dsp/EffectsEngine.h"

#ifndef NUBIS_INSTRUMENT
 #define NUBIS_INSTRUMENT 0
#endif

class NubisAudioProcessor : public juce::AudioProcessor,
                            private juce::Timer
{
public:
    NubisAudioProcessor();
    ~NubisAudioProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;
    nubis::PresetManager presets;
    nubis::GranularEngine engine;

    // ---- message-thread API -------------------------------------------------
    bool loadSampleFile (const juce::File& f);
    void clearSample();
    juce::String getSampleName() const { return sampleFile.getFileNameWithoutExtension(); }
    const std::array<float, nubis::kOverviewBins>& getSampleOverview() const { return sampleOverview; }
    bool hasSample() const { return sampleLoaded; }

    std::array<int, nubis::NumFx> getFxOrder() const { return nubis::unpackOrder (fxOrder.load()); }
    void setFxOrder (const std::array<int, nubis::NumFx>& order);
    void moveFx (int fxId, int delta);

    void randomize (bool experimental);
    /** Adds a modulation routing; returns slot index or -1 if the matrix is full. */
    int addModulation (int source, const juce::String& destParamId, float amount);
    void clearModulationsFor (const juce::String& destParamId);
    void clearSlot (int slot);

    // ---- visualisation (read from UI thread) -----------------------------------
    std::atomic<float> meterL { 0.0f }, meterR { 0.0f }, meterIn { 0.0f }, gainReductionDb { 0.0f };
    std::array<std::atomic<float>, 48> modulatedNorm;     // per destination, current modulated value 0..1
    std::array<std::atomic<bool>, 48> destHasMod;
    std::array<std::atomic<float>, nubis::NumModSources> sourceValues;
    std::atomic<int> currentSeqStep { 0 };
    juce::AbstractFifo analysisFifo { 8192 };
    std::vector<float> analysisData = std::vector<float> (8192, 0.0f);
    std::atomic<double> hostBpm { 120.0 };

    static constexpr bool isInstrument() { return NUBIS_INSTRUMENT != 0; }

private:
    static BusesProperties makeBuses();
    void timerCallback() override;
    void cacheParameters();
    void buildBaseParams();
    void restoreFrozen (std::vector<float> l, std::vector<float> r, double rate);
    float modulated (int destIndex, float baseValue, const float* offsets) noexcept;

    // parameters (raw atomic pointers, read lock-free on the audio thread)
    struct P { std::atomic<float>* v = nullptr; juce::RangedAudioParameter* p = nullptr; float get() const noexcept { return v->load (std::memory_order_relaxed); } };
    P get (const juce::String& id);
    std::vector<P> destParams;           // per kModDestinations
    struct Cache
    {
        P source, internalType, playMode, inputGain, dryWet, outputGain, limiter;
        P size, density, position, posRandom, pitch, pitchRandom, jitter, overlap, spread, pan, reverse, direction, envelope, freeze;
        P texture, smoothness, blur, randomness, diffusion, thickness, width, feedback;
        P infFreeze, spectralMode, spectralAmount, chaos, evolve, evolveRate, reverseCloud, drone;
        std::array<P, nubis::kNumMacros> macro;
        std::array<P, nubis::kNumLfos> lfoRate, lfoSync, lfoDiv, lfoShape;
        std::array<P, nubis::kNumEnvs> envA, envD, envS, envR, envTrig;
        P rndRate, rndSmooth, seqLen, seqDiv, seqGlide;
        std::array<P, nubis::kNumSteps> steps;
        std::array<P, nubis::kNumModSlots> modSrc, modDst, modAmt;
        std::array<P, nubis::NumFx> fxOn;
        P revSize, revDecay, revDamp, revPredelay, revMix;
        P dlyTime, dlySync, dlyDiv, dlyFb, dlyPP, dlyTone, dlyMix;
        P distDrive, distType, distTone, distMix, crushBits, crushDown, crushMix;
        P filtCut, filtRes, filtMode, filtDrive, chRate, chDepth, chMix;
        P phRate, phDepth, phFb, phMix, fsShift, fsMix, fsFb, imgWidth, imgBass;
    } c;

    nubis::ModulationEngine modEngine;
    nubis::EffectsEngine fxEngine;
    std::atomic<uint64_t> fxOrder { 0x876543210ull };

    juce::AudioBuffer<float> wetBuffer, dryBuffer;
    juce::SmoothedValue<float> outGainSm, mixSm, inGainSm;
    float ampEnvPrev = 0.0f, limiterGain = 1.0f, limiterRelease = 0.999f;
    double sampleRateHz = 44100.0;
    double ppqPosition = 0.0;
    bool prepared = false;

    // sample
    juce::AudioFormatManager formatManager;
    juce::File sampleFile;
    std::array<float, nubis::kOverviewBins> sampleOverview {};
    bool sampleLoaded = false;

    // frozen audio carried across prepare / state restore
    std::vector<float> pendingFrozenL, pendingFrozenR;
    double pendingFrozenRate = 0.0;

    nubis::ModParams modParams;
    nubis::EngineParams engineBase;
    nubis::FxParams fxBase;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NubisAudioProcessor)
};
