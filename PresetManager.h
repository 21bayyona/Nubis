// =============================================================================
//  NUBIS - PresetManager.h : factory + user presets
// =============================================================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/EffectsEngine.h"

namespace nubis
{
struct FactoryPreset
{
    const char* name;
    const char* category;
    std::vector<std::pair<const char*, float>> values;           // plain (real-unit) values
    std::vector<std::tuple<int, const char*, float>> mods;       // source, destination param id, amount
    std::array<int, NumFx> order { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
};
const std::vector<FactoryPreset>& getFactoryPresets();

inline const char* const kPresetCategories[] = { "Atmospheric", "Experimental", "Vocal Processing", "Granular Pads", "Glitch",
                                                 "Metallic", "Cinematic", "Drones", "Rhythmic", "Dark Textures", "User" };

struct PresetInfo
{
    juce::String name, category;
    bool isFactory = true;
    int factoryIndex = -1;
    juce::File file;
};

class PresetManager
{
public:
    using OrderSetter = std::function<void (const std::array<int, NumFx>&)>;
    using OrderGetter = std::function<std::array<int, NumFx>()>;

    PresetManager (juce::AudioProcessorValueTreeState& state, OrderSetter setter, OrderGetter getter);

    void refresh();
    const juce::Array<PresetInfo>& getPresets() const noexcept { return presets; }

    void loadPreset (int index);
    void loadNext();
    void loadPrevious();
    void initialise();          // "Init" patch: every parameter to default

    bool saveUserPreset (const juce::String& name);
    bool importPresetFile (const juce::File& f);

    int getCurrentIndex() const noexcept { return currentIndex; }
    juce::String getCurrentName() const { return currentName; }
    void setCurrentName (const juce::String& n) { currentName = n; currentIndex = indexOfName (n); }

    static juce::File getUserPresetFolder();
    static constexpr const char* kExtension = ".nubispreset";

    std::function<void()> onPresetLoaded;

private:
    int indexOfName (const juce::String& n) const;
    void resetToDefaults();
    void setParam (const juce::String& id, float plainValue);
    bool loadXml (const juce::XmlElement& xml);

    juce::AudioProcessorValueTreeState& apvts;
    OrderSetter setOrder;
    OrderGetter getOrder;
    juce::Array<PresetInfo> presets;
    int currentIndex = -1;
    juce::String currentName { "Init" };
};

} // namespace nubis
