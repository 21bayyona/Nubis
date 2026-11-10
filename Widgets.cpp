// =============================================================================
//  NUBIS - Widgets.cpp
// =============================================================================
#include "Widgets.h"
#include "../PluginProcessor.h"

namespace nubis
{
using namespace theme;

//==============================================================================
ParamKnob::ParamKnob (NubisAudioProcessor& p, const juce::String& id, const juce::String& lbl, bool bipolar)
    : proc (p), paramId (id), label (lbl)
{
    destIndex = findModDestination (id);
    slider.getProperties().set ("bipolar", bipolar);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setVelocityBasedMode (false);
    slider.setMouseDragSensitivity (220);
    slider.setPopupMenuEnabled (false);
    slider.onRightClick = [this] { showMenu(); };
    slider.addMouseListener (this, false);
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, slider);
    if (auto* param = proc.apvts.getParameter (id))
    {
        slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        slider.setTooltip (param->getName (64) + (destIndex >= 0 ? "  -  drop a mod source here, right-click for options" : ""));
    }
    slider.onValueChange = [this] { repaint(); };
}

void ParamKnob::resized()
{
    auto r = getLocalBounds();
    const int labelH = compact ? 13 : 15;
    r.removeFromBottom (labelH);
    const int side = std::min (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (side, side));
}

void ParamKnob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    const auto labelArea = r.removeFromBottom (compact ? 13 : 15);
    const bool active = slider.isMouseOverOrDragging();
    g.setFont (active ? mono (compact ? 10.5f : 11.5f) : font (compact ? 11.0f : 12.0f));
    g.setColour (active ? text : textDim);
    const auto txt = active ? slider.getTextFromValue (slider.getValue()) : label;
    g.drawFittedText (txt, labelArea, juce::Justification::centred, 1, 0.7f);
}

void ParamKnob::paintOverChildren (juce::Graphics& g)
{
    const auto sb = slider.getBounds().toFloat();
    const float size = std::min (sb.getWidth(), sb.getHeight());
    const auto rr = juce::Rectangle<float> (size, size).withCentre (sb.getCentre()).reduced (3.0f);
    const auto c = rr.getCentre();
    const float radius = rr.getWidth() * 0.5f;
    const auto rp = slider.getRotaryParameters();

    if (dragHover)
    {
        g.setColour (modCol.withAlpha (0.25f));
        g.fillEllipse (rr.expanded (2.0f));
        g.setColour (modCol);
        g.drawEllipse (rr.expanded (2.0f), 1.5f);
    }

    if (destIndex >= 0 && proc.destHasMod[(size_t) destIndex].load())
    {
        auto* param = proc.apvts.getParameter (paramId);
        const float base = param != nullptr ? param->getValue() : 0.0f;
        const float modN = proc.modulatedNorm[(size_t) destIndex].load();
        const float a0 = rp.startAngleRadians + base * (rp.endAngleRadians - rp.startAngleRadians);
        const float a1 = rp.startAngleRadians + modN * (rp.endAngleRadians - rp.startAngleRadians);
        const float mr = radius - 7.5f;
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, mr, mr, 0.0f, std::min (a0, a1), std::max (a0, a1), true);
        g.setColour (modCol.withAlpha (0.9f));
        g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const auto dot = c.getPointOnCircumference (radius - 2.0f, a1);
        g.setColour (modCol.withAlpha (0.3f));
        g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (dot));
        g.setColour (modCol);
        g.fillEllipse (juce::Rectangle<float> (4.5f, 4.5f).withCentre (dot));
    }
}

void ParamKnob::refreshModulation()
{
    if (destIndex < 0) return;
    const bool has = proc.destHasMod[(size_t) destIndex].load();
    const float m = proc.modulatedNorm[(size_t) destIndex].load();
    if (has != hadMod || (has && std::abs (m - lastModNorm) > 0.002f))
    {
        hadMod = has; lastModNorm = m;
        repaint();
    }
}

bool ParamKnob::isInterestedInDragSource (const SourceDetails& d)
{
    return destIndex >= 0 && d.description.toString().startsWith ("modsrc:");
}

void ParamKnob::itemDropped (const SourceDetails& d)
{
    dragHover = false;
    const int src = d.description.toString().fromFirstOccurrenceOf ("modsrc:", false, false).getIntValue();
    const bool bip = slider.getProperties().getWithDefault ("bipolar", false);
    if (proc.addModulation (src, paramId, bip ? 0.25f : 0.35f) < 0)
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Modulation matrix full",
                                                "All 16 modulation slots are in use. Remove a routing in the MTX tab first.");
    repaint();
}

