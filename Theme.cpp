// =============================================================================
//  NUBIS - Theme.cpp
// =============================================================================
#include "Theme.h"
#include "BinaryData.h"

namespace nubis::theme
{
juce::Typeface::Ptr FontHolder::load (const void* data, int size)
{
    return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
}

FontHolder::FontHolder()
    : regular (load (BinaryData::OutfitRegular_ttf, BinaryData::OutfitRegular_ttfSize)),
      bold    (load (BinaryData::OutfitBold_ttf, BinaryData::OutfitBold_ttfSize)),
      monoTf  (load (BinaryData::GeistMonoRegular_ttf, BinaryData::GeistMonoRegular_ttfSize))
{
}

juce::Typeface::Ptr regularTypeface() { juce::SharedResourcePointer<FontHolder> h; return h->regular; }
static juce::Typeface::Ptr boldTypeface() { juce::SharedResourcePointer<FontHolder> h; return h->bold; }
static juce::Typeface::Ptr monoTypeface() { juce::SharedResourcePointer<FontHolder> h; return h->monoTf; }

juce::Font font (float height, bool bold)
{
    return juce::Font (juce::FontOptions (bold ? boldTypeface() : regularTypeface()).withHeight (height));
}

juce::Font mono (float height)
{
    return juce::Font (juce::FontOptions (monoTypeface()).withHeight (height));
}

void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    g.setColour (panel);
    g.fillRoundedRectangle (r, radius);
    juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.025f), r.getX(), r.getY(),
                                juce::Colours::transparentWhite, r.getX(), r.getY() + 60.0f, false);
    g.setGradientFill (sheen);
    g.fillRoundedRectangle (r, radius);
    g.setColour (line);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}

void drawHeading (juce::Graphics& g, const juce::String& title, juce::Rectangle<int> area, juce::Colour c)
{
    g.setColour (accentHi);
    g.fillRoundedRectangle ((float) area.getX(), (float) area.getCentreY() - 3.0f, 2.0f, 6.0f, 1.0f);
    g.setColour (c);
    g.setFont (font (11.0f, true).withExtraKerningFactor (0.18f));
    g.drawText (title.toUpperCase(), area.withTrimmedLeft (8), juce::Justification::centredLeft, false);
}
} // namespace nubis::theme
