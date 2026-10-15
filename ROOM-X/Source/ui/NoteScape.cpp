#include "NoteScape.h"
#include "SpectrumView.h"
#include "../PluginProcessor.h"

namespace roomx::ui
{
namespace P = roomx::params;

static constexpr float kTwoPiF = juce::MathConstants<float>::twoPi;
static float sectorAngle (int pc) { return (float) pc * kTwoPiF / 12.0f; }   // 0 = topo, horário
static juce::Point<float> polar (juce::Point<float> c, float r, float a) { return { c.x + r * std::sin (a), c.y - r * std::cos (a) }; }

// =============================================================================
//  TemperDial
// =============================================================================
TemperDial::TemperDial (Context& c)
    : ctx (c), temper (c.apvts, P::temper), width (c.apvts, P::noteWidth)
{
    setTooltip (U ("Centro: arrastar = Tempero  ·  Anel exterior: arrastar = Note Width\n"
                   "Roda do rato = Tempero (Shift = Note Width)  ·  Duplo clique = repor"));
}

bool TemperDial::hitTest (int x, int y)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    return c.getDistanceFrom ({ (float) x, (float) y }) <= getWidth() * 0.5f;
}

TemperDial::Zone TemperDial::zoneAt (juce::Point<float> p) const
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const float r = getWidth() * 0.5f, d = c.getDistanceFrom (p);
    if (d <= r * 0.76f) return Zone::Temper;
    if (d <= r) return Zone::Width;
    return Zone::None;
}

void TemperDial::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const float r = b.getWidth() * 0.5f;
    const auto acc = ctx.accent.current;
    const float t = temper.get() / 100.0f;
    const float w = width.get() / 100.0f;

    // ---- anel exterior (Note Width): arco simétrico em torno da base
    {
        const float rr = r * 0.89f;
        juce::Path track;
        track.addCentredArc (c.x, c.y, rr, rr, 0.0f, juce::MathConstants<float>::pi - 2.6f, juce::MathConstants<float>::pi + 2.6f, true);
        g.setColour (col::line);
        g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float span = 0.06f + 2.54f * w;
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, rr, rr, 0.0f, juce::MathConstants<float>::pi - span, juce::MathConstants<float>::pi + span, true);
        g.setColour (col::silver.withAlpha (hover == Zone::Width || zone == Zone::Width ? 0.95f : 0.62f));
        g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        // marcas de escala
        for (int i = 0; i <= 10; ++i)
        {
            const float a = juce::MathConstants<float>::pi - 2.6f + 5.2f * (float) i / 10.0f;
            g.setColour (col::textFaint.withAlpha (0.6f));
            g.drawLine (juce::Line<float> (polar (c, rr + 5.0f, a), polar (c, rr + 8.0f, a)), 1.0f);
        }
    }

    // ---- disco central
    const float dr = r * 0.74f;
    const auto disc = juce::Rectangle<float> (dr * 2.0f, dr * 2.0f).withCentre (c);
    {
        juce::DropShadow (juce::Colours::black.withAlpha (0.65f), 18, { 0, 5 }).drawForPath (g, [&] { juce::Path p; p.addEllipse (disc); return p; }());
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff272c36), c.x, disc.getY(), juce::Colour (0xff0f1115), c.x, disc.getBottom(), false));
        g.fillEllipse (disc);
        g.setColour (juce::Colour (0xff3b4351));
        g.drawEllipse (disc.reduced (0.5f), 1.2f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawEllipse (disc.reduced (dr * 0.08f), 1.0f);
    }

    // ---- arco de Temper (bipolar a partir do topo)
    {
        const float ar = dr * 0.86f;
        juce::Path track;
        track.addCentredArc (c.x, c.y, ar, ar, 0.0f, -2.5f, 2.5f, true);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (std::abs (t) > 0.002f)
        {
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, ar, ar, 0.0f, juce::jmin (0.0f, 2.5f * t), juce::jmax (0.0f, 2.5f * t), true);
            g.setColour (acc.withAlpha (0.22f));
            g.strokePath (arc, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (acc);
            g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (col::silver);
        g.fillEllipse (juce::Rectangle<float> (4.5f, 4.5f).withCentre (polar (c, ar, 2.5f * t)));
        g.setColour (col::textFaint);
        g.fillRect (c.x - 0.5f, c.y - ar - 6.0f, 1.0f, 4.0f);
    }

    // ---- textos
    const bool hotT = hover == Zone::Temper || zone == Zone::Temper;
    auto tr = disc.reduced (dr * 0.12f);
    g.setColour (hotT ? col::text : col::textDim);
    g.setFont (Fonts::caps (dr * 0.175f, 0.12f));
    g.drawText ("TEMPERO", tr.removeFromTop (tr.getHeight() * 0.27f), juce::Justification::centredBottom);
    const int tv = juce::roundToInt (temper.get());
    g.setColour (col::text);
    g.setFont (Fonts::title (dr * 0.50f));
    g.drawText ((tv > 0 ? "+" : "") + juce::String (tv), tr.removeFromTop (tr.getHeight() * 0.44f), juce::Justification::centred);
    g.setColour (col::lineHi);
    g.fillRect (juce::Rectangle<float> (dr * 0.9f, 1.0f).withCentre ({ c.x, tr.getY() + 1.0f }));
    g.setColour (hover == Zone::Width || zone == Zone::Width ? col::text : col::textDim);
    g.setFont (Fonts::caps (dr * 0.14f, 0.08f));
    g.drawText ("NOTE WIDTH", tr.removeFromTop (tr.getHeight() * 0.40f), juce::Justification::centredBottom);
    g.setColour (col::text);
    g.setFont (Fonts::label (dr * 0.24f));
    g.drawText (juce::String (juce::roundToInt (width.get())) + "%", tr, juce::Justification::centredTop);
}

