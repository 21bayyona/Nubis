// =============================================================================
//  NUBIS - PluginEditor.cpp
// =============================================================================
#include "PluginEditor.h"

using namespace nubis;
using namespace nubis::theme;

namespace
{
    const char* const kGrainIds[12]    = { pid::size, pid::density, pid::position, pid::posRandom, pid::pitch, pid::pitchRandom,
                                           pid::jitter, pid::overlap, pid::spread, pid::pan, pid::reverse, pid::direction };
    const char* const kGrainLabels[12] = { "Size", "Density", "Position", "Pos Random", "Pitch", "Pitch Random",
                                           "Jitter", "Overlap", "Spread", "Pan", "Reverse", "Direction" };
    const bool kGrainBipolar[12]       = { false, false, false, false, true, false, false, false, false, true, false, true };

    const char* const kTexIds[8]    = { pid::texture, pid::smoothness, pid::blur, pid::randomness, pid::diffusion, pid::thickness, pid::width, pid::feedback };
    const char* const kTexLabels[8] = { "Texture", "Smoothness", "Spectral Blur", "Randomness", "Diffusion", "Density", "Width", "Feedback" };

    const char* const kSpecIds[6]    = { pid::spectralAmount, pid::chaos, pid::evolve, pid::evolveRate, pid::reverseCloud, pid::drone };
    const char* const kSpecLabels[6] = { "Spectral Amt", "Chaos", "Evolve", "Evolve Rate", "Reverse Cloud", "Drone" };
}

//==============================================================================
MainSurface::MainSurface (NubisAudioProcessor& p)
    : proc (p),
      sourceCombo (p, pid::source), internalCombo (p, pid::internalType), playModeCombo (p, pid::playMode),
      envelopeCombo (p, pid::envelope), freezeButton (p, pid::freeze, "FREEZE"),
      visualizer (p), spectrogram (p),
      infFreezeButton (p, pid::infFreeze, "INFINITE FREEZE"), spectralCombo (p, pid::spectralMode),
      modPanel (p), fxPanel (p)
{
    // ---- top bar ---------------------------------------------------------------
    presetButton.setTooltip ("Browse presets");
    presetButton.onClick = [this] { showPresetMenu(); };
    prevButton.onClick = [this] { proc.presets.loadPrevious(); };
    nextButton.onClick = [this] { proc.presets.loadNext(); };
    saveButton.onClick = [this] { savePreset(); };
    saveButton.setTooltip ("Save the current sound as a user preset");
    loadButton.setTooltip ("Import a .nubispreset file");
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Import preset", PresetManager::getUserPresetFolder(), juce::String ("*") + PresetManager::kExtension);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) proc.presets.importPresetFile (fc.getResult()); });
    };
    randomButton.setTooltip ("Musical random patch.  Shift-click: experimental random");
    randomButton.onClick = [this] { proc.randomize (juce::ModifierKeys::currentModifiers.isShiftDown()); };
    settingsButton.setTooltip ("Settings");
    settingsButton.onClick = [this] { showSettingsMenu(); };
    for (auto* b : { &presetButton, &prevButton, &nextButton, &saveButton, &loadButton, &randomButton, &settingsButton }) addAndMakeVisible (b);

    // ---- source row -------------------------------------------------------------
    loadSampleButton.onClick = [this] { loadSampleDialog(); };
    loadSampleButton.setTooltip ("Load an audio file (wav, aiff, flac, ogg, mp3) - or drag & drop onto the display");
    for (auto* c : std::initializer_list<juce::Component*> { &sourceCombo, &internalCombo, &playModeCombo, &loadSampleButton }) addAndMakeVisible (c);

    // ---- grain engine -------------------------------------------------------------
    for (int i = 0; i < 12; ++i)
    {
        grainKnobs.push_back (std::make_unique<ParamKnob> (p, kGrainIds[i], kGrainLabels[i], kGrainBipolar[i]));
        addAndMakeVisible (*grainKnobs.back());
        allKnobs.add (grainKnobs.back().get());
    }
    freezeButton.setTooltip ("Grain Freeze: Live = stop capturing (hold the buffer).  Sample / Internal = hold the scan position");
    addAndMakeVisible (envelopeCombo);
    addAndMakeVisible (freezeButton);

    // ---- centre --------------------------------------------------------------------
    addAndMakeVisible (visualizer);
    addAndMakeVisible (spectrogram);
    for (int i = 0; i < 8; ++i)
    {
        textureKnobs.push_back (std::make_unique<ParamKnob> (p, kTexIds[i], kTexLabels[i]));
        addAndMakeVisible (*textureKnobs.back());
        allKnobs.add (textureKnobs.back().get());
    }
    for (int i = 0; i < 6; ++i)
    {
        specialKnobs.push_back (std::make_unique<ParamKnob> (p, kSpecIds[i], kSpecLabels[i]));
        specialKnobs.back()->setCompact (true);
        addAndMakeVisible (*specialKnobs.back());
        allKnobs.add (specialKnobs.back().get());
    }
    infFreezeButton.setTooltip ("Infinite Freeze: captures the live buffer permanently. The frozen audio is saved with your project.");
    spectralCombo.setTooltip ("Spectral Play: quantise grain pitches to a scale, or spread them over the harmonic series");
    addAndMakeVisible (infFreezeButton);
    addAndMakeVisible (spectralCombo);

    addAndMakeVisible (modPanel);
    addAndMakeVisible (fxPanel);
    modPanel.collectKnobs (allKnobs);
    fxPanel.collectKnobs (allKnobs);

    refreshPresetLabel();
    updateSourceControls();
    setSize (kWidth, kHeight);
}

