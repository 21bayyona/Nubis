// =============================================================================
//  NUBIS - UiSnapshot.cpp : renders the editor to PNG with live audio running
//  (used to review the interface without a DAW).
//  Usage: NubisUiSnapshot <out.png> [presetIndex] [scale]
// =============================================================================
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "nubis.png"));
    const int preset = argc > 2 ? juce::String (argv[2]).getIntValue() : 0;
    const float scale = argc > 3 ? juce::String (argv[3]).getFloatValue() : 1.0f;

    auto proc = std::make_unique<NubisAudioProcessor>();
    proc->setPlayConfigDetails (2, 2, 48000.0, 512);
    proc->prepareToPlay (48000.0, 512);
    if (preset >= 0) proc->presets.loadPreset (preset);
    if (argc > 4)
        if (auto* src = proc->apvts.getParameter (nubis::pid::source))
            src->setValueNotifyingHost (src->convertTo0to1 (juce::String (argv[4]).getFloatValue()));
    if (argc > 5)
        if (auto* f = proc->apvts.getParameter (nubis::pid::infFreeze)) f->setValueNotifyingHost (1.0f);

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());
    ed->setSize (juce::roundToInt (1280 * scale), juce::roundToInt (800 * scale));

    if (argc > 6)
    {
        const juce::String page (argv[6]);
        std::function<void (juce::Component&)> visit = [&] (juce::Component& c)
        {
            if (auto* b = dynamic_cast<juce::TextButton*> (&c)) if (page == "matrix" && b->getButtonText() == "MATRIX") b->triggerClick();
            if (auto* chip = dynamic_cast<nubis::ModSourceChip*> (&c)) if (chip->getTooltip().containsIgnoreCase ("drag " + page + " ")) if (chip->onClick) chip->onClick();
            for (auto* child : c.getChildren()) visit (*child);
        };
        visit (*ed);
    }
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    juce::Random r (1);
    int64_t t = 0;
    for (int frame = 0; frame < 760; ++frame)
    {
        for (int b = 0; b < 2; ++b)
        {
            for (int i = 0; i < 512; ++i, ++t)
            {
                const float s = (float) t / 48000.0f;
                const float env = 0.5f + 0.5f * std::sin (6.2831853f * 0.7f * s);
                const float v = env * (0.3f * std::sin (6.2831853f * 196.0f * s) + 0.15f * std::sin (6.2831853f * 293.66f * s) + 0.08f * r.nextFloat());
                buf.setSample (0, i, v); buf.setSample (1, i, v * 0.85f);
            }
            proc->processBlock (buf, midi);
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (16);
    }

    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::PNGImageFormat png;
    out.deleteFile();
    juce::FileOutputStream os (out);
    const bool ok = os.openedOk() && png.writeImageToStream (img, os);
    std::printf ("%s %s (%d x %d)\n", ok ? "wrote" : "FAILED", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    proc.reset();
    return ok ? 0 : 1;
}
