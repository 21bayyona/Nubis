// =============================================================================
//  NUBIS - GrainVisualizer.h : live grain cloud view + real-time spectrogram
// =============================================================================
#pragma once

#include "Theme.h"
#include <juce_dsp/juce_dsp.h>

class NubisAudioProcessor;

namespace nubis
{
class GrainVisualizer : public juce::Component
{
public:
    explicit GrainVisualizer (NubisAudioProcessor& p);
    void paint (juce::Graphics&) override;
    void tick (float dt);   // called by editor timer

private:
    struct Particle
    {
        float pos = 0, pitch = 0, pan = 0, life = 0, maxLife = 1, gain = 1, speed = 0;
        bool reverse = false, alive = false;
    };
    float toX (float absPos, juce::Rectangle<float> area) const;

    NubisAudioProcessor& proc;
    std::array<Particle, 600> particles;
    int nextParticle = 0;
    float writeHead = 0.0f, playhead = 0.0f, regionStart = 0.0f, regionLen = 1.0f;
    int source = 0, grainCount = 0;
    bool frozen = false;
    float time = 0.0f;
};

//==============================================================================
class Spectrogram : public juce::Component
{
public:
    explicit Spectrogram (NubisAudioProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();

private:
    static constexpr int kOrder = 11, kSize = 1 << kOrder;
    NubisAudioProcessor& proc;
    juce::dsp::FFT fft { kOrder };
    juce::dsp::WindowingFunction<float> window { kSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, kSize * 2> fftData {};
    std::array<float, kSize> ring {};
    int ringPos = 0, samplesSinceColumn = 0;
    juce::Image image;
    juce::Colour colourFor (float level) const;
};

} // namespace nubis
