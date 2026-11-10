// =============================================================================
//  NUBIS - GrainVisualizer.cpp
// =============================================================================
#include "GrainVisualizer.h"
#include "../PluginProcessor.h"

namespace nubis
{
using namespace theme;

GrainVisualizer::GrainVisualizer (NubisAudioProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (false, false);
    setOpaque (false);
}

float GrainVisualizer::toX (float absPos, juce::Rectangle<float> area) const
{
    float n = absPos;
    if (source == (int) SourceMode::Live)
    {
        // newest audio on the right: rotate the circular buffer around the write head
        n = absPos - writeHead;
        n -= std::floor (n);
    }
    return area.getX() + n * area.getWidth();
}

void GrainVisualizer::tick (float dt)
{
    time += dt;
    auto& e = proc.engine;
    writeHead   = e.writeHeadNorm.load();
    playhead    = e.playheadNorm.load();
    regionStart = e.regionStartNorm.load();
    regionLen   = e.regionLenNorm.load();
    frozen      = e.isFrozen.load();
    source      = (int) proc.apvts.getRawParameterValue (pid::source)->load();
    grainCount  = e.activeGrainCount.load();

    GrainEvent ev;
    int pulled = 0;
    while (e.grainEvents.pop (ev) && pulled < 256)
    {
        ++pulled;
        auto& pt = particles[(size_t) nextParticle];
        nextParticle = (nextParticle + 1) % (int) particles.size();
        pt.alive = true;
        pt.pos = ev.position;
        pt.pitch = ev.pitch;
        pt.pan = ev.pan;
        pt.maxLife = std::max (0.06f, std::min (ev.lengthSec, 2.5f));
        pt.life = 0.0f;
        pt.gain = std::max (0.25f, ev.gain);
        pt.reverse = ev.reverse != 0;
        // travel speed across the view, normalised (buffer fraction per second)
        pt.speed = std::exp2 (ev.pitch / 12.0f) * e.displayRateScale.load() * (pt.reverse ? -1.0f : 1.0f);
    }
    for (auto& pt : particles)
    {
        if (! pt.alive) continue;
        pt.life += dt;
        pt.pos += pt.speed * dt;
        if (pt.life >= pt.maxLife) pt.alive = false;
    }
    repaint();
}

void GrainVisualizer::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (well);
    g.fillRoundedRectangle (bounds, 8.0f);

    auto area = bounds.reduced (10.0f, 12.0f);
    const float midY = area.getCentreY();

    // grid
    g.setColour (line.withAlpha (0.6f));
    for (int i = 1; i < 8; ++i)
    {
        const float x = area.getX() + area.getWidth() * (float) i / 8.0f;
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
    }
    for (int st = -24; st <= 24; st += 12)
    {
        const float y = juce::jmap ((float) st, -30.0f, 30.0f, area.getBottom(), area.getY());
        g.setColour (st == 0 ? lineHi : line.withAlpha (0.5f));
        g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
    }

    // waveform overview
    const auto* ovLive = &proc.engine.liveOverview;
    const auto& ovInt = proc.engine.getInternalOverview ((int) proc.apvts.getRawParameterValue (pid::internalType)->load());
    const auto& ovSmp = proc.getSampleOverview();
    const int bins = kOverviewBins;
    juce::Path wave;
    const float halfH = area.getHeight() * 0.42f;
    for (int b = 0; b < bins; ++b)
    {
        float v = 0.0f, absPos = (float) b / (float) bins;
        if (source == (int) SourceMode::Live) v = (*ovLive)[(size_t) b].load();
        else if (source == (int) SourceMode::Internal) v = ovInt[(size_t) b];
        else v = ovSmp[(size_t) b];
        v = std::pow (juce::jlimit (0.0f, 1.0f, v), 0.6f);
        const float x = toX (absPos, area);
        const float w = std::max (1.0f, area.getWidth() / (float) bins);
        wave.addRectangle (x, midY - v * halfH, w, std::max (1.0f, 2.0f * v * halfH));
    }
    g.setColour (textFaint.withAlpha (0.55f));
    g.fillPath (wave);

    // active region (live history or frozen loop)
    if (source == (int) SourceMode::Live)
    {
        const float x0 = toX (regionStart, area);
        const float w = regionLen * area.getWidth();
        const auto reg = juce::Rectangle<float> (std::min (x0, area.getRight() - w), area.getY(), w, area.getHeight());
        g.setColour ((frozen ? ice : accentHi).withAlpha (0.05f));
        g.fillRect (reg);
        if (frozen)
        {
            g.setColour (ice.withAlpha (0.5f));
            g.drawRect (reg, 1.0f);
        }
        // write head
        if (! frozen)
        {
            g.setColour (accentHi.withAlpha (0.8f));
            g.fillRect (area.getRight() - 2.0f, area.getY(), 2.0f, area.getHeight());
        }
    }

    // playhead
    {
        const float x = toX (playhead, area);
        juce::ColourGradient grad (accentHi.withAlpha (0.0f), x - 26.0f, 0, accentHi.withAlpha (0.22f), x, 0, false);
        g.setGradientFill (grad);
        g.fillRect (juce::Rectangle<float> (x - 26.0f, area.getY(), 26.0f, area.getHeight()));
        g.setColour (accentHi);
        g.fillRect (x - 0.75f, area.getY(), 1.5f, area.getHeight());
    }

