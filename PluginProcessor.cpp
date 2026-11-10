// =============================================================================
//  NUBIS - PluginProcessor.cpp
// =============================================================================
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace nubis;

//==============================================================================
juce::AudioProcessor::BusesProperties NubisAudioProcessor::makeBuses()
{
   #if NUBIS_INSTRUMENT
    return juce::AudioProcessor::BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true);
   #else
    return juce::AudioProcessor::BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true);
   #endif
}

NubisAudioProcessor::NubisAudioProcessor()
    : AudioProcessor (makeBuses()),
      apvts (*this, nullptr, "NUBIS", createParameterLayout (NUBIS_INSTRUMENT != 0)),
      presets (apvts,
               [this] (const std::array<int, NumFx>& o) { setFxOrder (o); },
               [this] { return getFxOrder(); })
{
    formatManager.registerBasicFormats();
    for (auto& m : modulatedNorm) m.store (0.0f);
    for (auto& h : destHasMod) h.store (false);
    for (auto& s : sourceValues) s.store (0.0f);
    cacheParameters();
    startTimerHz (5);
}

NubisAudioProcessor::~NubisAudioProcessor()
{
    stopTimer();
}

NubisAudioProcessor::P NubisAudioProcessor::get (const juce::String& id)
{
    P p;
    p.v = apvts.getRawParameterValue (id);
    p.p = apvts.getParameter (id);
    jassert (p.v != nullptr && p.p != nullptr);
    return p;
}

void NubisAudioProcessor::cacheParameters()
{
    c.source = get (pid::source); c.internalType = get (pid::internalType); c.playMode = get (pid::playMode);
    c.inputGain = get (pid::inputGain); c.dryWet = get (pid::dryWet); c.outputGain = get (pid::outputGain); c.limiter = get (pid::limiter);
    c.size = get (pid::size); c.density = get (pid::density); c.position = get (pid::position); c.posRandom = get (pid::posRandom);
    c.pitch = get (pid::pitch); c.pitchRandom = get (pid::pitchRandom); c.jitter = get (pid::jitter); c.overlap = get (pid::overlap);
    c.spread = get (pid::spread); c.pan = get (pid::pan); c.reverse = get (pid::reverse); c.direction = get (pid::direction);
    c.envelope = get (pid::envelope); c.freeze = get (pid::freeze);
    c.texture = get (pid::texture); c.smoothness = get (pid::smoothness); c.blur = get (pid::blur); c.randomness = get (pid::randomness);
    c.diffusion = get (pid::diffusion); c.thickness = get (pid::thickness); c.width = get (pid::width); c.feedback = get (pid::feedback);
    c.infFreeze = get (pid::infFreeze); c.spectralMode = get (pid::spectralMode); c.spectralAmount = get (pid::spectralAmount);
    c.chaos = get (pid::chaos); c.evolve = get (pid::evolve); c.evolveRate = get (pid::evolveRate);
    c.reverseCloud = get (pid::reverseCloud); c.drone = get (pid::drone);
    for (int i = 0; i < kNumMacros; ++i) c.macro[(size_t) i] = get (pid::macro (i));
    for (int i = 0; i < kNumLfos; ++i)
    {
        c.lfoRate[(size_t) i] = get (pid::lfo (i, "rate")); c.lfoSync[(size_t) i] = get (pid::lfo (i, "sync"));
        c.lfoDiv[(size_t) i] = get (pid::lfo (i, "div")); c.lfoShape[(size_t) i] = get (pid::lfo (i, "shape"));
    }
    for (int i = 0; i < kNumEnvs; ++i)
    {
        c.envA[(size_t) i] = get (pid::env (i, "a")); c.envD[(size_t) i] = get (pid::env (i, "d"));
        c.envS[(size_t) i] = get (pid::env (i, "s")); c.envR[(size_t) i] = get (pid::env (i, "r"));
        c.envTrig[(size_t) i] = get (pid::env (i, "trig"));
    }
    c.rndRate = get (pid::rndRate); c.rndSmooth = get (pid::rndSmooth);
    c.seqLen = get (pid::seqLen); c.seqDiv = get (pid::seqDiv); c.seqGlide = get (pid::seqGlide);
    for (int i = 0; i < kNumSteps; ++i) c.steps[(size_t) i] = get (pid::step (i));
    for (int s = 0; s < kNumModSlots; ++s)
    {
        c.modSrc[(size_t) s] = get (pid::modSrc (s)); c.modDst[(size_t) s] = get (pid::modDst (s)); c.modAmt[(size_t) s] = get (pid::modAmt (s));
    }
    for (int f = 0; f < NumFx; ++f) c.fxOn[(size_t) f] = get (pid::fxOn (f));
    c.revSize = get (pid::revSize); c.revDecay = get (pid::revDecay); c.revDamp = get (pid::revDamp); c.revPredelay = get (pid::revPredelay); c.revMix = get (pid::revMix);
    c.dlyTime = get (pid::dlyTime); c.dlySync = get (pid::dlySync); c.dlyDiv = get (pid::dlyDiv); c.dlyFb = get (pid::dlyFb);
    c.dlyPP = get (pid::dlyPP); c.dlyTone = get (pid::dlyTone); c.dlyMix = get (pid::dlyMix);
    c.distDrive = get (pid::distDrive); c.distType = get (pid::distType); c.distTone = get (pid::distTone); c.distMix = get (pid::distMix);
    c.crushBits = get (pid::crushBits); c.crushDown = get (pid::crushDown); c.crushMix = get (pid::crushMix);
    c.filtCut = get (pid::filtCut); c.filtRes = get (pid::filtRes); c.filtMode = get (pid::filtMode); c.filtDrive = get (pid::filtDrive);
    c.chRate = get (pid::chRate); c.chDepth = get (pid::chDepth); c.chMix = get (pid::chMix);
    c.phRate = get (pid::phRate); c.phDepth = get (pid::phDepth); c.phFb = get (pid::phFb); c.phMix = get (pid::phMix);
    c.fsShift = get (pid::fsShift); c.fsMix = get (pid::fsMix); c.fsFb = get (pid::fsFb);
    c.imgWidth = get (pid::imgWidth); c.imgBass = get (pid::imgBass);

    destParams.clear();
    for (auto& d : kModDestinations) destParams.push_back (get (d.paramId));
    jassert (destParams.size() <= modulatedNorm.size());
}