MainSurface::~MainSurface() = default;

//==============================================================================
void MainSurface::refreshPresetLabel()
{
    presetButton.setButtonText (proc.presets.getCurrentName());
    fxPanel.refreshOrder();
}

void MainSurface::updateSourceControls()
{
    const int src = (int) proc.apvts.getRawParameterValue (pid::source)->load();
    internalCombo.setVisible (src == (int) SourceMode::Internal);
    loadSampleButton.setVisible (src == (int) SourceMode::Sample);
    const auto name = proc.hasSample() ? proc.getSampleName() : juce::String();
    if (name != sampleName) { sampleName = name; repaint (centrePanel.withHeight (40)); }
}

void MainSurface::tick (float dt)
{
    visualizer.tick (dt);
    spectrogram.tick();
    modPanel.tick();
    fxPanel.tick();
    for (auto* k : allKnobs) k->refreshModulation();
    updateSourceControls();
    const int env = (int) proc.apvts.getRawParameterValue (pid::envelope)->load();
    if (env != lastEnvelope) { lastEnvelope = env; repaint (envPreviewArea); }
}

//==============================================================================
void MainSurface::showPresetMenu()
{
    juce::PopupMenu menu;
    menu.addItem (100000, "Init");
    menu.addSeparator();
    const auto& list = proc.presets.getPresets();
    for (auto* cat : kPresetCategories)
    {
        juce::PopupMenu sub;
        for (int i = 0; i < list.size(); ++i)
            if (list[i].category == cat) sub.addItem (i + 1, list[i].name, true, i == proc.presets.getCurrentIndex());
        if (sub.getNumItems() > 0) menu.addSubMenu (cat, sub);
    }
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetButton).withMinimumWidth (presetButton.getWidth()),
                        [this] (int r)
                        {
                            if (r == 100000) proc.presets.initialise();
                            else if (r > 0) proc.presets.loadPreset (r - 1);
                        });
}

