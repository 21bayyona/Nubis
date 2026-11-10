// =============================================================================
//  NUBIS - ModulationPanel.h : macros, mod sources (drag & drop), LFO / ENV /
//  RANDOM / SEQ editors and the modulation matrix.
// =============================================================================
#pragma once

#include "Widgets.h"

namespace nubis
{
class ModulationPanel : public juce::Component
{
public:
    explicit ModulationPanel (NubisAudioProcessor& p);
    ~ModulationPanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
    void collectKnobs (juce::Array<ParamKnob*>& out);

private:
    void selectPage (int index);

    NubisAudioProcessor& proc;
    std::vector<std::unique_ptr<ParamKnob>> macroKnobs;
    std::vector<std::unique_ptr<ModSourceChip>> macroChips;
    std::vector<std::unique_ptr<ModSourceChip>> sourceChips;   // LFO1-4, ENV1-2, RND, SEQ
    juce::TextButton matrixButton { "MATRIX" };
    std::vector<std::unique_ptr<juce::Component>> pages;      // 0-3 LFO, 4-5 ENV, 6 RND, 7 SEQ, 8 MATRIX
    int currentPage = 0;
};
} // namespace nubis
