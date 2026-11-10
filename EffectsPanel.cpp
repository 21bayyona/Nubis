// =============================================================================
//  NUBIS - EffectsPanel.cpp
// =============================================================================
#include "EffectsPanel.h"
#include "../PluginProcessor.h"

namespace nubis
{
using namespace theme;

namespace
{
    //==========================================================================
    class FxChip : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        FxChip (NubisAudioProcessor& p, EffectsPanel& o, int id) : owner (o), fx (id), power (p, pid::fxOn (id), "", true)
        {
            power.setTooltip (juce::String ("Enable ") + kFxNames[id]);
            addAndMakeVisible (power);
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
            setTooltip (juce::String (kFxNames[id]) + "  -  click to edit, drag to re-order");
        }
        void resized() override { power.setBounds (getLocalBounds().removeFromLeft (22).reduced (3, 6)); }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (1.0f);
            g.setColour (selected ? panelHi.brighter (0.06f) : well);
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (selected ? accentHi : (isMouseOver (true) ? lineHi : line));
            g.drawRoundedRectangle (r, 6.0f, 1.0f);
            g.setColour (power.getToggleState() ? text : textDim);
            g.setFont (font (11.5f, true).withExtraKerningFactor (0.06f));
            g.drawFittedText (kFxNames[fx], getLocalBounds().withTrimmedLeft (22).withTrimmedRight (4), juce::Justification::centred, 1, 0.75f);
        }
        void mouseDown (const juce::MouseEvent&) override { dragging = false; owner.selectFx (fx); }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (e.getDistanceFromDragStart() > 5) dragging = true;
            if (dragging) owner.chipDragged (fx, e.getEventRelativeTo (&owner).x);
        }
        void mouseUp (const juce::MouseEvent& e) override { if (dragging) owner.chipReleased (fx, e.getEventRelativeTo (&owner).x); dragging = false; }
        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override { repaint(); }
        bool selected = false;
    private:
        EffectsPanel& owner;
        int fx;
        ParamToggle power;
        bool dragging = false;
    };

    //==========================================================================
    class FxPage : public juce::Component
    {
    public:
        FxPage (NubisAudioProcessor& p, int id) : proc (p), fx (id) {}
        ParamKnob& knob (const juce::String& pidv, const juce::String& label, bool bip = false)
        {
            knobs.push_back (std::make_unique<ParamKnob> (proc, pidv, label, bip));
            addAndMakeVisible (*knobs.back());
            items.push_back (knobs.back().get());
            return *knobs.back();
        }
        void combo (const juce::String& pidv, const juce::String& cap)
        {
            combos.push_back (std::make_unique<ParamCombo> (proc, pidv));
            addAndMakeVisible (*combos.back());
            caps.push_back (std::make_unique<Caption> (cap));
            addAndMakeVisible (*caps.back());
            items.push_back (combos.back().get());
        }
        void toggle (const juce::String& pidv, const juce::String& t)
        {
            toggles.push_back (std::make_unique<ParamToggle> (proc, pidv, t));
            addAndMakeVisible (*toggles.back());
            items.push_back (toggles.back().get());
        }
        void resized() override
        {
            auto r = getLocalBounds();
            int capIdx = 0;
            for (auto* c : items)
            {
                if (auto* cb = dynamic_cast<ParamCombo*> (c))
                {
                    auto col = r.removeFromLeft (118).reduced (6, 0);
                    col.removeFromTop (22);
                    caps[(size_t) capIdx++]->setBounds (col.removeFromTop (16));
                    cb->setBounds (col.removeFromTop (26));
                }
                else if (auto* t = dynamic_cast<ParamToggle*> (c))
                {
                    auto col = r.removeFromLeft (84).reduced (6, 0);
                    col.removeFromTop (38);
                    t->setBounds (col.removeFromTop (26));
                }
                else c->setBounds (r.removeFromLeft (86).reduced (4, 2));
            }
        }
        void paint (juce::Graphics& g) override
        {
            g.setColour (textFaint);
            g.setFont (font (10.5f));
            g.drawText (description(), getLocalBounds().removeFromBottom (14).withTrimmedRight (6), juce::Justification::centredRight);
        }
        juce::String description() const
        {
            switch (fx)
            {
                case FxReverb:      return "8-line FDN reverb";
                case FxDelay:       return "stereo / ping-pong delay, tempo synced";
                case FxDistortion:  return "2x oversampled waveshaper";
                case FxBitcrusher:  return "bit depth + sample-rate reduction";
                case FxFilter:      return "TPT state-variable filter";
                case FxChorus:      return "3-voice stereo chorus";
                case FxPhaser:      return "6-stage phaser";
                case FxFreqShifter: return "Hilbert single-sideband shifter";
                case FxImager:      return "mid/side width + mono bass";
                default:            return {};
            }
        }
        std::vector<std::unique_ptr<ParamKnob>> knobs;
    private:
        NubisAudioProcessor& proc;
        int fx;
        std::vector<std::unique_ptr<ParamCombo>> combos;
        std::vector<std::unique_ptr<ParamToggle>> toggles;
        std::vector<std::unique_ptr<Caption>> caps;
        std::vector<juce::Component*> items;
    };

    //==========================================================================
    class Meter : public juce::Component
    {
    public:
        explicit Meter (NubisAudioProcessor& p) : proc (p) {}
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            auto bar = [&] (juce::Rectangle<float> b, float lin, const juce::String& lbl)
            {
                g.setColour (well);
                g.fillRoundedRectangle (b, 3.0f);
                const float db = juce::Decibels::gainToDecibels (lin, -60.0f);
                const float n = juce::jmap (db, -60.0f, 3.0f, 0.0f, 1.0f);
                auto fill = b.withTop (b.getBottom() - n * b.getHeight());
                juce::ColourGradient grad (accent, b.getX(), b.getBottom(), lin > 0.97f ? juce::Colours::white : accentHi, b.getX(), b.getY(), false);
                g.setGradientFill (grad);
                g.fillRoundedRectangle (fill, 3.0f);
                g.setColour (textFaint);
                g.setFont (mono (9.0f));
                g.drawText (lbl, b.withY (b.getBottom() + 2).withHeight (12), juce::Justification::centred);
            };
            auto meters = r.withTrimmedBottom (14);
            const float w = 9.0f;
            bar (meters.removeFromLeft (w), proc.meterIn.load(), "IN");
            meters.removeFromLeft (6);
            bar (meters.removeFromLeft (w), proc.meterL.load(), "L");
            meters.removeFromLeft (3);
            bar (meters.removeFromLeft (w), proc.meterR.load(), "R");
            meters.removeFromLeft (6);
            // gain reduction
            auto gr = meters.removeFromLeft (w);
            g.setColour (well);
            g.fillRoundedRectangle (gr, 3.0f);
            const float red = juce::jlimit (0.0f, 1.0f, -proc.gainReductionDb.load() / 12.0f);
            g.setColour (modCol);
            g.fillRoundedRectangle (gr.withHeight (red * gr.getHeight()), 3.0f);
            g.setColour (textFaint);
            g.setFont (mono (9.0f));
            g.drawText ("GR", gr.withY (gr.getBottom() + 2).withHeight (12), juce::Justification::centred);
            // scale ticks
            g.setColour (textFaint);
            for (float db : { 0.0f, -12.0f, -24.0f, -48.0f })
            {
                const float y = juce::jmap (db, -60.0f, 3.0f, meters.getBottom(), meters.getY());
                g.drawText (juce::String ((int) db), juce::Rectangle<float> (meters.getX() + 2, y - 5, 26, 10), juce::Justification::centredLeft);
            }
        }
    private:
        NubisAudioProcessor& proc;
    };

    class OutputSection : public juce::Component
    {
    public:
        explicit OutputSection (NubisAudioProcessor& p)
            : input (p, pid::inputGain, "Input", true), mix (p, pid::dryWet, "Dry / Wet"), out (p, pid::outputGain, "Output", true),
              limiter (p, pid::limiter, "LIMIT"), meter (p)
        {
            for (auto* c : std::initializer_list<juce::Component*> { &input, &mix, &out, &limiter, &meter }) addAndMakeVisible (c);
            if (NubisAudioProcessor::isInstrument()) { input.setEnabled (false); mix.setEnabled (false); }
        }
        void resized() override
        {
            auto r = getLocalBounds();
            meter.setBounds (r.removeFromRight (78).reduced (0, 4));
            auto knobs = r.removeFromTop (r.getHeight() - 30);
            const int w = knobs.getWidth() / 3;
            input.setBounds (knobs.removeFromLeft (w));
            mix.setBounds (knobs.removeFromLeft (w));
            out.setBounds (knobs);
            limiter.setBounds (r.reduced (40, 2).withTrimmedTop (2));
        }
        ParamKnob input, mix, out;
        ParamToggle limiter;
        Meter meter;
    };
} // namespace

