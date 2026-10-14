#include "Widgets.h"

namespace roomx::ui
{
// =============================================================================
//  Knob
// =============================================================================
Knob::Knob (APVTS& s, const juce::String& paramId, const juce::String& t, bool bipolar)
    : title (t), param (s.getParameter (paramId))
{
    jassert (param != nullptr);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
    slider.setMouseDragSensitivity (240);
    slider.setVelocityModeParameters (0.4, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
    slider.getProperties().set ("bipolar", bipolar);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    addAndMakeVisible (slider);
    attachment = std::make_unique<APVTS::SliderAttachment> (s, paramId, slider);
    slider.setDoubleClickReturnValue (true, (double) param->convertFrom0to1 (param->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };

    valueLabel.setJustificationType (juce::Justification::centred);
    valueLabel.setEditable (false, true, false);
    valueLabel.setFont (Fonts::label (15.0f));
    valueLabel.setColour (juce::Label::textColourId, col::text);
    valueLabel.setColour (juce::Label::textWhenEditingColourId, col::text);
    valueLabel.setColour (juce::Label::backgroundWhenEditingColourId, col::well);
    valueLabel.setColour (juce::Label::outlineWhenEditingColourId, col::lineHi);
    valueLabel.setTooltip (U ("Duplo clique para escrever um valor"));
    valueLabel.onTextChange = [this]
    {
        auto txt = valueLabel.getText().trim().retainCharacters ("0123456789.-+");
        if (txt.isNotEmpty())
        {
            const float v = param->getNormalisableRange().snapToLegalValue (txt.getFloatValue());
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 (v));
            param->endChangeGesture();
        }
        repaint();
    };
    addAndMakeVisible (valueLabel);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (titleHeight);
    valueLabel.setBounds (r.removeFromBottom (valueHeight));
    const int d = juce::jmin (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (d, d));
}

void Knob::paint (juce::Graphics& g)
{
    g.setColour (col::textDim);
    g.setFont (Fonts::caps (13.0f, 0.08f));
    g.drawText (title, getLocalBounds().removeFromTop (titleHeight), juce::Justification::centred);

    const auto txt = valueText ? valueText (slider.getValue()) : param->getCurrentValueAsText();
    if (! valueLabel.isBeingEdited() && valueLabel.getText() != txt)
        valueLabel.setText (txt, juce::dontSendNotification);
}

// =============================================================================
//  Fader com medidor estéreo
// =============================================================================
Fader::Fader (APVTS& s, const juce::String& paramId, const juce::String& t)
    : title (t), param (s.getParameter (paramId))
{
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setVelocityModeParameters (0.4, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
    addAndMakeVisible (slider);
    attachment = std::make_unique<APVTS::SliderAttachment> (s, paramId, slider);
    slider.setDoubleClickReturnValue (true, 0.0);
    slider.onValueChange = [this] { repaint(); };
}

void Fader::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom (40);
    r.removeFromTop (6);
    auto meter = r.removeFromRight (r.getWidth() / 2);
    slider.setBounds (r);
    meterArea = meter.withSizeKeepingCentre (14, meter.getHeight()).toFloat().reduced (0, 6);
}

void Fader::setLevels (float l, float r)
{
    const float in[2] = { l, r };
    for (int c = 0; c < 2; ++c)
    {
        const float db = juce::Decibels::gainToDecibels (in[c], -70.0f);
        const float norm = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);    // -60 .. +6 dBFS
        level[c] = norm > level[c] ? norm : level[c] * 0.88f + norm * 0.12f;
        if (norm >= hold[c]) { hold[c] = norm; holdTicks[c] = 45; }
        else if (--holdTicks[c] <= 0) hold[c] = juce::jmax (0.0f, hold[c] - 0.01f);
        clip[c] = db > -0.1f ? 60 : juce::jmax (0, clip[c] - 1);
    }
    repaint (meterArea.toNearestInt().expanded (2));
}

void Fader::paint (juce::Graphics& g)
{
    // medidor
    const float bw = 5.0f;
    for (int c = 0; c < 2; ++c)
    {
        auto bar = juce::Rectangle<float> (meterArea.getX() + (float) c * (bw + 3.0f), meterArea.getY(), bw, meterArea.getHeight());
        g.setColour (col::well);
        g.fillRoundedRectangle (bar, 1.5f);
        const float hgt = bar.getHeight() * level[c];
        auto fill = bar.withTop (bar.getBottom() - hgt);
        juce::ColourGradient grad (col::silver.withAlpha (0.55f), 0, bar.getBottom(), col::silver.withAlpha (0.95f), 0, bar.getY(), false);
        grad.addColour (0.88, col::warn);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fill, 1.5f);
        const float hy = bar.getBottom() - bar.getHeight() * hold[c];
        if (hold[c] > 0.01f)
        {
            g.setColour (col::text);
            g.fillRect (bar.getX(), hy - 1.0f, bw, 1.5f);
        }
        if (clip[c] > 0)
        {
            g.setColour (col::danger);
            g.fillRect (bar.getX(), bar.getY() - 5.0f, bw, 3.0f);
        }
    }

