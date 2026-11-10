// =============================================================================
//  NUBIS - ModulationPanel.cpp
// =============================================================================
#include "ModulationPanel.h"
#include "../PluginProcessor.h"

namespace nubis
{
using namespace theme;

namespace
{
    //==========================================================================
    /** Rolling trace of a modulation source. */
    class Scope : public juce::Component
    {
    public:
        Scope (NubisAudioProcessor& p, int src, bool bipolar) : proc (p), source (src), bip (bipolar) { setInterceptsMouseClicks (false, false); }
        void push()
        {
            history[(size_t) pos] = proc.sourceValues[(size_t) source].load();
            pos = (pos + 1) % (int) history.size();
            repaint();
        }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (well);
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (line);
            g.drawHorizontalLine ((int) r.getCentreY(), r.getX() + 4, r.getRight() - 4);
            r = r.reduced (6.0f, 8.0f);
            juce::Path p;
            const int n = (int) history.size();
            for (int i = 0; i < n; ++i)
            {
                const float v = history[(size_t) ((pos + i) % n)];
                const float y = bip ? juce::jmap (v, -1.0f, 1.0f, r.getBottom(), r.getY()) : juce::jmap (v, 0.0f, 1.0f, r.getBottom(), r.getY());
                const float x = r.getX() + r.getWidth() * (float) i / (float) (n - 1);
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (modCol.withAlpha (0.25f));
            g.strokePath (p, juce::PathStrokeType (5.0f));
            g.setColour (modCol);
            g.strokePath (p, juce::PathStrokeType (1.6f));
        }
    private:
        NubisAudioProcessor& proc;
        int source; bool bip;
        std::array<float, 160> history {};
        int pos = 0;
    };

    //==========================================================================
    class Page : public juce::Component
    {
    public:
        virtual void tick() {}
        juce::Array<ParamKnob*> knobs;
    };

    class LfoPage : public Page
    {
    public:
        LfoPage (NubisAudioProcessor& p, int i)
            : rate (p, pid::lfo (i, "rate"), "Rate"), sync (p, pid::lfo (i, "sync"), "SYNC"),
              div (p, pid::lfo (i, "div")), shape (p, pid::lfo (i, "shape")), scope (p, SrcLfo1 + i, true)
        {
            for (auto* c : std::initializer_list<juce::Component*> { &rate, &sync, &div, &shape, &scope, &divCap, &shapeCap }) addAndMakeVisible (c);
            knobs.add (&rate);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (4);
            scope.setBounds (r.removeFromTop (r.getHeight() - 120));
            r.removeFromTop (10);
            rate.setBounds (r.removeFromLeft (84));
            r.removeFromLeft (8);
            auto col = r;
            shapeCap.setBounds (col.removeFromTop (14));
            shape.setBounds (col.removeFromTop (26));
            col.removeFromTop (8);
            divCap.setBounds (col.removeFromTop (14));
            auto row = col.removeFromTop (26);
            sync.setBounds (row.removeFromRight (70));
            row.removeFromRight (6);
            div.setBounds (row);
        }
        void tick() override { scope.push(); rate.setAlpha (sync.getToggleState() ? 0.4f : 1.0f); }
    private:
        ParamKnob rate; ParamToggle sync; ParamCombo div, shape; Scope scope;
        Caption divCap { "Tempo division" }, shapeCap { "Waveform" };
    };