//==============================================================================
EffectsPanel::EffectsPanel (NubisAudioProcessor& p) : proc (p)
{
    for (int f = 0; f < NumFx; ++f)
    {
        chips.push_back (std::make_unique<FxChip> (p, *this, f));
        addAndMakeVisible (*chips.back());

        auto page = std::make_unique<FxPage> (p, f);
        auto& pg = *page;
        switch (f)
        {
            case FxReverb:
                pg.knob (pid::revSize, "Size"); pg.knob (pid::revDecay, "Decay"); pg.knob (pid::revDamp, "Damping");
                pg.knob (pid::revPredelay, "Pre-delay"); pg.knob (pid::revMix, "Mix"); break;
            case FxDelay:
                pg.knob (pid::dlyTime, "Time"); pg.toggle (pid::dlySync, "SYNC"); pg.combo (pid::dlyDiv, "Division");
                pg.knob (pid::dlyFb, "Feedback"); pg.knob (pid::dlyPP, "Ping-Pong"); pg.knob (pid::dlyTone, "Tone"); pg.knob (pid::dlyMix, "Mix"); break;
            case FxDistortion:
                pg.combo (pid::distType, "Type"); pg.knob (pid::distDrive, "Drive"); pg.knob (pid::distTone, "Tone"); pg.knob (pid::distMix, "Mix"); break;
            case FxBitcrusher:
                pg.knob (pid::crushBits, "Bits"); pg.knob (pid::crushDown, "Rate Div"); pg.knob (pid::crushMix, "Mix"); break;
            case FxFilter:
                pg.combo (pid::filtMode, "Mode"); pg.knob (pid::filtCut, "Cutoff"); pg.knob (pid::filtRes, "Resonance"); pg.knob (pid::filtDrive, "Drive"); break;
            case FxChorus:
                pg.knob (pid::chRate, "Rate"); pg.knob (pid::chDepth, "Depth"); pg.knob (pid::chMix, "Mix"); break;
            case FxPhaser:
                pg.knob (pid::phRate, "Rate"); pg.knob (pid::phDepth, "Depth"); pg.knob (pid::phFb, "Feedback"); pg.knob (pid::phMix, "Mix"); break;
            case FxFreqShifter:
                pg.knob (pid::fsShift, "Shift", true); pg.knob (pid::fsFb, "Feedback"); pg.knob (pid::fsMix, "Mix"); break;
            case FxImager:
                pg.knob (pid::imgWidth, "Width"); pg.knob (pid::imgBass, "Mono Bass"); break;
            default: break;
        }
        pages.push_back (std::move (page));
        addChildComponent (*pages.back());
    }
    output = std::make_unique<OutputSection> (p);
    addAndMakeVisible (*output);

    moveLeft.setTooltip ("Move selected effect earlier in the chain");
    moveRight.setTooltip ("Move selected effect later in the chain");
    moveLeft.onClick  = [this] { proc.moveFx (selected, -1); refreshOrder(); };
    moveRight.onClick = [this] { proc.moveFx (selected, +1); refreshOrder(); };
    addAndMakeVisible (moveLeft);
    addAndMakeVisible (moveRight);

    order = proc.getFxOrder();
    selectFx (0);
}