    auto r = getLocalBounds().removeFromBottom (40);
    g.setColour (col::textDim);
    g.setFont (Fonts::caps (13.0f, 0.08f));
    g.drawText (title, r.removeFromTop (18), juce::Justification::centred);
    g.setColour (col::text);
    g.setFont (Fonts::label (15.0f));
    g.drawText (param->getCurrentValueAsText(), r, juce::Justification::centred);
}

// =============================================================================
//  Ícones (desenhados em código — originais ROOM-X)
// =============================================================================
IconButton::IconButton (Icon i, const juce::String& tip) : juce::Button (tip), icon (i)
{
    setTooltip (tip);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void IconButton::drawIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> r, juce::Colour c, float stroke)
{
    g.setColour (c);
    const auto st = juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    const float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
    const auto cx = r.getCentreX(), cy = r.getCentreY();
    juce::Path p;

    switch (icon)
    {
        case Icon::Undo:
        case Icon::Redo:
        {
            const bool redo = icon == Icon::Redo;
            p.addCentredArc (cx, cy + h * 0.06f, w * 0.34f, h * 0.30f, 0.0f,
                             redo ? -2.3f : 2.3f, redo ? 1.2f : -1.2f, true);
            g.strokePath (p, st);
            const float ax = redo ? cx + w * 0.30f : cx - w * 0.30f, ay = cy - h * 0.20f;
            juce::Path a;
            a.addTriangle (ax, ay - h * 0.14f, ax, ay + h * 0.14f, ax + (redo ? w * 0.16f : -w * 0.16f), ay);
            a.applyTransform (juce::AffineTransform::rotation (redo ? 0.5f : -0.5f, ax, ay));
            g.fillPath (a);
            break;
        }
        case Icon::Prev:
        case Icon::Next:
        {
            const bool next = icon == Icon::Next;
            p.startNewSubPath (next ? cx - w * 0.12f : cx + w * 0.12f, cy - h * 0.24f);
            p.lineTo (next ? cx + w * 0.14f : cx - w * 0.14f, cy);
            p.lineTo (next ? cx - w * 0.12f : cx + w * 0.12f, cy + h * 0.24f);
            g.strokePath (p, juce::PathStrokeType (stroke * 1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            break;
        }
        case Icon::Save:
        {
            p.startNewSubPath (cx, y + h * 0.18f);
            p.lineTo (cx, y + h * 0.62f);
            p.startNewSubPath (cx - w * 0.17f, y + h * 0.46f);
            p.lineTo (cx, y + h * 0.63f);
            p.lineTo (cx + w * 0.17f, y + h * 0.46f);
            p.startNewSubPath (x + w * 0.22f, y + h * 0.66f);
            p.lineTo (x + w * 0.22f, y + h * 0.80f);
            p.lineTo (x + w * 0.78f, y + h * 0.80f);
            p.lineTo (x + w * 0.78f, y + h * 0.66f);
            g.strokePath (p, st);
            break;
        }
        case Icon::Info:
        {
            g.drawEllipse (r.reduced (w * 0.18f), stroke);
            g.fillEllipse (juce::Rectangle<float> (2.2f, 2.2f).withCentre ({ cx, cy - h * 0.14f }));
            g.fillRoundedRectangle (cx - 1.0f, cy - h * 0.04f, 2.0f, h * 0.22f, 1.0f);
            break;
        }
        case Icon::Leaf:
        {
            p.startNewSubPath (x + w * 0.24f, y + h * 0.78f);
            p.cubicTo (x + w * 0.20f, y + h * 0.36f, x + w * 0.46f, y + h * 0.22f, x + w * 0.80f, y + h * 0.20f);
            p.cubicTo (x + w * 0.80f, y + h * 0.56f, x + w * 0.62f, y + h * 0.78f, x + w * 0.26f, y + h * 0.76f);
            p.startNewSubPath (x + w * 0.24f, y + h * 0.78f);
            p.lineTo (x + w * 0.58f, y + h * 0.44f);
            g.strokePath (p, st);
            break;
        }
        case Icon::Lock:
        case Icon::Unlock:
        {
            const auto body = juce::Rectangle<float> (w * 0.46f, h * 0.36f).withCentre ({ cx, cy + h * 0.12f });
            g.fillRoundedRectangle (body, 2.0f);
            const float sx = body.getX() + w * 0.08f, ex = body.getRight() - w * 0.08f;
            p.startNewSubPath (sx, body.getY());
            p.lineTo (sx, cy - h * 0.12f);
            p.quadraticTo (sx, y + h * 0.18f, cx, y + h * 0.18f);
            p.quadraticTo (ex, y + h * 0.18f, ex, cy - h * 0.12f);
            if (icon == Icon::Lock) p.lineTo (ex, body.getY());
            g.strokePath (p, st);
            break;
        }
        case Icon::Fork:   // diapasão (afinação)
        {
            p.startNewSubPath (cx - w * 0.14f, y + h * 0.16f);
            p.lineTo (cx - w * 0.14f, cy);
            p.quadraticTo (cx - w * 0.14f, cy + h * 0.14f, cx, cy + h * 0.14f);
            p.quadraticTo (cx + w * 0.14f, cy + h * 0.14f, cx + w * 0.14f, cy);
            p.lineTo (cx + w * 0.14f, y + h * 0.16f);
            p.startNewSubPath (cx, cy + h * 0.14f);
            p.lineTo (cx, y + h * 0.86f);
            g.strokePath (p, st);
            break;
        }
        case Icon::Retrigger:
        {
            p.addCentredArc (cx, cy, w * 0.28f, h * 0.28f, 0.0f, 0.6f, 5.6f, true);
            g.strokePath (p, st);
            const float ax = cx + w * 0.28f * std::sin (0.6f), ay = cy - h * 0.28f * std::cos (0.6f);
            juce::Path a;
            a.addTriangle (ax - 4.0f, ay - 3.5f, ax + 2.5f, ay - 4.0f, ax, ay + 2.5f);
            g.fillPath (a);
            break;
        }
        case Icon::Play:
            p.addTriangle (cx - w * 0.16f, cy - h * 0.22f, cx - w * 0.16f, cy + h * 0.22f, cx + w * 0.22f, cy);
            g.fillPath (p);
            break;
        case Icon::Minus:
            g.fillRoundedRectangle (cx - w * 0.22f, cy - stroke * 0.5f, w * 0.44f, stroke, stroke * 0.5f);
            break;
        case Icon::Plus:
            g.fillRoundedRectangle (cx - w * 0.22f, cy - stroke * 0.5f, w * 0.44f, stroke, stroke * 0.5f);
            g.fillRoundedRectangle (cx - stroke * 0.5f, cy - h * 0.22f, stroke, h * 0.44f, stroke * 0.5f);
            break;
        case Icon::Menu:
            p.addTriangle (cx - w * 0.16f, cy - h * 0.07f, cx + w * 0.16f, cy - h * 0.07f, cx, cy + h * 0.12f);
            g.fillPath (p);
            break;
        case Icon::Power:
            p.addCentredArc (cx, cy + h * 0.03f, w * 0.27f, h * 0.27f, 0.0f, 0.65f, 5.63f, true);
            p.startNewSubPath (cx, y + h * 0.16f);
            p.lineTo (cx, cy);
            g.strokePath (p, st);
            break;
        case Icon::Copy:
        {
            const auto a = juce::Rectangle<float> (w * 0.40f, h * 0.44f);
            g.drawRoundedRectangle (a.withPosition (x + w * 0.20f, y + h * 0.18f), 2.0f, stroke);
            g.drawRoundedRectangle (a.withPosition (x + w * 0.38f, y + h * 0.38f), 2.0f, stroke);
            break;
        }
        case Icon::Folder:
            p.startNewSubPath (x + w * 0.18f, y + h * 0.30f);
            p.lineTo (x + w * 0.40f, y + h * 0.30f);
            p.lineTo (x + w * 0.46f, y + h * 0.38f);
            p.lineTo (x + w * 0.82f, y + h * 0.38f);
            p.lineTo (x + w * 0.82f, y + h * 0.74f);
            p.lineTo (x + w * 0.18f, y + h * 0.74f);
            p.closeSubPath();
            g.strokePath (p, st);
            break;
    }
}

void IconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat();
    const bool on = getToggleState();
    const auto acc = ! onColour.isTransparent() ? onColour : (accent != nullptr ? accent->current : col::all);
    if (drawBackground)
    {
        g.setColour (over ? col::panelHi.brighter (0.06f) : col::panelHi);
        g.fillEllipse (r.reduced (1.0f));
        g.setColour (on ? acc.withAlpha (0.7f) : col::line);
        g.drawEllipse (r.reduced (1.0f), 1.0f);
    }
    const auto c = ! isEnabled() ? col::textFaint : on ? acc : (over || down ? col::text : col::textDim);
    drawIcon (g, icon, r.reduced (r.getWidth() * 0.18f), c, 1.6f);
    if (showDot)
    {
        g.setColour (col::warn);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ r.getRight() - 5.0f, r.getY() + 5.0f }));
    }
}

