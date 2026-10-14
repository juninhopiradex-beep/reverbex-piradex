#include "LookAndFeel.h"

namespace roomx::ui
{
RoomXLookAndFeel::RoomXLookAndFeel (const Accent& a) : accent (a)
{
    setColour (juce::ResizableWindow::backgroundColourId, col::bg);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::panelHi);
    setColour (juce::PopupMenu::highlightedTextColourId, col::text);
    setColour (juce::TextEditor::backgroundColourId, col::well);
    setColour (juce::TextEditor::textColourId, col::text);
    setColour (juce::TextEditor::highlightColourId, col::all.withAlpha (0.3f));
    setColour (juce::CaretComponent::caretColourId, col::text);
    setColour (juce::Label::textColourId, col::text);
    setColour (juce::TooltipWindow::backgroundColourId, col::panelHi);
    setColour (juce::TooltipWindow::textColourId, col::text);
    setColour (juce::TextButton::textColourOffId, col::textDim);
    setColour (juce::TextButton::textColourOnId, col::text);
    setColour (juce::Slider::textBoxTextColourId, col::text);
    setColour (juce::Slider::textBoxBackgroundColourId, col::well);
    setColour (juce::Slider::textBoxOutlineColourId, col::line);
    setColour (juce::AlertWindow::backgroundColourId, col::panel);
    setColour (juce::AlertWindow::textColourId, col::text);
    setColour (juce::AlertWindow::outlineColourId, col::line);
    setColour (juce::ComboBox::backgroundColourId, col::well);
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::outlineColourId, col::line);
    setColour (juce::ToggleButton::textColourId, col::textDim);
    setColour (juce::ToggleButton::tickColourId, col::text);
}

juce::Typeface::Ptr RoomXLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
        return f.isBold() ? Fonts::semibold() : Fonts::regular();
    return LookAndFeel_V4::getTypefaceForFont (f);
}

void RoomXLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float r  = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto  c  = bounds.getCentre();
    const float trackR = r - 2.5f;
    const bool  bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const bool  enabled = s.isEnabled();
    const auto  acc = enabled ? accent.current : col::textFaint;

    // pista
    juce::Path track;
    track.addCentredArc (c.x, c.y, trackR, trackR, 0.0f, startAngle, endAngle, true);
    g.setColour (col::line);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // valor
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float from  = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.001f)
    {
        juce::Path val;
        val.addCentredArc (c.x, c.y, trackR, trackR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (acc.withAlpha (0.25f));
        g.strokePath (val, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (acc);
        g.strokePath (val, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // corpo
    const float br = r * 0.70f;
    const auto body = juce::Rectangle<float> (br * 2.0f, br * 2.0f).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (body.translated (0.0f, 2.5f).expanded (1.5f));
    juce::ColourGradient grad (juce::Colour (0xff2a303b), c.x, body.getY(), juce::Colour (0xff13161c), c.x, body.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (body);
    g.setColour (juce::Colour (0xff3a4250));
    g.drawEllipse (body.reduced (0.5f), 1.0f);
    // faceta interior
    g.setColour (juce::Colours::white.withAlpha (0.035f));
    g.fillEllipse (body.reduced (br * 0.28f));

    // indicador (ponto)
    const float ir = br * 0.72f;
    const juce::Point<float> ip (c.x + ir * std::sin (angle), c.y - ir * std::cos (angle));
    g.setColour (enabled ? col::silver : col::textFaint);
    g.fillEllipse (juce::Rectangle<float> (4.2f, 4.2f).withCentre (ip));
}

void RoomXLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                         juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);
        return;
    }
    const float cx = (float) x + (float) w * 0.5f;
    g.setColour (col::well);
    g.fillRoundedRectangle (cx - 2.5f, (float) y, 5.0f, (float) h, 2.5f);
    g.setColour (col::line);
    g.drawRoundedRectangle (cx - 2.5f, (float) y, 5.0f, (float) h, 2.5f, 1.0f);

    // marca de 0 dB
    const double zeroPos = s.valueToProportionOfLength (0.0);
    const float zy = (float) y + (float) h * (1.0f - (float) zeroPos);
    g.setColour (col::textFaint);
    g.fillRect (cx - 9.0f, zy - 0.5f, 18.0f, 1.0f);

    // cursor
    const auto thumb = juce::Rectangle<float> (26.0f, 13.0f).withCentre ({ cx, pos });
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (thumb.translated (0, 2), 4.0f);
    juce::ColourGradient grad (juce::Colour (0xff3a414e), cx, thumb.getY(), juce::Colour (0xff1c2028), cx, thumb.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (thumb, 4.0f);
    g.setColour (accent.current);
    g.fillRect (thumb.getX() + 5.0f, thumb.getCentreY() - 0.75f, thumb.getWidth() - 10.0f, 1.5f);
}

void RoomXLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const float corner = (float) b.getProperties().getWithDefault ("corner", 6.0);
    const bool danger = (bool) b.getProperties().getWithDefault ("danger", false);
    const auto acc = danger ? col::danger : accent.current;

    g.setColour (on ? acc.withAlpha (0.16f) : (down ? col::panelHi.brighter (0.08f) : (over ? col::panelHi.brighter (0.04f) : col::panelHi)));
    g.fillRoundedRectangle (r, corner);
    g.setColour (on ? acc.withAlpha (0.85f) : (over ? col::lineHi : col::line));
    g.drawRoundedRectangle (r, corner, 1.0f);
}

void RoomXLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool)
{
    const bool on = b.getToggleState();
    const bool danger = (bool) b.getProperties().getWithDefault ("danger", false);
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (! b.isEnabled() ? col::textFaint : on ? (danger ? col::danger : col::text) : (over ? col::text : col::textDim));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
}