//==============================================================================
void NubisAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // keep a frozen buffer alive across sample-rate / buffer-size changes
    if (prepared && pendingFrozenL.empty())
    {
        std::vector<float> l, r;
        if (engine.copyFrozenAudio (l, r)) { pendingFrozenL = std::move (l); pendingFrozenR = std::move (r); pendingFrozenRate = sampleRateHz; }
    }

    sampleRateHz = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock);
    modEngine.prepare (sampleRate);
    fxEngine.prepare (sampleRate, samplesPerBlock);

    wetBuffer.setSize (2, kSubBlock);
    dryBuffer.setSize (2, kSubBlock);
    outGainSm.reset (sampleRate, 0.05);
    mixSm.reset (sampleRate, 0.05);
    inGainSm.reset (sampleRate, 0.05);
    outGainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (c.outputGain.get()));
    mixSm.setCurrentAndTargetValue (c.dryWet.get());
    inGainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (c.inputGain.get()));
    limiterGain = 1.0f;
    limiterRelease = (float) std::exp (-1.0 / (0.15 * sampleRate));
    ampEnvPrev = 0.0f;
    prepared = true;

    if (! pendingFrozenL.empty())
    {
        restoreFrozen (pendingFrozenL, pendingFrozenR, pendingFrozenRate);
        pendingFrozenL.clear(); pendingFrozenR.clear();
    }
}

void NubisAudioProcessor::restoreFrozen (std::vector<float> l, std::vector<float> r, double rate)
{
    if (l.empty() || r.size() != l.size()) return;
    if (rate > 0.0 && std::abs (rate - sampleRateHz) > 1.0)
    {
        // linear resample to the current rate
        const double ratio = rate / sampleRateHz;
        const size_t outLen = (size_t) ((double) l.size() / ratio);
        std::vector<float> nl (outLen), nr (outLen);
        for (size_t i = 0; i < outLen; ++i)
        {
            const double pos = (double) i * ratio;
            const size_t i0 = std::min ((size_t) pos, l.size() - 1), i1 = std::min (i0 + 1, l.size() - 1);
            const float f = (float) (pos - (double) i0);
            nl[i] = l[i0] + (l[i1] - l[i0]) * f;
            nr[i] = r[i0] + (r[i1] - r[i0]) * f;
        }
        l = std::move (nl); r = std::move (nr);
    }
    engine.restoreFrozenAudio (l, r);
}

void NubisAudioProcessor::releaseResources() {}

bool NubisAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
   #if ! NUBIS_INSTRUMENT
    const auto in = layouts.getMainInputChannelSet();
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo()) return false;
   #endif
    return true;
}