EffectsPanel::~EffectsPanel() = default;

void EffectsPanel::collectKnobs (juce::Array<ParamKnob*>& out)
{
    for (auto& pg : pages) for (auto& k : static_cast<FxPage*> (pg.get())->knobs) out.add (k.get());
    auto* o = static_cast<OutputSection*> (output.get());
    out.addArray ({ &o->input, &o->mix, &o->out });
}

void EffectsPanel::selectFx (int fxId)
{
    selected = juce::jlimit (0, NumFx - 1, fxId);
    for (int f = 0; f < NumFx; ++f)
    {
        pages[(size_t) f]->setVisible (f == selected);
        static_cast<FxChip*> (chips[(size_t) f].get())->selected = (f == selected);
        chips[(size_t) f]->repaint();
    }
    repaint (pageArea.expanded (4));
}

void EffectsPanel::refreshOrder()
{
    const auto o = proc.getFxOrder();
    if (o != order) { order = o; layoutChips(); }
}

int EffectsPanel::indexForX (int x) const
{
    const float w = (float) chainArea.getWidth() / (float) NumFx;
    return juce::jlimit (0, NumFx - 1, (int) ((float) (x - chainArea.getX()) / w));
}

void EffectsPanel::chipDragged (int fxId, int x)
{
    draggingFx = fxId;
    dragX = x;
    layoutChips();
}

