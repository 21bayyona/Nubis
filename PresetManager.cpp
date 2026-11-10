// =============================================================================
//  NUBIS - PresetManager.cpp
// =============================================================================
#include "PresetManager.h"
#include "Parameters.h"

namespace nubis
{
PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state, OrderSetter setter, OrderGetter getter)
    : apvts (state), setOrder (std::move (setter)), getOrder (std::move (getter))
{
    refresh();
}

juce::File PresetManager::getUserPresetFolder()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("21bayyona").getChildFile ("Nubis").getChildFile ("Presets");
    if (! dir.exists()) dir.createDirectory();
    return dir;
}

void PresetManager::refresh()
{
    presets.clear();
    const auto& fac = getFactoryPresets();
    for (int i = 0; i < (int) fac.size(); ++i)
    {
        PresetInfo info;
        info.name = fac[(size_t) i].name;
        info.category = fac[(size_t) i].category;
        info.isFactory = true;
        info.factoryIndex = i;
        presets.add (info);
    }
    auto files = getUserPresetFolder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + kExtension);
    files.sort();
    for (auto& f : files)
    {
        PresetInfo info;
        info.name = f.getFileNameWithoutExtension();
        info.category = "User";
        info.isFactory = false;
        info.file = f;
        presets.add (info);
    }
    currentIndex = indexOfName (currentName);
}

int PresetManager::indexOfName (const juce::String& n) const
{
    for (int i = 0; i < presets.size(); ++i) if (presets[i].name == n) return i;
    return -1;
}

void PresetManager::setParam (const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
        p->endChangeGesture();
    }
}

void PresetManager::resetToDefaults()
{
    for (auto* rp : apvts.processor.getParameters())
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (rp))
        {
            if (isGlobalContextParam (p->getParameterID())) continue;
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getDefaultValue());
            p->endChangeGesture();
        }
    }
    setOrder ({ 0, 1, 2, 3, 4, 5, 6, 7, 8 });
}

void PresetManager::initialise()
{
    resetToDefaults();
    currentName = "Init";
    currentIndex = -1;
    if (onPresetLoaded) onPresetLoaded();
}

void PresetManager::loadPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, presets.size())) return;
    const auto& info = presets.getReference (index);

    if (info.isFactory)
    {
        const auto& fp = getFactoryPresets()[(size_t) info.factoryIndex];
        resetToDefaults();
        for (auto& [id, v] : fp.values) setParam (id, v);
        int slot = 0;
        for (auto& [src, dst, amt] : fp.mods)
        {
            if (slot >= kNumModSlots) break;
            const int d = findModDestination (dst);
            if (d < 0) continue;
            setParam (pid::modSrc (slot), (float) src);
            setParam (pid::modDst (slot), (float) (d + 1));
            setParam (pid::modAmt (slot), amt);
            ++slot;
        }
        setOrder (fp.order);
    }
    else
    {
        auto xml = juce::XmlDocument::parse (info.file);
        if (xml == nullptr || ! loadXml (*xml)) return;
    }
    currentIndex = index;
    currentName = info.name;
    if (onPresetLoaded) onPresetLoaded();
}

bool PresetManager::loadXml (const juce::XmlElement& xml)
{
    if (! xml.hasTagName ("NubisPreset")) return false;
    resetToDefaults();
    for (auto* e : xml.getChildWithTagNameIterator ("P"))
    {
        const auto id = e->getStringAttribute ("id");
        if (isGlobalContextParam (id)) continue;
        setParam (id, (float) e->getDoubleAttribute ("v"));
    }
    juce::StringArray parts;
    parts.addTokens (xml.getStringAttribute ("fxOrder", "0,1,2,3,4,5,6,7,8"), ",", "");
    if (parts.size() == NumFx)
    {
        std::array<int, NumFx> o {};
        for (int i = 0; i < NumFx; ++i) o[(size_t) i] = parts[i].getIntValue();
        setOrder (unpackOrder (packOrder (o)));
    }
    return true;
}

void PresetManager::loadNext()
{
    if (presets.isEmpty()) return;
    loadPreset ((currentIndex + 1 + presets.size()) % presets.size());
}

void PresetManager::loadPrevious()
{
    if (presets.isEmpty()) return;
    loadPreset ((currentIndex - 1 + presets.size()) % presets.size());
}

bool PresetManager::saveUserPreset (const juce::String& rawName)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty()) return false;

    juce::XmlElement xml ("NubisPreset");
    xml.setAttribute ("name", name);
    xml.setAttribute ("version", JucePlugin_VersionString);
    const auto order = getOrder();
    juce::StringArray o;
    for (auto v : order) o.add (juce::String (v));
    xml.setAttribute ("fxOrder", o.joinIntoString (","));

    for (auto* rp : apvts.processor.getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (rp))
        {
            auto* e = xml.createNewChildElement ("P");
            e->setAttribute ("id", p->getParameterID());
            e->setAttribute ("v", p->convertFrom0to1 (p->getValue()));
        }

    const auto file = getUserPresetFolder().getChildFile (name + kExtension);
    if (! xml.writeTo (file)) return false;
    currentName = name;
    refresh();
    return true;
}

bool PresetManager::importPresetFile (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName ("NubisPreset")) return false;
    const auto dest = getUserPresetFolder().getChildFile (f.getFileName());
    if (f != dest) f.copyFileTo (dest);
    currentName = dest.getFileNameWithoutExtension();
    refresh();
    loadPreset (indexOfName (currentName));
    return true;
}

} // namespace nubis