    // grains
    for (auto& pt : particles)
    {
        if (! pt.alive) continue;
        const float t = pt.life / pt.maxLife;
        const float env = std::sin (juce::MathConstants<float>::pi * juce::jlimit (0.0f, 1.0f, t));
        float x = toX (pt.pos - std::floor (pt.pos), area);
        const float y = juce::jmap (juce::jlimit (-30.0f, 30.0f, pt.pitch), -30.0f, 30.0f, area.getBottom(), area.getY()) + pt.pan * 6.0f;
        const float rad = 1.6f + 3.2f * pt.gain * env;
        const auto col = pt.reverse ? ice : accentHi;
        g.setColour (col.withAlpha (0.12f * env));
        g.fillEllipse (x - rad * 3.0f, y - rad * 3.0f, rad * 6.0f, rad * 6.0f);
        g.setColour (col.withAlpha (0.85f * env));
        g.fillEllipse (x - rad, y - rad, rad * 2.0f, rad * 2.0f);
        // motion trail
        const float tail = juce::jlimit (-40.0f, 40.0f, -pt.speed * area.getWidth() * 0.05f);
        g.setColour (col.withAlpha (0.35f * env));
        g.drawLine (x, y, x + tail, y, 1.0f);
    }

    // overlay text
    g.setFont (mono (11.0f));
    g.setColour (textDim);
    juce::String srcTxt = kSourceNames[juce::jlimit (0, 2, source)];
    g.drawText (srcTxt.toUpperCase() + "   " + juce::String (grainCount) + " GRAINS", area.removeFromTop (14.0f), juce::Justification::topLeft);
    if (frozen && source == (int) SourceMode::Live)
    {
        const auto badge = juce::Rectangle<float> (bounds.getRight() - 92.0f, bounds.getY() + 10.0f, 80.0f, 18.0f);
        g.setColour (ice.withAlpha (0.15f));
        g.fillRoundedRectangle (badge, 9.0f);
        g.setColour (ice);
        g.setFont (font (10.5f, true).withExtraKerningFactor (0.15f));
        g.drawText ("FROZEN", badge, juce::Justification::centred);
    }
    if (source == (int) SourceMode::Sample && ! proc.hasSample())
    {
        g.setColour (textDim);
        g.setFont (font (15.0f));
        g.drawText ("Drop an audio file here or press LOAD", bounds, juce::Justification::centred);
    }
    if (source == (int) SourceMode::Live && NubisAudioProcessor::isInstrument() && ! frozen)
    {
        g.setColour (textDim);
        g.setFont (font (14.0f));
        g.drawText ("Instrument build has no audio input - choose Sample or Internal", bounds, juce::Justification::centred);
    }
}

//==============================================================================
Spectrogram::Spectrogram (NubisAudioProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (false, false);
    setOpaque (true);
}

void Spectrogram::resized()
{
    image = juce::Image (juce::Image::RGB, std::max (1, getWidth()), std::max (1, getHeight()), true);
    image.clear (image.getBounds(), well);
}

juce::Colour Spectrogram::colourFor (float level) const
{
    level = juce::jlimit (0.0f, 1.0f, level);
    if (level < 0.5f) return well.interpolatedWith (accent, level * 2.0f);
    return accent.interpolatedWith (accentHi, (level - 0.5f) * 1.4f).interpolatedWith (juce::Colours::white, std::max (0.0f, level - 0.85f) * 3.0f);
}

void Spectrogram::tick()
{
    auto& fifo = proc.analysisFifo;
    int s1, n1, s2, n2;
    const int ready = fifo.getNumReady();
    if (ready <= 0 || ! image.isValid()) return;
    fifo.prepareToRead (ready, s1, n1, s2, n2);
    auto push = [this] (const float* d, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            ring[(size_t) ringPos] = d[i];
            ringPos = (ringPos + 1) % kSize;
            ++samplesSinceColumn;
        }
    };
    push (proc.analysisData.data() + s1, n1);
    push (proc.analysisData.data() + s2, n2);
    fifo.finishedRead (n1 + n2);

    const int hop = 1024;
    while (samplesSinceColumn >= hop)
    {
        samplesSinceColumn -= hop;
        for (int i = 0; i < kSize; ++i) fftData[(size_t) i] = ring[(size_t) ((ringPos + i) % kSize)];
        std::fill (fftData.begin() + kSize, fftData.end(), 0.0f);
        window.multiplyWithWindowingTable (fftData.data(), kSize);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        const int w = image.getWidth(), h = image.getHeight();
        image.moveImageSection (0, 0, 1, 0, w - 1, h);
        const double sr = std::max (1.0, proc.engine.getSampleRate());
        for (int y = 0; y < h; ++y)
        {
            const float norm = 1.0f - (float) y / (float) (h - 1);
            const float freq = 30.0f * std::pow (18000.0f / 30.0f, norm);
            const int bin = juce::jlimit (1, kSize / 2 - 1, (int) (freq / sr * kSize));
            const float mag = fftData[(size_t) bin] / (float) kSize * 4.0f;
            const float db = juce::Decibels::gainToDecibels (mag, -100.0f);
            image.setPixelAt (w - 1, y, colourFor (juce::jmap (db, -90.0f, -10.0f, 0.0f, 1.0f)));
        }
    }
    repaint();
}

void Spectrogram::paint (juce::Graphics& g)
{
    g.drawImageAt (image, 0, 0);
    g.setColour (textFaint);
    g.setFont (mono (9.5f));
    g.drawText ("SPECTRUM", getLocalBounds().reduced (6, 3), juce::Justification::topLeft);
    g.setColour (line);
    g.drawRect (getLocalBounds(), 1);
}

} // namespace nubis
