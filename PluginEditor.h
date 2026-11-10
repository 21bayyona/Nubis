// =============================================================================
//  NUBIS - PluginEditor.h
// =============================================================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/NubisLookAndFeel.h"
#include "ui/Widgets.h"
#include "ui/GrainVisualizer.h"
#include "ui/ModulationPanel.h"
#include "ui/EffectsPanel.h"
#include "dsp/Grain.h"

namespace nubis
{
/** Fixed-size (1280 x 800) design surface. The editor scales it with an affine transform. */
class MainSurface : public juce::Component
{
public:
    explicit MainSurface (NubisAudioProcessor& p);
    ~MainSurface() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick (float dt);
    void refreshPresetLabel();

    static constexpr int kWidth = 1280, kHeight = 800;

private:
    void showPresetMenu();
    void showSettingsMenu();
    void savePreset();
    void loadSampleDialog();
    void updateSourceControls();

    NubisAudioProcessor& proc;

    // top bar
    juce::TextButton presetButton, prevButton { "<" }, nextButton { ">" }, saveButton { "SAVE" }, loadButton { "LOAD" },
                     randomButton { "RANDOMIZE" }, settingsButton { "..." };

    // source row
    ParamCombo sourceCombo, internalCombo, playModeCombo;
    juce::TextButton loadSampleButton { "LOAD SAMPLE" };
    juce::String sampleName;

    // grain engine
    std::vector<std::unique_ptr<ParamKnob>> grainKnobs;
    ParamCombo envelopeCombo;
    ParamToggle freezeButton;
    EnvelopeTables envTables;
    juce::Rectangle<int> envPreviewArea;

    // centre
    GrainVisualizer visualizer;
    Spectrogram spectrogram;
    std::vector<std::unique_ptr<ParamKnob>> textureKnobs;
    std::vector<std::unique_ptr<ParamKnob>> specialKnobs;
    ParamToggle infFreezeButton;
    ParamCombo spectralCombo;

    ModulationPanel modPanel;
    EffectsPanel fxPanel;

    juce::Array<ParamKnob*> allKnobs;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> leftPanel, centrePanel, rightPanel, bottomPanel, topBar;
    juce::Rectangle<int> textureHead, specialHead, grainHead;
    int lastEnvelope = -1;
};
} // namespace nubis

//==============================================================================
class NubisAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  public juce::DragAndDropContainer,
                                  public juce::FileDragAndDropTarget,
                                  private juce::Timer,
                                  private juce::AsyncUpdater
{
public:
    explicit NubisAudioProcessorEditor (NubisAudioProcessor&);
    ~NubisAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    void timerCallback() override;
    void handleAsyncUpdate() override;

    NubisAudioProcessor& proc;
    nubis::NubisLookAndFeel lnf;
    nubis::MainSurface surface;
    juce::TooltipWindow tooltips { this, 600 };
    double lastTick = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NubisAudioProcessorEditor)
};