juce::Font RoomXLookAndFeel::getTextButtonFont (juce::TextButton& b, int h)
{
    const float size = (float) b.getProperties().getWithDefault ("fontSize", juce::jmin (15.0f, (float) h * 0.55f));
    return Fonts::label (size);
}

void RoomXLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const auto box = juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ r.getX() + 9.0f, r.getCentreY() });
    g.setColour (col::well);
    g.fillRoundedRectangle (box, 3.0f);
    g.setColour (b.getToggleState() ? accent.current : (over ? col::lineHi : col::line));
    g.drawRoundedRectangle (box, 3.0f, 1.0f);
    if (b.getToggleState())
    {
        g.setColour (accent.current);
        g.fillRoundedRectangle (box.reduced (3.5f), 1.5f);
    }
    g.setColour (b.getToggleState() ? col::text : col::textDim);
    g.setFont (Fonts::text (14.0f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (22.0f), juce::Justification::centredLeft);
}

void RoomXLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (col::panel);
    g.setColour (col::lineHi);
    g.drawRect (0, 0, w, h, 1);
}

void RoomXLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                                          bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                                          const juce::String& shortcutKeyText, const juce::Drawable* icon, const juce::Colour* textColour)
{
    if (isSeparator)
    {
        g.setColour (col::line);
        g.fillRect (area.reduced (8, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }
    auto r = area.reduced (3, 1);
    if (isHighlighted && isActive)
    {
        g.setColour (accent.current.withAlpha (0.16f));
        g.fillRoundedRectangle (r.toFloat(), 4.0f);
    }
    if (isTicked)
    {
        g.setColour (accent.current);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ (float) r.getX() + 10.0f, (float) r.getCentreY() }));
    }
    g.setColour (textColour != nullptr ? *textColour : (isActive ? col::text : col::textFaint));
    g.setFont (getPopupMenuFont());
    g.drawFittedText (text, r.withTrimmedLeft (22).withTrimmedRight (hasSubMenu ? 22 : 8), juce::Justification::centredLeft, 1);
    if (hasSubMenu)
    {
        juce::Path p;
        const float ax = (float) r.getRight() - 12.0f, ay = (float) r.getCentreY();
        p.addTriangle (ax, ay - 4.0f, ax, ay + 4.0f, ax + 5.0f, ay);
        g.setColour (col::textDim);
        g.fillPath (p);
    }
    if (shortcutKeyText.isNotEmpty())
    {
        g.setColour (col::textFaint);
        g.drawText (shortcutKeyText, r.withTrimmedRight (8), juce::Justification::centredRight);
    }
    juce::ignoreUnused (icon);
}

void RoomXLookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& text)
{
    g.setColour (col::textFaint);
    g.setFont (Fonts::caps (11.5f));
    g.drawText (text.toUpperCase(), area.reduced (12, 0), juce::Justification::bottomLeft);
}

juce::Font RoomXLookAndFeel::getPopupMenuFont() { return Fonts::text (15.0f); }

void RoomXLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    g.setColour (col::panelHi);
    g.fillRoundedRectangle (0.0f, 0.0f, (float) w, (float) h, 4.0f);
    g.setColour (col::lineHi);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 4.0f, 1.0f);
    g.setColour (col::text);
    g.setFont (Fonts::text (14.0f));
    g.drawFittedText (text, juce::Rectangle<int> (w, h).reduced (8, 4), juce::Justification::centredLeft, 4);
}

juce::Rectangle<int> RoomXLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea)
{
    const int w = juce::jmin (320, juce::GlyphArrangement::getStringWidthInt (Fonts::text (14.0f), tipText) + 20);
    const int lines = 1 + juce::GlyphArrangement::getStringWidthInt (Fonts::text (14.0f), tipText) / 300;
    const int h = 10 + 18 * lines;
    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
        .constrainedWithin (parentArea);
}

juce::Font RoomXLookAndFeel::getLabelFont (juce::Label& l)
{
    return Fonts::text ((float) l.getProperties().getWithDefault ("fontSize", 15.0));
}

void RoomXLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (col::well);
    g.fillRoundedRectangle (0.0f, 0.0f, (float) w, (float) h, 5.0f);
}

void RoomXLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& te)
{
    g.setColour (te.hasKeyboardFocus (true) ? accent.current.withAlpha (0.8f) : col::line);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 5.0f, 1.0f);
}

void RoomXLookAndFeel::drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics& g, const juce::Path& path, juce::Image&)
{
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillPath (path, juce::AffineTransform::translation (0.0f, 3.0f));
    g.setColour (col::panel);
    g.fillPath (path);
    g.setColour (col::lineHi);
    g.strokePath (path, juce::PathStrokeType (1.0f));
}
} // namespace roomx::ui
