// =============================================================================
//  NUBIS - Widgets.h : parameter-bound controls used across the editor
// =============================================================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

class NubisAudioProcessor;

namespace nubis
{
//==============================================================================
/** Rotary slider that hands right-clicks to its owner. */
class KnobSlider : public juce::Slider
{
public:
    KnobSlider() : juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox) {}
    std::function<void()> onRightClick;
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
        juce::Slider::mouseDown (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) juce::Slider::mouseDrag (e); }
    void mouseUp (const juce::MouseEvent& e) override   { if (! e.mods.isPopupMenu()) juce::Slider::mouseUp (e); }
};

//==============================================================================
/** Knob + label + value read-out + modulation ring + drop target for mod sources. */
class ParamKnob : public juce::Component, public juce::DragAndDropTarget
{
public:
    ParamKnob (NubisAudioProcessor& p, const juce::String& paramId, const juce::String& label, bool bipolar = false);

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dragHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override  { dragHover = false; repaint(); }
    void itemDropped (const SourceDetails&) override;

    /** Called by the editor's timer. Repaints only when modulated. */
    void refreshModulation();
    void setCompact (bool c) { compact = c; resized(); }
    const juce::String& getParamId() const { return paramId; }

    KnobSlider slider;

private:
    void showMenu();

    NubisAudioProcessor& proc;
    juce::String paramId, label;
    int destIndex = -1;
    bool dragHover = false, compact = false, hadMod = false;
    float lastModNorm = -1.0f;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
class ParamToggle : public juce::TextButton
{
public:
    ParamToggle (NubisAudioProcessor& p, const juce::String& paramId, const juce::String& text, bool led = true);
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

//==============================================================================
class ParamCombo : public juce::ComboBox
{
public:
    ParamCombo (NubisAudioProcessor& p, const juce::String& paramId);
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

//==============================================================================
/** Draggable modulation source. Drop it on any knob to create a routing. */
class ModSourceChip : public juce::Component, public juce::SettableTooltipClient
{
public:
    ModSourceChip (NubisAudioProcessor& p, int sourceIndex, const juce::String& text);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { if (! dragging && onClick) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }
    void refresh() { repaint(); }
    std::function<void()> onClick;
    bool selected = false;
private:
    NubisAudioProcessor& proc;
    int source;
    juce::String label;
    bool dragging = false;
};

//==============================================================================
/** Small static label in the house style. */
class Caption : public juce::Component
{
public:
    explicit Caption (juce::String t, juce::Justification j = juce::Justification::centredLeft) : text (std::move (t)), just (j) { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics& g) override
    {
        g.setColour (theme::textDim);
        g.setFont (theme::font (11.0f, true).withExtraKerningFactor (0.12f));
        g.drawText (text.toUpperCase(), getLocalBounds(), just, true);
    }
    juce::String text;
    juce::Justification just;
};

} // namespace nubis