//==============================================================================
void NubisAudioProcessor::buildBaseParams()
{
    auto& e = engineBase;
    e.source       = (SourceMode) juce::jlimit (0, 2, (int) c.source.get());
    e.internalType = (int) c.internalType.get();
    e.midiMode     = (int) c.playMode.get() == 1;
    e.envelope     = (int) c.envelope.get();
    e.freeze       = c.freeze.get() > 0.5f;
    e.infiniteFreeze = c.infFreeze.get() > 0.5f;
    e.spectralMode = (int) c.spectralMode.get();
    e.evolveRate   = c.evolveRate.get();

    auto& m = modParams;
    for (size_t i = 0; i < (size_t) kNumLfos; ++i)
    {
        m.lfo[i].rateHz = c.lfoRate[i].get(); m.lfo[i].sync = c.lfoSync[i].get() > 0.5f;
        m.lfo[i].division = (int) c.lfoDiv[i].get(); m.lfo[i].shape = (int) c.lfoShape[i].get();
    }
    for (size_t i = 0; i < (size_t) kNumEnvs; ++i)
    {
        m.env[i].attack = c.envA[i].get(); m.env[i].decay = c.envD[i].get(); m.env[i].sustain = c.envS[i].get();
        m.env[i].release = c.envR[i].get(); m.env[i].trigger = (int) c.envTrig[i].get();
    }
    m.rndRate = c.rndRate.get(); m.rndSmooth = c.rndSmooth.get();
    for (size_t i = 0; i < (size_t) kNumSteps; ++i) m.steps[i] = c.steps[i].get();
    m.seqLength = (int) c.seqLen.get(); m.seqDivision = (int) c.seqDiv.get(); m.seqGlide = c.seqGlide.get();
    for (size_t i = 0; i < (size_t) kNumMacros; ++i) m.macro[i] = c.macro[i].get();
    for (size_t s = 0; s < (size_t) kNumModSlots; ++s)
    {
        m.slots[s].source = (int) c.modSrc[s].get();
        m.slots[s].dest   = (int) c.modDst[s].get() - 1;     // 0 = None
        m.slots[s].amount = c.modAmt[s].get();
    }

    auto& f = fxBase;
    for (size_t i = 0; i < (size_t) NumFx; ++i) f.enabled[i] = c.fxOn[i].get() > 0.5f;
    f.order = unpackOrder (fxOrder.load (std::memory_order_relaxed));
    f.revDecay = c.revDecay.get(); f.revDamp = c.revDamp.get(); f.revPredelayMs = c.revPredelay.get();
    f.dlyTimeMs = c.dlyTime.get(); f.dlySync = c.dlySync.get() > 0.5f; f.dlyDivision = (int) c.dlyDiv.get();
    f.dlyPingPong = c.dlyPP.get(); f.dlyTone = c.dlyTone.get();
    f.distType = (int) c.distType.get(); f.distTone = c.distTone.get(); f.distMix = c.distMix.get();
    f.crushMix = c.crushMix.get();
    f.filtMode = (int) c.filtMode.get(); f.filtDrive = c.filtDrive.get();
    f.chRate = c.chRate.get(); f.chDepth = c.chDepth.get();
    f.phRate = c.phRate.get(); f.phDepth = c.phDepth.get(); f.phFeedback = c.phFb.get();
    f.fsMix = c.fsMix.get(); f.fsFeedback = c.fsFb.get();
    f.imgMonoBassHz = c.imgBass.get();
}

float NubisAudioProcessor::modulated (int d, float base, const float* offsets) noexcept
{
    const auto& dp = destParams[(size_t) d];
    if (std::abs (offsets[d]) < 1.0e-9f)
    {
        modulatedNorm[(size_t) d].store (dp.p->convertTo0to1 (base), std::memory_order_relaxed);
        return base;
    }
    const float n = juce::jlimit (0.0f, 1.0f, dp.p->convertTo0to1 (base) + offsets[d]);
    modulatedNorm[(size_t) d].store (n, std::memory_order_relaxed);
    return dp.p->convertFrom0to1 (n);
}

void NubisAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    if (! prepared || numOut == 0) { buffer.clear(); return; }

    // ---- transport ---------------------------------------------------------
    TransportInfo transport;
    transport.bpm = 120.0;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) transport.bpm = *bpm;
            transport.playing = pos->getIsPlaying();
            if (auto ppq = pos->getPpqPosition()) ppqPosition = *ppq;
        }
    }
    hostBpm.store (transport.bpm, std::memory_order_relaxed);

    buildBaseParams();
    fxBase.bpm = transport.bpm;
    const bool limiterOn = c.limiter.get() > 0.5f;
    outGainSm.setTargetValue (juce::Decibels::decibelsToGain (c.outputGain.get()));
    inGainSm.setTargetValue (juce::Decibels::decibelsToGain (c.inputGain.get()));
    const float limitCeil = juce::Decibels::decibelsToGain (-0.3f);

    const float* inL = numIn > 0 ? buffer.getReadPointer (0) : nullptr;
    const float* inR = numIn > 1 ? buffer.getReadPointer (1) : inL;
    float* outL = buffer.getWritePointer (0);
    float* outR = numOut > 1 ? buffer.getWritePointer (1) : nullptr;

    const int numDests = (int) destParams.size();
    std::array<float, 48> offsets {};
    std::array<bool, 48> hasMod {};
    for (auto& s : modParams.slots) if (s.source > 0 && s.dest >= 0 && s.dest < numDests && std::abs (s.amount) > 1.0e-6f) hasMod[(size_t) s.dest] = true;
    for (int d = 0; d < numDests; ++d) destHasMod[(size_t) d].store (hasMod[(size_t) d], std::memory_order_relaxed);

    auto midiIt = midi.cbegin();
    float peakL = 0.0f, peakR = 0.0f, peakIn = 0.0f;

    for (int pos = 0; pos < numSamples; pos += kSubBlock)
    {
        const int n = std::min (kSubBlock, numSamples - pos);

        // ---- MIDI up to the end of this sub-block ---------------------------
        bool noteOnEvent = false;
        for (; midiIt != midi.cend(); ++midiIt)
        {
            const auto meta = *midiIt;
            if (meta.samplePosition >= pos + n) break;
            const auto msg = meta.getMessage();
            if (msg.isNoteOn())          { engine.noteOn (msg.getNoteNumber(), msg.getFloatVelocity()); noteOnEvent = true; }
            else if (msg.isNoteOff())    engine.noteOff (msg.getNoteNumber());
            else if (msg.isAllNotesOff() || msg.isAllSoundOff()) engine.allNotesOff();
        }

        // ---- dry (input gain applied) -----------------------------------------
        float* dL = dryBuffer.getWritePointer (0);
        float* dR = dryBuffer.getWritePointer (1);
        float inPeak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float g = inGainSm.getNextValue();
            const float l = inL != nullptr ? inL[pos + i] * g : 0.0f;
            const float r = inR != nullptr ? inR[pos + i] * g : l;
            dL[i] = l; dR[i] = r;
            inPeak = std::max (inPeak, std::max (std::abs (l), std::abs (r)));
        }
        peakIn = std::max (peakIn, inPeak);

        // ---- modulation --------------------------------------------------------------
        modEngine.process (n, modParams, transport, engine.anyNoteHeld(), noteOnEvent, inPeak);
        std::fill (offsets.begin(), offsets.end(), 0.0f);
        modEngine.applyMatrix (modParams, offsets.data(), numDests);

        auto& e = engineBase;
        const float* o = offsets.data();
        int d = 0;
        e.sizeMs      = modulated (d++, c.size.get(), o);
        e.density     = modulated (d++, c.density.get(), o);
        e.position    = modulated (d++, c.position.get(), o);
        e.posRandom   = modulated (d++, c.posRandom.get(), o);
        e.pitch       = modulated (d++, c.pitch.get(), o);
        e.pitchRandom = modulated (d++, c.pitchRandom.get(), o);
        e.jitter      = modulated (d++, c.jitter.get(), o);
        e.overlap     = modulated (d++, c.overlap.get(), o);
        e.spread      = modulated (d++, c.spread.get(), o);
        e.pan         = modulated (d++, c.pan.get(), o);
        e.reverse     = modulated (d++, c.reverse.get(), o);
        e.direction   = modulated (d++, c.direction.get(), o);
        e.texture     = modulated (d++, c.texture.get(), o);
        e.smoothness  = modulated (d++, c.smoothness.get(), o);
        e.blur        = modulated (d++, c.blur.get(), o);
        e.randomness  = modulated (d++, c.randomness.get(), o);
        e.diffusion   = modulated (d++, c.diffusion.get(), o);
        e.thickness   = modulated (d++, c.thickness.get(), o);
        e.width       = modulated (d++, c.width.get(), o);
        e.feedback    = modulated (d++, c.feedback.get(), o);
        e.spectralAmount = modulated (d++, c.spectralAmount.get(), o);
        e.chaos       = modulated (d++, c.chaos.get(), o);
        e.evolve      = modulated (d++, c.evolve.get(), o);
        e.reverseCloud = modulated (d++, c.reverseCloud.get(), o);
        e.drone       = modulated (d++, c.drone.get(), o);
        auto& f = fxBase;
        f.filtCutoff  = modulated (d++, c.filtCut.get(), o);
        f.filtReso    = modulated (d++, c.filtRes.get(), o);
        f.revMix      = modulated (d++, c.revMix.get(), o);
        f.revSize     = modulated (d++, c.revSize.get(), o);
        f.dlyMix      = modulated (d++, c.dlyMix.get(), o);
        f.dlyFeedback = modulated (d++, c.dlyFb.get(), o);
        f.distDrive   = modulated (d++, c.distDrive.get(), o);
        f.crushBits   = modulated (d++, c.crushBits.get(), o);
        f.crushDownsample = modulated (d++, c.crushDown.get(), o);
        f.chMix       = modulated (d++, c.chMix.get(), o);
        f.phMix       = modulated (d++, c.phMix.get(), o);
        f.fsShiftHz   = modulated (d++, c.fsShift.get(), o);
        f.imgWidth    = modulated (d++, c.imgWidth.get(), o);
        const float wetMix = modulated (d++, c.dryWet.get(), o);
        jassert (d == numDests);

        const float ampEnv = e.midiMode ? modEngine.getEnvLevel (0) : 1.0f;
        e.ampEnv = ampEnv;

        // ---- engine + fx -------------------------------------------------------------
        float* wL = wetBuffer.getWritePointer (0);
        float* wR = wetBuffer.getWritePointer (1);
        engine.process (numIn > 0 ? dL : nullptr, numIn > 0 ? dR : nullptr, wL, wR, n, e);

        if (e.midiMode || std::abs (ampEnvPrev - ampEnv) > 1.0e-6f)
        {
            for (int i = 0; i < n; ++i)
            {
                const float a = ampEnvPrev + (ampEnv - ampEnvPrev) * (float) (i + 1) / (float) n;
                wL[i] *= a; wR[i] *= a;
            }
        }
        ampEnvPrev = ampEnv;

        fxEngine.process (wL, wR, n, f);

        // ---- mix, gain, protection -----------------------------------------------------
        mixSm.setTargetValue (isInstrument() ? 1.0f : wetMix);
        for (int i = 0; i < n; ++i)
        {
            const float m = mixSm.getNextValue();
            const float g = outGainSm.getNextValue();
            const float dryG = std::cos (m * juce::MathConstants<float>::halfPi);
            const float wetG = std::sin (m * juce::MathConstants<float>::halfPi);
            float l = (dL[i] * dryG + wL[i] * wetG) * g;
            float r = (dR[i] * dryG + wR[i] * wetG) * g;
            if (! std::isfinite (l) || ! std::isfinite (r)) { l = r = 0.0f; fxEngine.reset(); }

            if (limiterOn)
            {
                const float pk = std::max (std::abs (l), std::abs (r));
                const float target = pk > limitCeil ? limitCeil / pk : 1.0f;
                limiterGain = target < limiterGain ? target : target + (limiterGain - target) * limiterRelease;
                l = juce::jlimit (-limitCeil, limitCeil, l * limiterGain);
                r = juce::jlimit (-limitCeil, limitCeil, r * limiterGain);
            }
            else
            {
                l = juce::jlimit (-4.0f, 4.0f, l);
                r = juce::jlimit (-4.0f, 4.0f, r);
            }

            if (outR != nullptr) { outL[pos + i] = l; outR[pos + i] = r; }
            else                   outL[pos + i] = 0.5f * (l + r);
            peakL = std::max (peakL, std::abs (l));
            peakR = std::max (peakR, std::abs (r));
        }

        // ---- analysis feed -------------------------------------------------------------
        {
            int s1, n1, s2, n2;
            analysisFifo.prepareToWrite (n, s1, n1, s2, n2);
            for (int i = 0; i < n1; ++i) analysisData[(size_t) (s1 + i)] = outR != nullptr ? 0.5f * (outL[pos + i] + outR[pos + i]) : outL[pos + i];
            for (int i = 0; i < n2; ++i) analysisData[(size_t) (s2 + i)] = outR != nullptr ? 0.5f * (outL[pos + n1 + i] + outR[pos + n1 + i]) : outL[pos + n1 + i];
            analysisFifo.finishedWrite (n1 + n2);
        }

        if (transport.playing) transport.ppq += (double) n / sampleRateHz * transport.bpm / 60.0;
        else                   transport.ppq = ppqPosition;
    }

    for (int ch = 2; ch < numOut; ++ch) buffer.clear (ch, 0, numSamples);

    meterL.store (std::max (peakL, meterL.load() * 0.85f), std::memory_order_relaxed);
    meterR.store (std::max (peakR, meterR.load() * 0.85f), std::memory_order_relaxed);
    meterIn.store (std::max (peakIn, meterIn.load() * 0.85f), std::memory_order_relaxed);
    gainReductionDb.store (juce::Decibels::gainToDecibels (limiterGain), std::memory_order_relaxed);
    for (int s = 1; s < NumModSources; ++s) sourceValues[(size_t) s].store (modEngine.getSource (s), std::memory_order_relaxed);
    currentSeqStep.store (modEngine.getCurrentStep(), std::memory_order_relaxed);
}