    class EnvPage : public Page
    {
    public:
        EnvPage (NubisAudioProcessor& p, int i)
            : proc (p), index (i), a (p, pid::env (i, "a"), "Attack"), d (p, pid::env (i, "d"), "Decay"),
              s (p, pid::env (i, "s"), "Sustain"), r (p, pid::env (i, "r"), "Release"), trig (p, pid::env (i, "trig"))
        {
            for (auto* c : std::initializer_list<juce::Component*> { &a, &d, &s, &r, &trig, &trigCap }) addAndMakeVisible (c);
            knobs.addArray ({ &a, &d, &s, &r });
        }
        void resized() override
        {
            auto b = getLocalBounds().reduced (4);
            curve = b.removeFromTop (b.getHeight() - 132).toFloat();
            b.removeFromTop (8);
            auto row = b.removeFromTop (78);
            const int w = row.getWidth() / 4;
            for (auto* k : { &a, &d, &s, &r }) k->setBounds (row.removeFromLeft (w));
            b.removeFromTop (6);
            trigCap.setBounds (b.removeFromLeft (90));
            trig.setBounds (b.removeFromLeft (120).withHeight (24));
        }
        void paint (juce::Graphics& g) override
        {
            g.setColour (well);
            g.fillRoundedRectangle (curve, 6.0f);
            auto rr = curve.reduced (8.0f, 10.0f);
            auto get = [this] (const juce::String& id) { return proc.apvts.getRawParameterValue (id)->load(); };
            const float at = get (pid::env (index, "a")), dt = get (pid::env (index, "d")), sv = get (pid::env (index, "s")), rt = get (pid::env (index, "r"));
            const float total = at + dt + 0.6f + rt;
            auto X = [&] (float t) { return rr.getX() + rr.getWidth() * std::sqrt (t / total); };
            juce::Path p;
            p.startNewSubPath (rr.getX(), rr.getBottom());
            p.lineTo (X (at), rr.getY());
            const float sy = rr.getBottom() - sv * rr.getHeight();
            p.quadraticTo (X (at + dt * 0.3f), sy, X (at + dt), sy);
            p.lineTo (X (at + dt + 0.6f), sy);
            p.quadraticTo (X (at + dt + 0.6f + rt * 0.3f), rr.getBottom(), X (total), rr.getBottom());
            juce::Path fill (p);
            fill.lineTo (rr.getX(), rr.getBottom());
            fill.closeSubPath();
            g.setColour (modCol.withAlpha (0.1f));
            g.fillPath (fill);
            g.setColour (modCol);
            g.strokePath (p, juce::PathStrokeType (1.6f));
            const float lvl = proc.sourceValues[(size_t) (SrcEnv1 + index)].load();
            g.setColour (text);
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ rr.getRight() - 6.0f, rr.getBottom() - lvl * rr.getHeight() }));
        }
        void tick() override { repaint (curve.toNearestInt()); }
    private:
        NubisAudioProcessor& proc; int index;
        ParamKnob a, d, s, r; ParamCombo trig;
        Caption trigCap { "Trigger" };
        juce::Rectangle<float> curve;
    };

    class RndPage : public Page
    {
    public:
        explicit RndPage (NubisAudioProcessor& p)
            : rate (p, pid::rndRate, "Rate"), smooth (p, pid::rndSmooth, "Smooth"), scope (p, SrcRandom, true)
        {
            for (auto* c : std::initializer_list<juce::Component*> { &rate, &smooth, &scope }) addAndMakeVisible (c);
            knobs.addArray ({ &rate, &smooth });
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (4);
            scope.setBounds (r.removeFromTop (r.getHeight() - 100));
            r.removeFromTop (10);
            rate.setBounds (r.removeFromLeft (r.getWidth() / 2));
            smooth.setBounds (r);
        }
        void tick() override { scope.push(); }
    private:
        ParamKnob rate, smooth; Scope scope;
    };

    //==========================================================================
    class StepEditor : public juce::Component
    {
    public:
        explicit StepEditor (NubisAudioProcessor& p) : proc (p) {}
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (well);
            g.fillRoundedRectangle (r, 6.0f);
            r = r.reduced (6.0f);
            const int len = (int) proc.apvts.getRawParameterValue (pid::seqLen)->load();
            const int cur = proc.currentSeqStep.load();
            const float w = r.getWidth() / (float) kNumSteps;
            for (int i = 0; i < kNumSteps; ++i)
            {
                const float v = proc.apvts.getRawParameterValue (pid::step (i))->load();
                auto cell = juce::Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight()).reduced (1.5f, 0.0f);
                g.setColour (i < len ? line : line.withAlpha (0.3f));
                g.fillRoundedRectangle (cell, 2.0f);
                auto bar = cell.withTop (cell.getBottom() - v * cell.getHeight());
                g.setColour ((i == cur ? accentHi : modCol).withAlpha (i < len ? 1.0f : 0.25f));
                g.fillRoundedRectangle (bar, 2.0f);
                if (i % 4 == 0) { g.setColour (textFaint); g.fillRect (cell.getX(), r.getBottom() + 2.0f, cell.getWidth(), 1.0f); }
            }
        }
        void mouseDown (const juce::MouseEvent& e) override { lastStep = -1; edit (e); }
        void mouseDrag (const juce::MouseEvent& e) override { edit (e); }
        void mouseUp (const juce::MouseEvent&) override { if (lastStep >= 0) if (auto* p = proc.apvts.getParameter (pid::step (lastStep))) p->endChangeGesture(); lastStep = -1; }
        void tick() { if (proc.currentSeqStep.load() != shownStep) { shownStep = proc.currentSeqStep.load(); repaint(); } }
    private:
        void edit (const juce::MouseEvent& e)
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            const int i = juce::jlimit (0, kNumSteps - 1, (int) ((e.position.x - r.getX()) / r.getWidth() * kNumSteps));
            const float v = juce::jlimit (0.0f, 1.0f, (r.getBottom() - e.position.y) / r.getHeight());
            auto* p = proc.apvts.getParameter (pid::step (i));
            if (p == nullptr) return;
            if (i != lastStep)
            {
                if (lastStep >= 0) if (auto* lp = proc.apvts.getParameter (pid::step (lastStep))) lp->endChangeGesture();
                p->beginChangeGesture();
                lastStep = i;
            }
            p->setValueNotifyingHost (v);
            repaint();
        }
        NubisAudioProcessor& proc;
        int lastStep = -1, shownStep = -1;
    };

    class SeqPage : public Page
    {
    public:
        explicit SeqPage (NubisAudioProcessor& p)
            : steps (p), length (p, pid::seqLen, "Length"), glide (p, pid::seqGlide, "Glide"), div (p, pid::seqDiv)
        {
            for (auto* c : std::initializer_list<juce::Component*> { &steps, &length, &glide, &div, &divCap }) addAndMakeVisible (c);
            knobs.addArray ({ &length, &glide });
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (4);
            steps.setBounds (r.removeFromTop (r.getHeight() - 100));
            r.removeFromTop (12);
            length.setBounds (r.removeFromLeft (76));
            glide.setBounds (r.removeFromLeft (76));
            r.removeFromLeft (8);
            divCap.setBounds (r.removeFromTop (16));
            div.setBounds (r.removeFromTop (26));
        }
        void tick() override { steps.tick(); }
    private:
        StepEditor steps; ParamKnob length, glide; ParamCombo div;
        Caption divCap { "Step rate" };
    };

    //==========================================================================
    class MatrixRow : public juce::Component
    {
    public:
        MatrixRow (NubisAudioProcessor& p, int s) : proc (p), slot (s), src (p, pid::modSrc (s)), dst (p, pid::modDst (s))
        {
            amount.setSliderStyle (juce::Slider::LinearHorizontal);
            amount.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            amount.getProperties().set ("bipolar", true);
            amount.setDoubleClickReturnValue (true, 0.0);
            att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, pid::modAmt (s), amount);
            clear.setButtonText ("x");
            clear.setTooltip ("Clear this routing");
            clear.onClick = [this] { proc.clearSlot (slot); };
            for (auto* c : std::initializer_list<juce::Component*> { &src, &dst, &amount, &clear }) addAndMakeVisible (c);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (0, 2);
            r.removeFromLeft (18);
            src.setBounds (r.removeFromLeft (64));
            r.removeFromLeft (3);
            dst.setBounds (r.removeFromLeft (88));
            r.removeFromLeft (4);
            clear.setBounds (r.removeFromRight (20));
            amount.setBounds (r);
        }
        void paint (juce::Graphics& g) override
        {
            g.setColour (textFaint);
            g.setFont (mono (10.0f));
            g.drawText (juce::String (slot + 1), getLocalBounds().removeFromLeft (16), juce::Justification::centred);
        }
    private:
        NubisAudioProcessor& proc; int slot;
        ParamCombo src, dst;
        juce::Slider amount;
        juce::TextButton clear;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };

    class MatrixPage : public Page
    {
    public:
        explicit MatrixPage (NubisAudioProcessor& p)
        {
            for (int s = 0; s < kNumModSlots; ++s)
            {
                rows.push_back (std::make_unique<MatrixRow> (p, s));
                content.addAndMakeVisible (*rows.back());
            }
            view.setViewedComponent (&content, false);
            view.setScrollBarsShown (true, false);
            view.setScrollBarThickness (8);
            addAndMakeVisible (view);
            addAndMakeVisible (hint);
        }
        void resized() override
        {
            auto r = getLocalBounds();
            hint.setBounds (r.removeFromTop (18));
            view.setBounds (r);
            const int rowH = 26;
            content.setSize (r.getWidth() - 10, rowH * kNumModSlots);
            for (int s = 0; s < kNumModSlots; ++s) rows[(size_t) s]->setBounds (0, s * rowH, content.getWidth(), rowH);
        }
    private:
        juce::Viewport view;
        juce::Component content;
        std::vector<std::unique_ptr<MatrixRow>> rows;
        Caption hint { "Source  >  destination  -  amount" };
    };
} // namespace

