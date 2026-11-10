// =============================================================================
//  NUBIS - Theme.h : colours, fonts, metrics
// =============================================================================
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace nubis::theme
{
// palette — near-black graphite with a single signature accent (#941135)
inline const juce::Colour bg        { 0xff0a0b0e };
inline const juce::Colour panel     { 0xff111318 };
inline const juce::Colour panelHi   { 0xff171a21 };
inline const juce::Colour well      { 0xff0c0d11 };
inline const juce::Colour line      { 0xff22262f };
inline const juce::Colour lineHi    { 0xff323743 };
inline const juce::Colour text      { 0xffe7e9ee };
inline const juce::Colour textDim   { 0xff8a90a0 };
inline const juce::Colour textFaint { 0xff4d5260 };
inline const juce::Colour accent    { 0xff941135 };   // brand red
inline const juce::Colour accentHi  { 0xffe03a6a };   // luminous variant for arcs/glows
inline const juce::Colour accentSoft{ 0xff5a1328 };
inline const juce::Colour ice       { 0xffbfe6ff };   // secondary cool highlight (reverse grains, frozen state)
inline const juce::Colour modCol    { 0xfff2b84b };   // modulation amber

/** Holds the embedded typefaces. Keep one alive (editor / look-and-feel own one). */
struct FontHolder
{
    FontHolder();
    juce::Typeface::Ptr regular, bold, monoTf;
private:
    static juce::Typeface::Ptr load (const void* data, int size);
};

juce::Font font (float height, bool bold = false);
juce::Font mono (float height);
juce::Typeface::Ptr regularTypeface();

/** Panel background with hairline border and faint top sheen. */
void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius = 10.0f);
/** Small spaced-caps section heading. */
void drawHeading (juce::Graphics& g, const juce::String& title, juce::Rectangle<int> area, juce::Colour c = textDim);
} // namespace nubis::theme