//==============================================================================
void NubisAudioProcessor::timerCallback()
{
    engine.collectGarbage();
}

bool NubisAudioProcessor::loadSampleFile (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr) return false;

    const auto maxLen = (juce::int64) (reader->sampleRate * 60.0);  // up to 60 s
    const int len = (int) std::min (reader->lengthInSamples, maxLen);
    if (len < 256) return false;

    juce::AudioBuffer<float> tmp ((int) std::min<unsigned int> (2, reader->numChannels), len);
    reader->read (&tmp, 0, len, 0, true, tmp.getNumChannels() > 1);

    auto data = std::make_unique<SampleData>();
    data->sampleRate = reader->sampleRate;
    data->path = file.getFullPathName().toStdString();
    data->L.assign (tmp.getReadPointer (0), tmp.getReadPointer (0) + len);
    if (tmp.getNumChannels() > 1) data->R.assign (tmp.getReadPointer (1), tmp.getReadPointer (1) + len);
    else data->R = data->L;

    // normalise quiet material to a healthy level (never boost more than +18 dB)
    float pk = 1.0e-6f;
    for (int i = 0; i < len; ++i) pk = std::max ({ pk, std::abs (data->L[(size_t) i]), std::abs (data->R[(size_t) i]) });
    const float g = std::min (8.0f, 0.8f / pk);
    if (g > 1.0f) for (int i = 0; i < len; ++i) { data->L[(size_t) i] *= g; data->R[(size_t) i] *= g; }

    data->finalise();
    sampleOverview = data->overview;
    engine.submitSample (std::move (data));
    sampleFile = file;
    sampleLoaded = true;
    return true;
}