//==============================================================================
ModulationPanel::ModulationPanel (NubisAudioProcessor& p) : proc (p)
{
    for (int i = 0; i < kNumMacros; ++i)
    {
        macroKnobs.push_back (std::make_unique<ParamKnob> (p, pid::macro (i), "Macro " + juce::String (i + 1)));
        addAndMakeVisible (*macroKnobs.back());
        macroChips.push_back (std::make_unique<ModSourceChip> (p, SrcMacro1 + i, "M" + juce::String (i + 1)));
        addAndMakeVisible (*macroChips.back());
    }
    const char* names[] = { "LFO 1", "LFO 2", "LFO 3", "LFO 4", "ENV 1", "ENV 2", "RANDOM", "SEQ" };
    for (int i = 0; i < 8; ++i)
    {
        sourceChips.push_back (std::make_unique<ModSourceChip> (p, SrcLfo1 + i, names[i]));
        sourceChips.back()->onClick = [this, i] { selectPage (i); };
        addAndMakeVisible (*sourceChips.back());
    }
    matrixButton.getProperties().set ("tab", true);
    matrixButton.setClickingTogglesState (false);
    matrixButton.onClick = [this] { selectPage (8); };
    addAndMakeVisible (matrixButton);

    for (int i = 0; i < kNumLfos; ++i) pages.push_back (std::make_unique<LfoPage> (p, i));
    for (int i = 0; i < kNumEnvs; ++i) pages.push_back (std::make_unique<EnvPage> (p, i));
    pages.push_back (std::make_unique<RndPage> (p));
    pages.push_back (std::make_unique<SeqPage> (p));
    pages.push_back (std::make_unique<MatrixPage> (p));
    for (auto& pg : pages) addChildComponent (*pg);
    selectPage (0);
}