void MainSurface::showSettingsMenu()
{
    juce::PopupMenu m;
    juce::PopupMenu size;
    const float scales[] = { 0.75f, 0.875f, 1.0f, 1.25f, 1.5f };
    for (int i = 0; i < 5; ++i) size.addItem (10 + i, juce::String (juce::roundToInt (scales[i] * 100)) + " %");
    m.addSubMenu ("Interface size", size);
    m.addSeparator();
    m.addItem (1, "Init patch");
    m.addItem (2, "Randomize (musical)");
    m.addItem (3, "Randomize (experimental)");
    m.addSeparator();
    m.addItem (4, "Open user preset folder");
    m.addItem (5, "Rescan presets");
    if (proc.hasSample()) m.addItem (6, "Unload sample");
    m.addSeparator();
    m.addItem (7, "About NUBIS");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settingsButton), [this, scales] (int r)
    {
        if (r >= 10 && r < 15)
        {
            if (auto* ed = findParentComponentOfClass<NubisAudioProcessorEditor>())
                ed->setSize (juce::roundToInt (kWidth * scales[r - 10]), juce::roundToInt (kHeight * scales[r - 10]));
        }
        else if (r == 1) proc.presets.initialise();
        else if (r == 2) proc.randomize (false);
        else if (r == 3) proc.randomize (true);
        else if (r == 4) PresetManager::getUserPresetFolder().startAsProcess();
        else if (r == 5) { proc.presets.refresh(); refreshPresetLabel(); }
        else if (r == 6) proc.clearSample();
        else if (r == 7)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "NUBIS  " JucePlugin_VersionString,
                "Granular texture engine by 21bayyona.\n\n"
                "Drag modulation sources onto any knob. Right-click a knob for modulation options.\n"
                "Shift-click RANDOMIZE for experimental patches.\n\n"
                "Fonts: Outfit and Geist Mono (SIL Open Font License).");
    });
}

void MainSurface::savePreset()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name for the new user preset:", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", proc.presets.getCurrentName());
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<MainSurface> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (result == 1 && safe != nullptr)
        {
            if (! safe->proc.presets.saveUserPreset (w->getTextEditorContents ("name")))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save preset", "The preset could not be saved.");
            safe->refreshPresetLabel();
        }
    }), true);
}

void MainSurface::loadSampleDialog()
{
    chooser = std::make_unique<juce::FileChooser> ("Load sample", juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (! f.existsAsFile()) return;
        if (! proc.loadSampleFile (f))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Load sample", "This file could not be read.");
        updateSourceControls();
    });
}