void NubisAudioProcessor::clearSample()
{
    engine.clearSample();
    sampleFile = juce::File();
    sampleOverview.fill (0.0f);
    sampleLoaded = false;
}

void NubisAudioProcessor::setFxOrder (const std::array<int, NumFx>& order)
{
    fxOrder.store (packOrder (order));
}

void NubisAudioProcessor::moveFx (int fxId, int delta)
{
    auto order = getFxOrder();
    for (int i = 0; i < NumFx; ++i)
    {
        if (order[(size_t) i] == fxId)
        {
            const int j = juce::jlimit (0, NumFx - 1, i + delta);
            std::swap (order[(size_t) i], order[(size_t) j]);
            break;
        }
    }
    setFxOrder (order);
}

int NubisAudioProcessor::addModulation (int source, const juce::String& destParamId, float amount)
{
    const int d = findModDestination (destParamId);
    if (d < 0 || source <= 0 || source >= NumModSources) return -1;
    auto set = [this] (const juce::String& id, float plain)
    {
        if (auto* p = apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (plain)); p->endChangeGesture(); }
    };
    // re-use an existing slot for the same pair
    for (int s = 0; s < kNumModSlots; ++s)
        if ((int) c.modSrc[(size_t) s].get() == source && (int) c.modDst[(size_t) s].get() == d + 1) return s;
    for (int s = 0; s < kNumModSlots; ++s)
    {
        if ((int) c.modSrc[(size_t) s].get() == 0 || (int) c.modDst[(size_t) s].get() == 0)
        {
            set (pid::modSrc (s), (float) source);
            set (pid::modDst (s), (float) (d + 1));
            set (pid::modAmt (s), amount);
            return s;
        }
    }
    return -1;
}

void NubisAudioProcessor::clearSlot (int s)
{
    for (auto id : { pid::modSrc (s), pid::modDst (s), pid::modAmt (s) })
        if (auto* p = apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture(); }
}

void NubisAudioProcessor::clearModulationsFor (const juce::String& destParamId)
{
    const int d = findModDestination (destParamId);
    if (d < 0) return;
    for (int s = 0; s < kNumModSlots; ++s)
        if ((int) c.modDst[(size_t) s].get() == d + 1) clearSlot (s);
}

