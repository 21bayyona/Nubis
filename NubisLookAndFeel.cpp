// =============================================================================
//  NUBIS - NubisLookAndFeel.cpp
// =============================================================================
#include "NubisLookAndFeel.h"

namespace nubis
{
using namespace theme;

NubisLookAndFeel::NubisLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::PopupMenu::backgroundColourId, panelHi);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accentSoft);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::backgroundColourId, well);
    setColour (juce::ComboBox::outlineColourId, line);
    setColour (juce::ComboBox::arrowColourId, textDim);
    setColour (juce::Label::textColourId, text);
    setColour (juce::TextButton::textColourOffId, textDim);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::TextEditor::backgroundColourId, well);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, line);
    setColour (juce::TextEditor::focusedOutlineColourId, accentHi);
    setColour (juce::AlertWindow::backgroundColourId, panelHi);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, line);
    setColour (juce::ScrollBar::thumbColourId, lineHi);
    setColour (juce::TooltipWindow::backgroundColourId, panelHi);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::Slider::textBoxTextColourId, text);
}

juce::Typeface::Ptr NubisLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
        return fonts->regular;
    return LookAndFeel_V4::getTypefaceForFont (f);
}

//==============================================================================
void NubisLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat();
    const float size = std::min (bounds.getWidth(), bounds.getHeight());
    const auto r = juce::Rectangle<float> (size, size).withCentre (bounds.getCentre()).reduced (3.0f);
    const auto c = r.getCentre();
    const float radius = r.getWidth() * 0.5f;
    const float arcR = radius - 2.0f;
    const bool hover = s.isMouseOverOrDragging();
    const bool bipolar = s.getProperties().getWithDefault ("bipolar", false);
    const float angle = start + pos * (end - start);
    const auto accentCol = s.isEnabled() ? accentHi : textFaint;

    // track
    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, start, end, true);
    g.setColour (line);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc
    const float from = bipolar ? (start + end) * 0.5f : start;
    juce::Path value;
    value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, std::min (from, angle), std::max (from, angle), true);
    if (hover)
    {
        g.setColour (accentCol.withAlpha (0.25f));
        g.strokePath (value, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (accentCol);
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // body
    const float bodyR = radius * 0.68f;
    juce::ColourGradient body (panelHi.brighter (0.15f), c.x, c.y - bodyR, well, c.x, c.y + bodyR, false);
    g.setGradientFill (body);
    g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (hover ? lineHi.brighter (0.2f) : lineHi);
    g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

    // pointer
    const juce::Point<float> p1 = c.getPointOnCircumference (bodyR * 0.35f, angle);
    const juce::Point<float> p2 = c.getPointOnCircumference (bodyR * 0.85f, angle);
    g.setColour (s.isEnabled() ? text : textDim);
    g.drawLine ({ p1, p2 }, 2.0f);
}

void NubisLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                         juce::Slider::SliderStyle style, juce::Slider& s)
{
    const auto r = juce::Rectangle<int> (x, y, w, h).toFloat();
    const bool bipolar = s.getProperties().getWithDefault ("bipolar", false);
    if (style == juce::Slider::LinearHorizontal)
    {
        const auto trackR = r.withSizeKeepingCentre (r.getWidth(), 4.0f);
        g.setColour (line);
        g.fillRoundedRectangle (trackR, 2.0f);
        const float zero = bipolar ? trackR.getCentreX() : trackR.getX();
        g.setColour (s.isMouseOverOrDragging() ? accentHi : accentHi.withAlpha (0.85f));
        g.fillRoundedRectangle (juce::Rectangle<float> (std::min (zero, pos), trackR.getY(), std::abs (pos - zero), trackR.getHeight()), 2.0f);
        g.setColour (text);
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ pos, trackR.getCentreY() }));
    }
    else
    {
        const auto trackR = r.withSizeKeepingCentre (4.0f, r.getHeight());
        g.setColour (line);
        g.fillRoundedRectangle (trackR, 2.0f);
        g.setColour (accentHi);
        g.fillRoundedRectangle (juce::Rectangle<float> (trackR.getX(), pos, trackR.getWidth(), trackR.getBottom() - pos), 2.0f);
    }
}

//==============================================================================
void NubisLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const float radius = std::min (6.0f, r.getHeight() * 0.5f);
    const bool isTab = b.getProperties().getWithDefault ("tab", false);

    if (isTab)
    {
        g.setColour (on ? panelHi : juce::Colours::transparentBlack);
        g.fillRoundedRectangle (r, radius);
        if (on) { g.setColour (accentHi); g.fillRoundedRectangle (r.getX() + 6.0f, r.getBottom() - 2.0f, r.getWidth() - 12.0f, 2.0f, 1.0f); }
        else if (highlighted) { g.setColour (panelHi.withAlpha (0.5f)); g.fillRoundedRectangle (r, radius); }
        return;
    }

    g.setColour (on ? accent.withAlpha (0.35f) : (down ? panelHi.brighter (0.1f) : (highlighted ? panelHi.brighter (0.05f) : well)));
    g.fillRoundedRectangle (r, radius);
    if (on)
    {
        g.setColour (accentHi.withAlpha (0.18f));
        g.drawRoundedRectangle (r.expanded (1.0f), radius + 1.0f, 2.0f);
    }
    g.setColour (on ? accentHi : (highlighted ? lineHi.brighter (0.2f) : line));
    g.drawRoundedRectangle (r, radius, 1.0f);

    if (b.getProperties().getWithDefault ("led", false))
    {
        const auto led = juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ r.getX() + 10.0f, r.getCentreY() });
        if (on) { g.setColour (accentHi.withAlpha (0.35f)); g.fillEllipse (led.expanded (3.0f)); }
        g.setColour (on ? accentHi : textFaint);
        g.fillEllipse (led);
    }
}