void TemperDial::mouseDown (const juce::MouseEvent& e)
{
    zone = zoneAt (e.position);
    if (zone == Zone::Temper) { temper.begin(); startValue = temper.get(); }
    if (zone == Zone::Width)  { width.begin();  startValue = width.get(); }
    repaint();
}

void TemperDial::mouseDrag (const juce::MouseEvent& e)
{
    const float k = e.mods.isShiftDown() ? 0.2f : 1.0f;
    const float dy = -(float) e.getDistanceFromDragStartY() + 0.35f * (float) e.getDistanceFromDragStartX();
    if (zone == Zone::Temper) temper.set (std::round ((startValue + dy * k * 200.0f / 260.0f) * 10.0f) / 10.0f);
    if (zone == Zone::Width)  width.set  (std::round ((startValue + dy * k * 98.0f / 260.0f) * 10.0f) / 10.0f);
    getParentComponent()->repaint();
}

void TemperDial::mouseUp (const juce::MouseEvent&)
{
    if (zone == Zone::Temper) temper.end();
    if (zone == Zone::Width)  width.end();
    zone = Zone::None;
    getParentComponent()->repaint();
}

void TemperDial::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto z = zoneAt (e.position);
    if (z == Zone::Temper) temper.setOnce (temper.defaultValue());
    if (z == Zone::Width)  width.setOnce (width.defaultValue());
    getParentComponent()->repaint();
}

void TemperDial::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& d)
{
    const float step = (d.deltaY != 0.0f ? d.deltaY : d.deltaX) * (d.isReversed ? -1.0f : 1.0f);
    if (e.mods.isShiftDown() || zoneAt (e.position) == Zone::Width)
        width.setOnce (width.get() + step * 12.0f);
    else
        temper.setOnce (temper.get() + step * 25.0f);
    getParentComponent()->repaint();
}

void TemperDial::mouseMove (const juce::MouseEvent& e)
{
    const auto z = zoneAt (e.position);
    setMouseCursor (z == Zone::None ? juce::MouseCursor::NormalCursor : juce::MouseCursor::UpDownResizeCursor);
    if (z != hover) { hover = z; repaint(); }
}

void TemperDial::mouseExit (const juce::MouseEvent&)
{
    hover = Zone::None;
    repaint();
}

// =============================================================================
//  NoteScape
// =============================================================================
NoteScape::NoteScape (Context& c)
    : dial (c), ctx (c), temper (c.apvts, P::temper), width (c.apvts, P::noteWidth),
      notes (c.apvts, P::notes), mode (c.apvts, P::noteMode)
{
    addAndMakeVisible (dial);
    for (auto& s : shown) s = 0.3f;
}

void NoteScape::resized()
{
    const auto b = getLocalBounds().toFloat();
    centre = b.getCentre();
    radius = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 2.0f;
    const float dr = radius * 0.50f;
    dial.setBounds (juce::Rectangle<float> (dr * 2.0f, dr * 2.0f).withCentre (centre).toNearestInt());
}