//==============================================================================
void NubisAudioProcessor::randomize (bool experimental)
{
    juce::Random r;
    auto set = [this] (const juce::String& id, float plain)
    {
        if (auto* p = apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (plain)); p->endChangeGesture(); }
    };
    auto rf = [&r] (float lo, float hi) { return lo + r.nextFloat() * (hi - lo); };
    auto chance = [&r] (float p) { return r.nextFloat() < p; };
    // log-uniform helper
    auto rlog = [&r] (float lo, float hi) { return lo * std::pow (hi / lo, r.nextFloat()); };

    // Musical constraints: grain size and density are chosen together so the cloud is
    // neither silent (tiny grains, low density) nor a wall of mud. Feedback, drive and
    // crushing are capped; output safety stays on.
    const float sizeMs = experimental ? rlog (4.0f, 1500.0f) : rlog (40.0f, 1200.0f);
    const float targetOverlap = experimental ? rf (0.3f, 6.0f) : rf (1.5f, 5.0f);
    const float densityV = juce::jlimit (1.0f, experimental ? 350.0f : 120.0f, targetOverlap * 1000.0f / sizeMs);

    set (pid::size, sizeMs);
    set (pid::density, densityV);
    set (pid::position, rf (0.3f, 0.95f));
    set (pid::posRandom, rf (0.0f, experimental ? 0.9f : 0.4f));
    set (pid::pitch, chance (0.6f) ? 0.0f : (float) (r.nextInt (5) - 2) * 12.0f + (chance (0.3f) ? 7.0f : 0.0f));
    set (pid::pitchRandom, chance (0.5f) ? 0.0f : rf (0.05f, experimental ? 24.0f : 7.0f));
    set (pid::jitter, rf (0.0f, 1.0f));
    set (pid::overlap, rf (0.2f, 1.0f));
    set (pid::spread, rf (0.2f, 1.0f));
    set (pid::pan, 0.0f);
    set (pid::reverse, chance (0.4f) ? rf (0.2f, 1.0f) : 0.0f);
    set (pid::direction, chance (0.5f) ? 0.0f : rf (-0.5f, 0.5f));
    set (pid::envelope, (float) r.nextInt ((int) GrainEnvelope::NumShapes));

    set (pid::texture, rf (0.2f, experimental ? 1.0f : 0.7f));
    set (pid::smoothness, rf (0.0f, 0.5f));
    set (pid::blur, chance (0.4f) ? rf (0.1f, 0.7f) : 0.0f);
    set (pid::randomness, rf (0.0f, experimental ? 0.8f : 0.3f));
    set (pid::diffusion, rf (0.0f, 0.7f));
    set (pid::thickness, chance (0.5f) ? rf (0.2f, 0.9f) : 0.0f);
    set (pid::width, rf (0.9f, 1.6f));
    set (pid::feedback, chance (0.3f) ? rf (0.1f, 0.55f) : 0.0f);

    set (pid::spectralMode, chance (0.5f) ? (float) (1 + r.nextInt (6)) : 0.0f);
    set (pid::spectralAmount, rf (0.4f, 1.0f));
    set (pid::chaos, experimental ? rf (0.0f, 0.7f) : (chance (0.3f) ? rf (0.05f, 0.3f) : 0.0f));
    set (pid::evolve, chance (0.5f) ? rf (0.1f, 0.7f) : 0.0f);
    set (pid::evolveRate, rlog (0.02f, 0.5f));
    set (pid::reverseCloud, chance (0.2f) ? rf (0.3f, 1.0f) : 0.0f);
    set (pid::drone, chance (0.25f) ? rf (0.3f, 1.0f) : 0.0f);

    // effects: a few at a time, safe ranges
    for (int f = 0; f < NumFx; ++f) set (pid::fxOn (f), 0.0f);
    set (pid::fxOn (FxReverb), chance (0.8f) ? 1.0f : 0.0f);
    set (pid::revMix, rf (0.15f, 0.5f));
    set (pid::revDecay, rlog (1.0f, 12.0f));
    set (pid::revSize, rf (0.4f, 1.0f));
    if (chance (0.4f)) { set (pid::fxOn (FxDelay), 1.0f); set (pid::dlyMix, rf (0.1f, 0.35f)); set (pid::dlyFb, rf (0.2f, 0.6f)); set (pid::dlyDiv, (float) (3 + r.nextInt (8))); }
    if (chance (0.3f)) { set (pid::fxOn (FxFilter), 1.0f); set (pid::filtMode, (float) r.nextInt (3)); set (pid::filtCut, rlog (300.0f, 9000.0f)); set (pid::filtRes, rf (0.0f, 0.6f)); }
    if (chance (0.3f)) { set (pid::fxOn (FxChorus), 1.0f); set (pid::chMix, rf (0.2f, 0.5f)); }
    if (chance (experimental ? 0.4f : 0.15f)) { set (pid::fxOn (FxDistortion), 1.0f); set (pid::distDrive, rf (0.1f, 0.5f)); set (pid::distType, (float) r.nextInt (4)); set (pid::distMix, rf (0.3f, 0.8f)); }
    if (chance (experimental ? 0.4f : 0.1f)) { set (pid::fxOn (FxBitcrusher), 1.0f); set (pid::crushBits, rf (6.0f, 14.0f)); set (pid::crushDown, rlog (1.0f, 12.0f)); set (pid::crushMix, rf (0.2f, 0.6f)); }
    if (chance (0.2f)) { set (pid::fxOn (FxPhaser), 1.0f); set (pid::phMix, rf (0.3f, 0.6f)); }
    if (chance (experimental ? 0.4f : 0.15f)) { set (pid::fxOn (FxFreqShifter), 1.0f); set (pid::fsShift, rf (-300.0f, 300.0f)); set (pid::fsMix, rf (0.2f, 0.5f)); set (pid::fsFb, rf (0.0f, 0.4f)); }

    // a couple of gentle modulations
    for (int s = 0; s < kNumModSlots; ++s) clearSlot (s);
    const char* targets[] = { pid::position, pid::size, pid::blur, pid::filtCut, pid::pan, pid::pitchRandom, pid::diffusion, pid::spread };
    const int numMods = 1 + r.nextInt (experimental ? 4 : 2);
    for (int i = 0; i < numMods; ++i)
    {
        const int src = 1 + r.nextInt (4); // LFOs
        set (pid::lfo (src - 1, "rate"), rlog (0.03f, experimental ? 6.0f : 0.8f));
        set (pid::lfo (src - 1, "shape"), (float) r.nextInt (6));
        addModulation (src, targets[r.nextInt (8)], rf (-0.35f, 0.35f));
    }
    presets.setCurrentName (experimental ? "Random (Experimental)" : "Random (Musical)");
    if (presets.onPresetLoaded) presets.onPresetLoaded();
}