juce::Font NubisLookAndFeel::getTextButtonFont (juce::TextButton&, int height)
{
    return font (std::min (12.5f, (float) height * 0.5f), true).withExtraKerningFactor (0.06f);
}

void NubisLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool highlighted, bool)
{
    const bool on = b.getToggleState();
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (on ? juce::Colours::white : (highlighted ? text : textDim));
    auto r = b.getLocalBounds();
    if (b.getProperties().getWithDefault ("led", false)) r.removeFromLeft (14);
    g.drawFittedText (b.getButtonText(), r.reduced (4, 0), juce::Justification::centred, 1, 0.8f);
}

//==============================================================================
void NubisLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (well);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (box.isMouseOver (true) ? lineHi.brighter (0.2f) : line);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.startNewSubPath (ax - 3.5f, ay - 1.5f);
    arrow.lineTo (ax, ay + 2.0f);
    arrow.lineTo (ax + 3.5f, ay - 1.5f);
    g.setColour (textDim);
    g.strokePath (arrow, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font NubisLookAndFeel::getComboBoxFont (juce::ComboBox& b) { return font (std::min (13.0f, (float) b.getHeight() * 0.55f)); }

void NubisLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 0, box.getWidth() - 22, box.getHeight());
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

//==============================================================================
void NubisLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (panelHi);
    g.setColour (lineHi);
    g.drawRect (0, 0, w, h, 1);
}

void NubisLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                                          bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& txt,
                                          const juce::String&, const juce::Drawable*, const juce::Colour*)
{
    if (isSeparator)
    {
        g.setColour (line);
        g.fillRect (area.reduced (8, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }
    auto r = area.reduced (3, 1);
    if (isHighlighted && isActive)
    {
        g.setColour (accentSoft);
        g.fillRoundedRectangle (r.toFloat(), 4.0f);
    }
    if (isTicked)
    {
        g.setColour (accentHi);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ (float) r.getX() + 9.0f, (float) r.getCentreY() }));
    }
    g.setColour (isActive ? (isHighlighted ? juce::Colours::white : text) : textFaint);
    g.setFont (getPopupMenuFont());
    g.drawText (txt, r.withTrimmedLeft (20).withTrimmedRight (16), juce::Justification::centredLeft, true);
    if (hasSubMenu)
    {
        juce::Path p;
        const float ax = (float) r.getRight() - 9.0f, ay = (float) r.getCentreY();
        p.startNewSubPath (ax - 2.0f, ay - 3.5f); p.lineTo (ax + 1.5f, ay); p.lineTo (ax - 2.0f, ay + 3.5f);
        g.strokePath (p, juce::PathStrokeType (1.3f));
    }
}

juce::Font NubisLookAndFeel::getPopupMenuFont() { return font (14.0f); }

void NubisLookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& name)
{
    g.setColour (textDim);
    g.setFont (font (11.0f, true).withExtraKerningFactor (0.15f));
    g.drawText (name.toUpperCase(), area.withTrimmedLeft (12), juce::Justification::centredLeft, true);
}

//==============================================================================
void NubisLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& txt, int w, int h)
{
    g.setColour (panelHi);
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 5.0f);
    g.setColour (lineHi);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 5.0f, 1.0f);
    g.setColour (text);
    g.setFont (font (13.0f));
    g.drawFittedText (txt, juce::Rectangle<int> (w, h).reduced (8, 4), juce::Justification::centredLeft, 4);
}

juce::Rectangle<int> NubisLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    const int w = std::min (320, (int) juce::GlyphArrangement::getStringWidth (font (13.0f), tipText) + 20);
    const int lines = 1 + (int) juce::GlyphArrangement::getStringWidth (font (13.0f), tipText) / 300;
    const int h = 10 + lines * 16;
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 12,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 8) : pos.y + 18, w, h).constrainedWithin (parent);
}

void NubisLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                                      int thumbStart, int thumbSize, bool over, bool)
{
    const auto r = vertical ? juce::Rectangle<int> (x + w / 2 - 2, thumbStart, 4, thumbSize)
                            : juce::Rectangle<int> (thumbStart, y + h / 2 - 2, thumbSize, 4);
    g.setColour (over ? lineHi.brighter (0.3f) : lineHi);
    g.fillRoundedRectangle (r.toFloat(), 2.0f);
}

} // namespace nubis