ModulationPanel::~ModulationPanel() = default;

void ModulationPanel::selectPage (int index)
{
    currentPage = juce::jlimit (0, (int) pages.size() - 1, index);
    for (size_t i = 0; i < pages.size(); ++i) pages[i]->setVisible ((int) i == currentPage);
    for (size_t i = 0; i < sourceChips.size(); ++i) { sourceChips[i]->selected = (int) i == currentPage; sourceChips[i]->repaint(); }
    matrixButton.setToggleState (currentPage == 8, juce::dontSendNotification);
}

void ModulationPanel::collectKnobs (juce::Array<ParamKnob*>& out)
{
    for (auto& k : macroKnobs) out.add (k.get());
    for (auto& pg : pages) out.addArray (static_cast<Page*> (pg.get())->knobs);
}

void ModulationPanel::tick()
{
    for (auto& c : macroChips) c->refresh();
    for (auto& c : sourceChips) c->refresh();
    if (auto* pg = dynamic_cast<Page*> (pages[(size_t) currentPage].get())) pg->tick();
}

void ModulationPanel::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat());
    auto r = getLocalBounds().reduced (12, 8);
    drawHeading (g, "Macros", r.removeFromTop (20));
    r.removeFromTop (98);
    drawHeading (g, "Modulation", r.removeFromTop (22));
    g.setColour (textFaint);
    g.setFont (font (10.5f));
    g.drawText ("drag a source onto any knob", r.withHeight (0).withTop (r.getY() - 22).withHeight (22).withTrimmedRight (74), juce::Justification::centredRight);
}

void ModulationPanel::resized()
{
    auto r = getLocalBounds().reduced (12, 8);
    r.removeFromTop (20);
    auto macroRow = r.removeFromTop (98);
    const int mw = macroRow.getWidth() / kNumMacros;
    for (int i = 0; i < kNumMacros; ++i)
    {
        auto cell = macroRow.removeFromLeft (mw);
        macroChips[(size_t) i]->setBounds (cell.removeFromBottom (20).reduced (12, 0));
        cell.removeFromBottom (2);
        macroKnobs[(size_t) i]->setBounds (cell);
    }
    auto head = r.removeFromTop (22);
    matrixButton.setBounds (head.removeFromRight (70).reduced (0, 1));
    r.removeFromTop (4);
    for (int row = 0; row < 2; ++row)
    {
        auto line = r.removeFromTop (26);
        const int cw = line.getWidth() / 4;
        for (int i = 0; i < 4; ++i) sourceChips[(size_t) (row * 4 + i)]->setBounds (line.removeFromLeft (cw).reduced (2, 1));
        r.removeFromTop (2);
    }
    r.removeFromTop (8);
    for (auto& pg : pages) pg->setBounds (r);
}

} // namespace nubis