void ParamKnob::showMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader (label);
    m.addItem (1, "Reset to default");
    if (destIndex >= 0)
    {
        m.addSeparator();
        int found = 0;
        for (int s = 0; s < kNumModSlots; ++s)
        {
            auto* dst = proc.apvts.getParameter (pid::modDst (s));
            auto* src = proc.apvts.getParameter (pid::modSrc (s));
            auto* amt = proc.apvts.getParameter (pid::modAmt (s));
            if (dst == nullptr || (int) dst->convertFrom0to1 (dst->getValue()) != destIndex + 1) continue;
            const int srcIdx = (int) src->convertFrom0to1 (src->getValue());
            if (srcIdx <= 0) continue;
            ++found;
            juce::PopupMenu sub;
            const float cur = amt->convertFrom0to1 (amt->getValue());
            const float amounts[] = { 1.0f, 0.5f, 0.25f, 0.1f, -0.1f, -0.25f, -0.5f, -1.0f };
            for (int k = 0; k < 8; ++k)
                sub.addItem ((s + 1) * 100 + k, (amounts[k] > 0 ? "+" : "") + juce::String (juce::roundToInt (amounts[k] * 100)) + " %", true, std::abs (cur - amounts[k]) < 0.005f);
            sub.addSeparator();
            sub.addItem ((s + 1) * 100 + 50, "Remove");
            m.addSubMenu (juce::String (kModSourceNames[srcIdx]) + "   " + (cur > 0 ? "+" : "") + juce::String (juce::roundToInt (cur * 100)) + " %", sub);
        }
        juce::PopupMenu assign;
        for (int i = 0; i < kNumMacros; ++i) assign.addItem (10 + i, "Macro " + juce::String (i + 1));
        for (int i = 0; i < kNumLfos; ++i)   assign.addItem (20 + i, "LFO " + juce::String (i + 1));
        assign.addItem (30, "ENV 1"); assign.addItem (31, "ENV 2"); assign.addItem (32, "Random"); assign.addItem (33, "Step Seq");
        m.addSubMenu ("Modulate with...", assign);
        if (found > 0) m.addItem (2, "Remove all modulation");
    }

    juce::Component::SafePointer<ParamKnob> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&slider), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& self = *safe;
        auto& pr = self.proc;
        if (r == 1) { if (auto* p = pr.apvts.getParameter (self.paramId)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture(); } }
        else if (r == 2) pr.clearModulationsFor (self.paramId);
        else if (r >= 10 && r < 14) pr.addModulation (SrcMacro1 + (r - 10), self.paramId, 0.5f);
        else if (r >= 20 && r < 24) pr.addModulation (SrcLfo1 + (r - 20), self.paramId, 0.25f);
        else if (r == 30) pr.addModulation (SrcEnv1, self.paramId, 0.4f);
        else if (r == 31) pr.addModulation (SrcEnv2, self.paramId, 0.4f);
        else if (r == 32) pr.addModulation (SrcRandom, self.paramId, 0.25f);
        else if (r == 33) pr.addModulation (SrcSeq, self.paramId, 0.4f);
        else if (r >= 100)
        {
            const int slot = r / 100 - 1, k = r % 100;
            if (k == 50) pr.clearSlot (slot);
            else if (k < 8)
            {
                const float amounts[] = { 1.0f, 0.5f, 0.25f, 0.1f, -0.1f, -0.25f, -0.5f, -1.0f };
                if (auto* p = pr.apvts.getParameter (pid::modAmt (slot))) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (amounts[k])); p->endChangeGesture(); }
            }
        }
        self.repaint();
    });
}

//==============================================================================
ParamToggle::ParamToggle (NubisAudioProcessor& p, const juce::String& id, const juce::String& txt, bool led)
    : juce::TextButton (txt)
{
    setClickingTogglesState (true);
    getProperties().set ("led", led);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, id, *this);
    if (auto* param = p.apvts.getParameter (id)) setTooltip (param->getName (64));
}

//==============================================================================
ParamCombo::ParamCombo (NubisAudioProcessor& p, const juce::String& id)
{
    if (auto* cp = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (id)))
    {
        addItemList (cp->choices, 1);
        setTooltip (cp->getName (64));
    }
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, id, *this);
}

//==============================================================================
ModSourceChip::ModSourceChip (NubisAudioProcessor& p, int src, const juce::String& t)
    : proc (p), source (src), label (t)
{
    setTooltip ("Drag " + t + " onto any knob to modulate it");
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void ModSourceChip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool over = isMouseOver();
    g.setColour (selected ? panelHi.brighter (0.08f) : well);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (selected ? accentHi : (over ? modCol.withAlpha (0.8f) : line));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    // live value
    const float v = proc.sourceValues[(size_t) source].load();
    const bool bip = (source >= SrcLfo1 && source <= SrcLfo4) || source == SrcRandom;
    auto bar = r.reduced (4.0f, 0.0f).removeFromBottom (3.0f).translated (0.0f, -3.0f);
    g.setColour (line);
    g.fillRoundedRectangle (bar, 1.5f);
    g.setColour (modCol.withAlpha (0.9f));
    if (bip)
    {
        const float cx = bar.getCentreX(), w = bar.getWidth() * 0.5f * juce::jlimit (-1.0f, 1.0f, v);
        g.fillRoundedRectangle (juce::Rectangle<float> (std::min (cx, cx + w), bar.getY(), std::abs (w), bar.getHeight()), 1.5f);
    }
    else g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, v)), 1.5f);

    g.setColour (selected || over ? text : textDim);
    g.setFont (font (11.0f, true).withExtraKerningFactor (0.05f));
    g.drawFittedText (label, getLocalBounds().withTrimmedBottom (5), juce::Justification::centred, 1, 0.7f);
}

void ModSourceChip::mouseDown (const juce::MouseEvent&) { dragging = false; }

void ModSourceChip::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging && e.getDistanceFromDragStart() > 4)
    {
        dragging = true;
        if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
            c->startDragging ("modsrc:" + juce::String (source), this);
    }
}

} // namespace nubis