// =============================================================================
ParamToggle::ParamToggle (APVTS& s, const juce::String& paramId, const juce::String& text)
    : juce::TextButton (text)
{
    setClickingTogglesState (true);
    getProperties().set ("fontSize", 14.0);
    attachment = std::make_unique<APVTS::ButtonAttachment> (s, paramId, *this);
}

// =============================================================================
//  Selector segmentado
// =============================================================================
SegmentedChoice::SegmentedChoice (APVTS& s, const juce::String& paramId, juce::StringArray l, bool v)
    : labels (std::move (l)), vertical (v), param (s.getParameter (paramId))
{
    attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float value)
    {
        const int i = juce::roundToInt (value);
        if (i != index)
        {
            index = i;
            if (onChange) onChange (index);
            repaint();
        }
    }, s.undoManager);
    attachment->sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

juce::Rectangle<float> SegmentedChoice::segment (int i) const
{
    auto r = getLocalBounds().toFloat();
    if (caption.isNotEmpty()) r.removeFromTop (20.0f);
    const int n = labels.size();
    if (vertical)
    {
        const float h = r.getHeight() / (float) n;
        return { r.getX(), r.getY() + h * (float) i, r.getWidth(), h };
    }
    const float w = r.getWidth() / (float) n;
    return { r.getX() + w * (float) i, r.getY(), w, r.getHeight() };
}