void NoteScape::tick()
{
    const uint16_t pcs = ctx.displayedPitchClasses();
    const float t = temper.get() / 100.0f;
    const float w = width.get() / 100.0f;
    const float lvl = ctx.analyzer.level();
    const float* meas = ctx.analyzer.pitchEnergy();

    ghost += ((dial.isDragging() ? 1.0f : 0.0f) - ghost) * 0.2f;

    float model[12];
    const float base = 0.34f + 0.18f * lvl;
    for (int i = 0; i < 12; ++i)
    {
        const bool sel = ((pcs >> i) & 1u) != 0;
        if (t >= 0.0f) model[i] = sel ? base * (1.0f + 1.55f * t) : base * (1.0f - 0.72f * t);
        else           model[i] = sel ? base * (1.0f - 0.92f * -t) : base * (1.0f + 0.22f * -t);
    }
    // Note Width larga: o realce derrama para os vizinhos
    if (t > 0.0f)
        for (int i = 0; i < 12; ++i)
            if (((pcs >> i) & 1u) == 0)
            {
                const bool nb = ((pcs >> ((i + 1) % 12)) & 1u) || ((pcs >> ((i + 11) % 12)) & 1u);
                if (nb) model[i] += 0.45f * w * w * t * base;
            }

    const float mw = lvl > 0.03f ? 0.5f : 0.0f;   // peso da medição real
    for (int i = 0; i < 12; ++i)
    {
        const float target = juce::jlimit (0.02f, 1.0f, (1.0f - mw) * model[i] + mw * (0.25f + 0.85f * meas[i]) * (model[i] / juce::jmax (0.05f, base)));
        shown[i] += (target - shown[i]) * 0.22f;
    }
    repaint();
}

void NoteScape::paint (juce::Graphics& g)
{
    const auto acc = ctx.accent.current;
    const uint16_t pcs = ctx.displayedPitchClasses();
    const float t = temper.get() / 100.0f;
    const bool flats = ctx.useFlats();
    const bool manual = (int) mode.get() == (int) P::NoteMode::Manual;

    const float rOuter = radius * 0.80f;          // fim dos segmentos
    const float rInner = radius * 0.545f;         // começo (fora do dial)
    const float rLabel = radius * 0.91f;

    // ---- fundo: halo radial + anéis
    {
        juce::ColourGradient halo (acc.withAlpha (0.07f + 0.05f * std::abs (t)), centre.x, centre.y,
                                   juce::Colours::transparentBlack, centre.x + radius, centre.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));
        g.setColour (col::line.withAlpha (0.7f));
        for (float rr : { rInner, (rInner + rOuter) * 0.5f, rOuter })
            g.drawEllipse (juce::Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (centre), 0.8f);
        g.setColour (col::line);
        g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre).reduced (1.0f), 1.0f);
    }

    // ---- sectores segmentados
    constexpr int kSeg = 9;
    const float half = kTwoPiF / 24.0f;
    const float gap  = 0.035f;
    const float segH = (rOuter - rInner) / (float) kSeg;
    for (int pc = 0; pc < 12; ++pc)
    {
        const float a  = sectorAngle (pc);
        const bool sel = ((pcs >> pc) & 1u) != 0;
        const float v  = juce::jlimit (0.0f, 1.0f, shown[pc]);
        const float lit = v * (float) kSeg;
        const bool hole = sel && t < -0.02f;

        for (int s = 0; s < kSeg; ++s)
        {
            const float r0 = rInner + segH * (float) s + 1.2f, r1 = r0 + segH - 2.4f;
            const float a0 = a - half + gap, a1 = a + half - gap;
            juce::Path seg;
            seg.addPieSegment (juce::Rectangle<float> (r1 * 2.0f, r1 * 2.0f).withCentre (centre), a0, a1, r0 / r1);
            const float on = juce::jlimit (0.0f, 1.0f, lit - (float) s);

            if (hole)
            {
                // Temper negativo: sector oco ("buraco")
                g.setColour (juce::Colours::black.withAlpha (0.55f));
                g.fillPath (seg);
                g.setColour (acc.withAlpha ((0.18f + 0.45f * -t) * (1.0f - 0.6f * (float) s / (float) kSeg)));
                g.strokePath (seg, juce::PathStrokeType (1.0f));
                continue;
            }

            g.setColour (col::well.brighter (0.05f));
            g.fillPath (seg);
            if (on > 0.0f)
            {
                const float depth = (float) s / (float) kSeg;
                auto c = sel ? acc.interpolatedWith (col::silver, 0.18f * depth) : acc;
                g.setColour (c.withAlpha (on * (sel ? 0.92f : 0.55f) * (0.55f + 0.45f * (1.0f - depth * 0.5f))));
                g.fillPath (seg);
            }
        }

        // marcador da nota seleccionada (arco prateado + entalhe)
        if (sel)
        {
            juce::Path mark;
            const float mr = rOuter + 6.0f;
            mark.addCentredArc (centre.x, centre.y, mr, mr, 0.0f, a - half + 0.05f, a + half - 0.05f, true);
            g.setColour ((hole ? acc : col::silver).withAlpha (0.95f));
            g.strokePath (mark, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            juce::Path notch;
            const auto tip = polar (centre, mr - 4.5f, a);
            notch.addTriangle (polar (centre, mr + 1.0f, a - 0.05f), polar (centre, mr + 1.0f, a + 0.05f), tip);
            g.fillPath (notch);
        }

        // nome da nota
        const auto lp = polar (centre, rLabel, a);
        const bool natural = music::pitchName (pc, false)[1] == '\0';
        const bool hot = pc == hoverNote && manual;
        auto box = juce::Rectangle<float> (36.0f, 22.0f).withCentre (lp);
        if (hot)
        {
            g.setColour (col::panelHi);
            g.fillRoundedRectangle (box, 6.0f);
        }
        g.setColour (sel ? col::silver : (natural ? col::textDim : col::textFaint));
        g.setFont (sel ? Fonts::title (natural ? 17.0f : 15.0f) : Fonts::label (natural ? 15.5f : 13.5f));
        g.drawText (juce::String::fromUTF8 (music::pitchName (pc, flats)), box, juce::Justification::centred);
    }

    // ---- contorno suave pelas pontas (Catmull-Rom)
    {
        juce::Point<float> pts[12];
        for (int i = 0; i < 12; ++i)
            pts[i] = polar (centre, rInner + juce::jlimit (0.0f, 1.0f, shown[i]) * (rOuter - rInner), sectorAngle (i));
        juce::Path contour;
        contour.startNewSubPath (pts[0]);
        for (int i = 0; i < 12; ++i)
        {
            const auto p0 = pts[(i + 11) % 12], p1 = pts[i], p2 = pts[(i + 1) % 12], p3 = pts[(i + 2) % 12];
            contour.cubicTo (p1 + (p2 - p0) / 6.0f, p2 - (p3 - p1) / 6.0f, p2);
        }
        contour.closeSubPath();
        g.setColour (acc.withAlpha (0.10f + 0.12f * ghost));
        g.strokePath (contour, juce::PathStrokeType (7.0f));
        g.setColour (acc.withAlpha (0.65f + 0.3f * ghost));
        g.strokePath (contour, juce::PathStrokeType (1.5f));
    }
}