//==============================================================================
void MainSurface::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    // faint radial glow behind the centre display
    juce::ColourGradient glow (accent.withAlpha (0.10f), (float) centrePanel.getCentreX(), (float) centrePanel.getY() + 140.0f,
                               juce::Colours::transparentBlack, (float) centrePanel.getCentreX(), (float) centrePanel.getY() + 520.0f, true);
    g.setGradientFill (glow);
    g.fillRect (getLocalBounds());

    // ---- top bar ------------------------------------------------------------------
    {
        auto r = topBar;
        auto logo = r.removeFromLeft (250);
        const auto c = juce::Point<float> ((float) logo.getX() + 22.0f, (float) logo.getCentreY());
        // logo mark: a ring of particles condensing into a cloud
        for (int i = 0; i < 18; ++i)
        {
            const float a = (float) i / 18.0f * juce::MathConstants<float>::twoPi;
            const float rad = 10.0f + 3.0f * std::sin ((float) i * 2.3f);
            const float s = 1.5f + 1.2f * (float) ((i * 7) % 3) / 2.0f;
            g.setColour ((i % 3 == 0 ? accentHi : text).withAlpha (0.55f + 0.15f * (float) (i % 3)));
            g.fillEllipse (c.x + std::cos (a) * rad - s * 0.5f, c.y + std::sin (a) * rad - s * 0.5f, s, s);
        }
        g.setColour (accentHi);
        g.fillEllipse (c.x - 3.0f, c.y - 3.0f, 6.0f, 6.0f);
        g.setColour (text);
        g.setFont (font (24.0f, true).withExtraKerningFactor (0.28f));
        g.drawText ("NUBIS", logo.withTrimmedLeft (46).withTrimmedBottom (14), juce::Justification::bottomLeft);
        g.setColour (textDim);
        g.setFont (font (9.5f, true).withExtraKerningFactor (0.22f));
        g.drawText (NubisAudioProcessor::isInstrument() ? "GRANULAR TEXTURE ENGINE  /  INST" : "GRANULAR TEXTURE ENGINE  /  FX", logo.withTrimmedLeft (47).withTrimmedTop (31), juce::Justification::topLeft);
    }
    g.setColour (line);
    g.drawHorizontalLine (topBar.getBottom() + 3, 8.0f, (float) getWidth() - 8.0f);

    // ---- left panel ----------------------------------------------------------------
    drawPanel (g, leftPanel.toFloat());
    drawHeading (g, "Grain Engine", grainHead);
    {
        // envelope preview
        auto r = envPreviewArea.toFloat();
        g.setColour (well);
        g.fillRoundedRectangle (r, 5.0f);
        const float* t = envTables.get (lastEnvelope < 0 ? 0 : lastEnvelope);
        juce::Path p;
        auto rr = r.reduced (6.0f, 5.0f);
        for (int i = 0; i <= 64; ++i)
        {
            const float v = t[i * EnvelopeTables::kSize / 64];
            const float x = rr.getX() + rr.getWidth() * (float) i / 64.0f, y = rr.getBottom() - v * rr.getHeight();
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (accentHi);
        g.strokePath (p, juce::PathStrokeType (1.5f));
    }

    // ---- centre --------------------------------------------------------------------
    drawPanel (g, centrePanel.toFloat());
    drawHeading (g, "Texture Engine", textureHead);
    drawHeading (g, "Creative", specialHead);
    if (loadSampleButton.isVisible())
    {
        g.setColour (sampleName.isEmpty() ? textFaint : text);
        g.setFont (font (12.0f));
        g.drawText (sampleName.isEmpty() ? "no sample loaded" : sampleName,
                    juce::Rectangle<int> (loadSampleButton.getRight() + 8, loadSampleButton.getY(), 200, loadSampleButton.getHeight()),
                    juce::Justification::centredLeft, true);
    }
}

void MainSurface::resized()
{
    topBar      = { 8, 6, kWidth - 16, 46 };
    leftPanel   = { 8, 60, 292, 532 };
    centrePanel = { 308, 60, 664, 532 };
    rightPanel  = { 980, 60, 292, 532 };
    bottomPanel = { 8, 600, kWidth - 16, 192 };

    // ---- top bar ---------------------------------------------------------------
    {
        auto r = topBar.reduced (0, 8);
        r.removeFromLeft (260);
        settingsButton.setBounds (r.removeFromRight (34));
        r.removeFromRight (90);
        randomButton.setBounds (r.removeFromRight (108));
        r.removeFromRight (16);
        auto presetArea = r.withSizeKeepingCentre (520, r.getHeight());
        prevButton.setBounds (presetArea.removeFromLeft (30));
        presetArea.removeFromLeft (4);
        loadButton.setBounds (presetArea.removeFromRight (62));
        presetArea.removeFromRight (4);
        saveButton.setBounds (presetArea.removeFromRight (62));
        presetArea.removeFromRight (4);
        nextButton.setBounds (presetArea.removeFromRight (30));
        presetArea.removeFromRight (4);
        presetButton.setBounds (presetArea);
    }

    // ---- left: grain engine -------------------------------------------------------
    {
        auto r = leftPanel.reduced (12, 8);
        grainHead = r.removeFromTop (20);
        r.removeFromTop (6);
        const int cols = 3, rows = 4;
        const int cw = r.getWidth() / cols, ch = 96;
        for (int i = 0; i < 12; ++i)
            grainKnobs[(size_t) i]->setBounds (r.getX() + (i % cols) * cw, r.getY() + (i / cols) * ch, cw, ch - 6);
        r.removeFromTop (rows * ch + 4);
        auto envRow = r.removeFromTop (28);
        envelopeCombo.setBounds (envRow.removeFromLeft (128));
        envRow.removeFromLeft (8);
        envPreviewArea = envRow;
        r.removeFromTop (10);
        freezeButton.setBounds (r.removeFromTop (30));
    }

    // ---- centre ---------------------------------------------------------------------
    {
        auto r = centrePanel.reduced (12, 10);
        auto head = r.removeFromTop (28);
        sourceCombo.setBounds (head.removeFromLeft (124));
        head.removeFromLeft (8);
        const auto afterSource = head;
        internalCombo.setBounds (afterSource.withWidth (110));
        loadSampleButton.setBounds (afterSource.withWidth (104));
        playModeCombo.setBounds (head.removeFromRight (120));
        r.removeFromTop (8);
        visualizer.setBounds (r.removeFromTop (226));
        r.removeFromTop (4);
        spectrogram.setBounds (r.removeFromTop (42));
        r.removeFromTop (8);
        textureHead = r.removeFromTop (18);
        auto texRow = r.removeFromTop (80);
        const int tw = texRow.getWidth() / 8;
        for (auto& k : textureKnobs) k->setBounds (texRow.removeFromLeft (tw).reduced (2, 0));
        r.removeFromTop (6);
        specialHead = r.removeFromTop (18);
        auto spRow = r;
        auto toggles = spRow.removeFromLeft (150);
        infFreezeButton.setBounds (toggles.removeFromTop (34).withTrimmedTop (4));
        toggles.removeFromTop (8);
        spectralCombo.setBounds (toggles.removeFromTop (26));
        spRow.removeFromLeft (8);
        const int sw = spRow.getWidth() / 6;
        for (auto& k : specialKnobs) k->setBounds (spRow.removeFromLeft (sw).reduced (2, 0));
    }

    modPanel.setBounds (rightPanel);
    fxPanel.setBounds (bottomPanel);
}

