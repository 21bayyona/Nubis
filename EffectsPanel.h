// =============================================================================
//  NUBIS - EffectsPanel.h : re-orderable FX chain, per-effect controls, output
// =============================================================================
#pragma once

#include "Widgets.h"
#include "../dsp/EffectsEngine.h"

namespace nubis
{
class EffectsPanel : public juce::Component
{
public:
    explicit EffectsPanel (NubisAudioProcessor& p);
    ~EffectsPanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
    void collectKnobs (juce::Array<ParamKnob*>& out);
    void refreshOrder();

    // chip drag callbacks
    void chipDragged (int fxId, int x);
    void chipReleased (int fxId, int x);
    void selectFx (int fxId);

private:
    int indexForX (int x) const;
    void layoutChips();

    NubisAudioProcessor& proc;
    std::vector<std::unique_ptr<juce::Component>> chips;   // indexed by fx id
    std::vector<std::unique_ptr<juce::Component>> pages;   // indexed by fx id
    juce::TextButton moveLeft { "<" }, moveRight { ">" };
    std::unique_ptr<juce::Component> output;
    juce::Rectangle<int> chainArea, pageArea, outputArea;
    std::array<int, NumFx> order {};
    int selected = 0, draggingFx = -1, dragX = 0;
};
} // namespace nubis