void EffectsPanel::chipReleased (int fxId, int x)
{
    auto o = proc.getFxOrder();
    std::vector<int> v (o.begin(), o.end());
    v.erase (std::remove (v.begin(), v.end(), fxId), v.end());
    v.insert (v.begin() + indexForX (x), fxId);
    for (int i = 0; i < NumFx; ++i) o[(size_t) i] = v[(size_t) i];
    proc.setFxOrder (o);
    draggingFx = -1;
    order = o;
    layoutChips();
}

void EffectsPanel::layoutChips()
{
    if (chainArea.isEmpty()) return;
    const int w = chainArea.getWidth() / NumFx;
    std::vector<int> v (order.begin(), order.end());
    if (draggingFx >= 0)
    {
        v.erase (std::remove (v.begin(), v.end(), draggingFx), v.end());
        v.insert (v.begin() + indexForX (dragX), draggingFx);
    }
    for (int i = 0; i < NumFx; ++i)
    {
        const int fx = v[(size_t) i];
        auto b = juce::Rectangle<int> (chainArea.getX() + i * w, chainArea.getY(), w, chainArea.getHeight()).reduced (3, 0);
        if (fx == draggingFx) b = b.withCentre ({ juce::jlimit (chainArea.getX() + w / 2, chainArea.getRight() - w / 2, dragX), b.getCentreY() });
        chips[(size_t) fx]->setBounds (b);
        if (fx == draggingFx) chips[(size_t) fx]->toFront (false);
    }
    repaint();
}

void EffectsPanel::tick()
{
    refreshOrder();
    static_cast<OutputSection*> (output.get())->meter.repaint();
}

void EffectsPanel::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat());
    auto r = getLocalBounds().reduced (12, 8);
    auto top = r.removeFromTop (20);
    drawHeading (g, "Effects  -  signal flow  >", top.removeFromLeft (300));
    drawHeading (g, "Output", juce::Rectangle<int> (outputArea.getX(), top.getY(), 200, 20));

    // flow arrows between chips
    g.setColour (textFaint);
    const int w = chainArea.getWidth() / NumFx;
    for (int i = 1; i < NumFx; ++i)
    {
        const float x = (float) (chainArea.getX() + i * w);
        const float y = (float) chainArea.getCentreY();
        juce::Path a;
        a.addTriangle (x - 2.5f, y - 3.0f, x - 2.5f, y + 3.0f, x + 2.0f, y);
        g.fillPath (a);
    }
    g.setColour (line);
    g.drawVerticalLine (outputArea.getX() - 12, (float) getY() + 12.0f, (float) getHeight() - 12.0f);

    // selected page header
    g.setColour (text);
    g.setFont (font (15.0f, true));
    g.drawText (kFxNames[selected], pageArea.getX(), pageArea.getY() - 2, 200, 20, juce::Justification::centredLeft);
}

void EffectsPanel::resized()
{
    auto r = getLocalBounds().reduced (12, 8);
    r.removeFromTop (22);
    outputArea = r.removeFromRight (330);
    r.removeFromRight (24);
    chainArea = r.removeFromTop (32);
    r.removeFromTop (10);
    pageArea = r;
    auto moveRow = juce::Rectangle<int> (pageArea.getRight() - 64, pageArea.getY(), 64, 20);
    moveRight.setBounds (moveRow.removeFromRight (30));
    moveRow.removeFromRight (4);
    moveLeft.setBounds (moveRow.removeFromRight (30));
    for (auto& pg : pages) pg->setBounds (pageArea.withTrimmedTop (18).withTrimmedLeft (-4));
    output->setBounds (outputArea.withTrimmedTop (4));
    layoutChips();
}

} // namespace nubis