//==============================================================================
NubisAudioProcessorEditor::NubisAudioProcessorEditor (NubisAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), surface (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (surface);

    const double scale = (double) proc.apvts.state.getProperty ("uiScale", 0.875);
    setResizable (true, true);
    setResizeLimits (MainSurface::kWidth / 2, MainSurface::kHeight / 2, MainSurface::kWidth * 2, MainSurface::kHeight * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) MainSurface::kWidth / (double) MainSurface::kHeight);
    setSize (juce::roundToInt (MainSurface::kWidth * scale), juce::roundToInt (MainSurface::kHeight * scale));

    proc.presets.onPresetLoaded = [this] { triggerAsyncUpdate(); };
    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

NubisAudioProcessorEditor::~NubisAudioProcessorEditor()
{
    stopTimer();
    cancelPendingUpdate();
    proc.presets.onPresetLoaded = nullptr;
    setLookAndFeel (nullptr);
}

void NubisAudioProcessorEditor::paint (juce::Graphics& g) { g.fillAll (theme::bg); }

void NubisAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) MainSurface::kWidth;
    surface.setTransform (juce::AffineTransform::scale (scale));
    surface.setBounds (0, 0, MainSurface::kWidth, MainSurface::kHeight);
    proc.apvts.state.setProperty ("uiScale", scale, nullptr);
}

void NubisAudioProcessorEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) juce::jlimit (0.0, 0.1, (now - lastTick) * 0.001);
    lastTick = now;
    surface.tick (dt);
}

void NubisAudioProcessorEditor::handleAsyncUpdate() { surface.refreshPresetLabel(); }

bool NubisAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;ogg;mp3") || juce::File (f).hasFileExtension (PresetManager::kExtension))
            return true;
    return false;
}

void NubisAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& path : files)
    {
        const juce::File f (path);
        if (f.hasFileExtension (PresetManager::kExtension)) { proc.presets.importPresetFile (f); return; }
        if (proc.loadSampleFile (f))
        {
            if (auto* src = proc.apvts.getParameter (pid::source))
            {
                src->beginChangeGesture();
                src->setValueNotifyingHost (src->convertTo0to1 ((float) SourceMode::Sample));
                src->endChangeGesture();
            }
            return;
        }
    }
}