//==============================================================================
int NubisAudioProcessor::getNumPrograms()   { return std::max (1, (int) getFactoryPresets().size()); }
int NubisAudioProcessor::getCurrentProgram() { return std::max (0, presets.getCurrentIndex()); }
void NubisAudioProcessor::setCurrentProgram (int index)
{
    if (juce::isPositiveAndBelow (index, (int) getFactoryPresets().size()) && index != presets.getCurrentIndex())
        presets.loadPreset (index);
}
const juce::String NubisAudioProcessor::getProgramName (int index)
{
    const auto& f = getFactoryPresets();
    return juce::isPositiveAndBelow (index, (int) f.size()) ? juce::String (f[(size_t) index].name) : juce::String ("Init");
}

//==============================================================================
void NubisAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    const auto order = getFxOrder();
    juce::StringArray o;
    for (auto v : order) o.add (juce::String (v));
    state.setProperty ("fxOrder", o.joinIntoString (","), nullptr);
    state.setProperty ("presetName", presets.getCurrentName(), nullptr);
    state.setProperty ("samplePath", sampleFile.getFullPathName(), nullptr);
    state.setProperty ("version", JucePlugin_VersionString, nullptr);

    auto xml = state.createXml();

    // Infinite Freeze: the captured audio is stored inside the project (16-bit PCM).
    if (c.infFreeze.get() > 0.5f)
    {
        std::vector<float> l, r;
        if (engine.copyFrozenAudio (l, r) && ! l.empty())
        {
            juce::MemoryBlock mb (l.size() * 2 * sizeof (int16_t));
            auto* d = static_cast<int16_t*> (mb.getData());
            for (size_t i = 0; i < l.size(); ++i)
            {
                d[2 * i]     = (int16_t) juce::jlimit (-32767, 32767, juce::roundToInt (l[i] * 32767.0f));
                d[2 * i + 1] = (int16_t) juce::jlimit (-32767, 32767, juce::roundToInt (r[i] * 32767.0f));
            }
            auto* fz = xml->createNewChildElement ("FrozenAudio");
            fz->setAttribute ("rate", sampleRateHz);
            fz->setAttribute ("frames", (int) l.size());
            fz->addTextElement (mb.toBase64Encoding());
        }
    }
    copyXmlToBinary (*xml, destData);
}

void NubisAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    std::vector<float> frozenL, frozenR;
    double frozenRate = 0.0;
    if (auto* fz = xml->getChildByName ("FrozenAudio"))
    {
        juce::MemoryBlock mb;
        if (mb.fromBase64Encoding (fz->getAllSubText().trim()))
        {
            const size_t frames = mb.getSize() / (2 * sizeof (int16_t));
            const auto* d = static_cast<const int16_t*> (mb.getData());
            frozenL.resize (frames); frozenR.resize (frames);
            for (size_t i = 0; i < frames; ++i) { frozenL[i] = d[2 * i] / 32767.0f; frozenR[i] = d[2 * i + 1] / 32767.0f; }
            frozenRate = fz->getDoubleAttribute ("rate", sampleRateHz);
        }
        xml->removeChildElement (fz, true);
    }

    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    const auto& st = apvts.state;

    juce::StringArray parts;
    parts.addTokens (st.getProperty ("fxOrder", "0,1,2,3,4,5,6,7,8").toString(), ",", "");
    if (parts.size() == NumFx)
    {
        std::array<int, NumFx> order {};
        for (int i = 0; i < NumFx; ++i) order[(size_t) i] = parts[i].getIntValue();
        setFxOrder (unpackOrder (packOrder (order)));
    }
    presets.setCurrentName (st.getProperty ("presetName", "Init").toString());

    const juce::File sf (st.getProperty ("samplePath", "").toString());
    if (sf.existsAsFile()) loadSampleFile (sf);

    if (! frozenL.empty())
    {
        if (prepared) restoreFrozen (std::move (frozenL), std::move (frozenR), frozenRate);
        else { pendingFrozenL = std::move (frozenL); pendingFrozenR = std::move (frozenR); pendingFrozenRate = frozenRate; }
    }
    if (presets.onPresetLoaded) presets.onPresetLoaded();
}

//==============================================================================
juce::AudioProcessorEditor* NubisAudioProcessor::createEditor() { return new NubisAudioProcessorEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NubisAudioProcessor(); }