int NoteScape::noteAt (juce::Point<float> p) const
{
    const float d = centre.getDistanceFrom (p);
    if (d < radius * 0.545f || d > radius) return -1;
    float a = std::atan2 (p.x - centre.x, centre.y - p.y);
    if (a < 0.0f) a += kTwoPiF;
    return juce::roundToInt (a / (kTwoPiF / 12.0f)) % 12;
}

void NoteScape::toggleNote (int pc, bool on)
{
    const int m = (int) notes.get();
    const int nm = on ? (m | (1 << pc)) : (m & ~(1 << pc));
    if (nm != m) notes.set ((float) nm);
}

void NoteScape::mouseDown (const juce::MouseEvent& e)
{
    const int pc = noteAt (e.position);
    if (pc < 0 || (int) mode.get() != (int) P::NoteMode::Manual) return;
    paintValue = ((((int) notes.get()) >> pc) & 1) == 0;
    lastPainted = pc;
    notes.begin();
    toggleNote (pc, paintValue);
    notes.end();
    repaint();
}

void NoteScape::mouseDrag (const juce::MouseEvent& e)
{
    const int pc = noteAt (e.position);
    if (pc < 0 || pc == lastPainted || lastPainted < 0) return;
    lastPainted = pc;
    notes.begin();
    toggleNote (pc, paintValue);
    notes.end();
    repaint();
}

void NoteScape::mouseMove (const juce::MouseEvent& e)
{
    const int pc = noteAt (e.position);
    const bool manual = (int) mode.get() == (int) P::NoteMode::Manual;
    setMouseCursor (pc >= 0 && manual ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    if (pc != hoverNote) { hoverNote = pc; repaint(); }
}

void NoteScape::mouseExit (const juce::MouseEvent&)
{
    hoverNote = -1;
    lastPainted = -1;
    repaint();
}
} // namespace roomx::ui