void SegmentedChoice::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    if (caption.isNotEmpty())
    {
        g.setColour (col::textDim);
        g.setFont (Fonts::caps (13.0f, 0.08f));
        g.drawText (caption, r.removeFromTop (20.0f), juce::Justification::centred);
    }

    g.setColour (col::well);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (col::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);

    for (int i = 0; i < labels.size(); ++i)
    {
        auto s = segment (i).reduced (3.0f);
        const bool on = i == index;
        const auto c = colourForIndex ? colourForIndex (i) : col::text;
        if (on)
        {
            g.setColour (c.withAlpha (0.14f));
            g.fillRoundedRectangle (s, 5.0f);
            g.setColour (c.withAlpha (0.75f));
            g.drawRoundedRectangle (s, 5.0f, 1.0f);
        }
        else if (i == hover)
        {
            g.setColour (col::panelHi);
            g.fillRoundedRectangle (s, 5.0f);
        }

        // ponto indicador (cor própria do segmento)
        if (colourForIndex)
        {
            const auto dot = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ s.getX() + 13.0f, s.getCentreY() });
            g.setColour (on ? c : c.withAlpha (0.28f));
            g.fillEllipse (dot);
            if (on)
            {
                g.setColour (c.withAlpha (0.3f));
                g.drawEllipse (dot.expanded (3.0f), 1.0f);
            }
            s.removeFromLeft (24.0f);
        }
        g.setColour (on ? col::text : (i == hover ? col::text.withAlpha (0.8f) : col::textDim));
        g.setFont (Fonts::label (14.5f));
        g.drawText (labels[i], s, colourForIndex ? juce::Justification::centredLeft : juce::Justification::centred);
    }
}

void SegmentedChoice::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < labels.size(); ++i)
        if (segment (i).contains (e.position))
        {
            attachment->setValueAsCompleteGesture ((float) i);
            return;
        }
}

void SegmentedChoice::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    for (int i = 0; i < labels.size(); ++i)
        if (segment (i).contains (e.position)) h = i;
    if (h != hover) { hover = h; repaint(); }
}

void SegmentedChoice::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}
} // namespace roomx::ui
