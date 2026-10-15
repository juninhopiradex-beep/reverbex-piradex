#include "SpectrumView.h"
#include "../PluginProcessor.h"
#include "../dsp/ModalBank.h"

namespace roomx::ui
{
namespace P = roomx::params;

// =============================================================================
//  WetAnalyzer
// =============================================================================
WetAnalyzer::WetAnalyzer()
{
    std::fill (std::begin (display), std::end (display), -140.0f);
}

bool WetAnalyzer::update (RoomXProcessor& p, float lo, float hi, const music::Tuning& tuning)
{
    const double sr = p.getCurrentSampleRate();
    if (sr != rate)
    {
        rate = sr;
        std::fill (ring.begin(), ring.end(), 0.0f);
        ringPos = 0;
    }

    // ---- recolhe o que o áudio deixou no FIFO (descarta se a UI estiver desligada)
    const int avail = p.analysisFifo.getNumReady();
    if (avail > 0)
    {
        int s1, z1, s2, z2;
        p.analysisFifo.prepareToRead (avail, s1, z1, s2, z2);
        auto push = [this, &p] (int start, int count)
        {
            for (int i = 0; i < count; ++i)
            {
                ring[(size_t) ringPos] = p.analysisBuffer[(size_t) (start + i)];
                ringPos = (ringPos + 1) & (kSize - 1);
            }
        };
        if (enabled) { push (s1, z1); push (s2, z2); }
        p.analysisFifo.finishedRead (z1 + z2);
        newSamples += avail;
    }

    if (! enabled) return false;

    // ---- sem áudio novo (transporte parado): deixa cair suavemente
    const int hop = eco ? kSize / 3 : kSize / 8;
    if (newSamples < hop)
    {
        if (avail == 0)
        {
            bool changed = false;
            for (auto& d : display) if (d > -140.0f) { d = juce::jmax (-140.0f, d - 1.2f); changed = true; }
            for (auto& e : pcEnergy) { e *= 0.92f; }
            overall *= 0.92f;
            return changed;
        }
        return false;
    }
    newSamples = 0;

    // ---- FFT
    for (int i = 0; i < kSize; ++i)
        work[(size_t) i] = ring[(size_t) ((ringPos + i) & (kSize - 1))];
    std::fill (work.begin() + kSize, work.end(), 0.0f);
    window.multiplyWithWindowingTable (work.data(), (size_t) kSize);
    fft.performFrequencyOnlyForwardTransform (work.data(), true);

    const float binHz = (float) (rate / kSize);
    const float norm  = 4.0f / (float) kSize;             // Hann (ganho coerente 0.5) + espectro unilateral
    const int   nBins = kSize / 2;
    auto power = [&] (int k) { const float m = work[(size_t) k] * norm; return m * m; };

    const float halfStep = 0.5f * std::log2 (1000.0f) / (float) (kPoints - 1);
    for (int i = 0; i < kPoints; ++i)
    {
        const float f  = pointToHz (i);
        const float f0 = f * std::exp2 (-halfStep), f1 = f * std::exp2 (halfStep);
        int k0 = (int) std::floor (f0 / binHz), k1 = (int) std::ceil (f1 / binHz);
        float pw;
        if (k1 - k0 <= 1)
        {
            const float kf = f / binHz;
            const int k = juce::jlimit (1, nBins - 2, (int) kf);
            const float fr = kf - (float) k;
            pw = power (k) * (1.0f - fr) + power (k + 1) * fr;
        }
        else
        {
            k0 = juce::jlimit (1, nBins - 1, k0); k1 = juce::jlimit (k0 + 1, nBins, k1);
            pw = 0.0f;
            for (int k = k0; k < k1; ++k) pw += power (k);
            pw /= (float) (k1 - k0);
        }
        float db = 10.0f * std::log10 (pw + 1.0e-14f) + 4.5f * std::log2 (f / 1000.0f);   // inclinação "rosa"
        if (f > 0.49f * (float) rate) db = -140.0f;
        float& d = display[i];
        d = db > d ? d + 0.65f * (db - d) : d + 0.22f * (db - d);
    }

    // ---- energia por classe de altura (dentro do Temper Range)
    float pcs[12] {};
    float total = 0.0f;
    const int kLo = juce::jmax (1, (int) (90.0f / binHz)), kHi = juce::jmin (nBins - 1, (int) (9000.0f / binHz));
    for (int k = kLo; k <= kHi; ++k)
    {
        const float pw = power (k);
        total += pw;
        const double note = tuning.hzToNote ((double) k * binHz);
        if (note < lo - 0.5 || note > hi + 0.5) continue;
        const double rn = std::round (note);
        const float w = (float) juce::jmax (0.0, 1.0 - 2.0 * std::abs (note - rn));
        pcs[((int) rn % 12 + 12) % 12] += pw * w;
    }
    float maxDb = -200.0f, pcDb[12];
    for (int i = 0; i < 12; ++i) { pcDb[i] = 10.0f * std::log10 (pcs[i] + 1.0e-14f); maxDb = juce::jmax (maxDb, pcDb[i]); }
    const float totalDb = 10.0f * std::log10 (total + 1.0e-14f);
    const float lvl = juce::jlimit (0.0f, 1.0f, (totalDb + 75.0f) / 55.0f);
    overall += 0.35f * (lvl - overall);
    for (int i = 0; i < 12; ++i)
    {
        const float e = maxDb < -110.0f ? 0.0f : juce::jlimit (0.0f, 1.0f, (pcDb[i] - (maxDb - 22.0f)) / 22.0f) * lvl;
        pcEnergy[i] += (e > pcEnergy[i] ? 0.5f : 0.15f) * (e - pcEnergy[i]);
    }
    return true;
}

// =============================================================================
//  SpectrumView
// =============================================================================
SpectrumView::SpectrumView (Context& c)
    : ctx (c),
      rangeLow (c.apvts, P::rangeLow), rangeHigh (c.apvts, P::rangeHigh),
      lowCut (c.apvts, P::lowCut), highCut (c.apvts, P::highCut),
      lowSlope (c.apvts, P::lowCutSlope), highSlope (c.apvts, P::highCutSlope),
      temper (c.apvts, P::temper), noteWidth (c.apvts, P::noteWidth)
{
    setRepaintsOnMouseActivity (false);
}

void SpectrumView::resized()
{
    auto r = getLocalBounds().toFloat();
    lowBtn  = r.removeFromLeft (64.0f).reduced (0.0f, 2.0f);
    highBtn = r.removeFromRight (64.0f).reduced (0.0f, 2.0f);
    plot = r.reduced (10.0f, 2.0f);
}

float SpectrumView::xForHz (float hz) const
{
    return plot.getX() + plot.getWidth() * std::log (juce::jlimit (20.0f, 20000.0f, hz) / 20.0f) / std::log (1000.0f);
}
float SpectrumView::hzForX (float x) const
{
    return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, (x - plot.getX()) / plot.getWidth()));
}
float SpectrumView::xForNote (float note) const { return xForHz ((float) ctx.tuning().noteToHz (note)); }
float SpectrumView::noteForX (float x) const    { return (float) ctx.tuning().hzToNote (hzForX (x)); }
float SpectrumView::yForDb (float db) const
{
    const float t = juce::jlimit (0.0f, 1.0f, (db + 105.0f) / 95.0f);
    return plot.getBottom() - 4.0f - t * (plot.getHeight() - 22.0f);
}

void SpectrumView::drawFilterButton (juce::Graphics& g, juce::Rectangle<float> r, bool high)
{
    const bool steep = (high ? highSlope : lowSlope).get() > 0.5f;
    const bool active = high ? highCut.get() < 19900.0f : lowCut.get() > 20.5f;
    drawPanel (g, r, 8.0f);
    auto icon = r.reduced (14.0f, 0.0f).withHeight (r.getHeight() * 0.42f).withY (r.getY() + r.getHeight() * 0.16f);
    juce::Path p;
    if (! high)
    {
        p.startNewSubPath (icon.getX(), icon.getBottom());
        p.quadraticTo (icon.getX() + icon.getWidth() * (steep ? 0.18f : 0.32f), icon.getY(), icon.getX() + icon.getWidth() * 0.55f, icon.getY());
        p.lineTo (icon.getRight(), icon.getY());
    }
    else
    {
        p.startNewSubPath (icon.getX(), icon.getY());
        p.lineTo (icon.getX() + icon.getWidth() * 0.45f, icon.getY());
        p.quadraticTo (icon.getRight() - icon.getWidth() * (steep ? 0.18f : 0.32f), icon.getY(), icon.getRight(), icon.getBottom());
    }
    g.setColour (active ? ctx.accent.current : col::textDim);
    g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (col::textDim);
    g.setFont (Fonts::label (13.0f));
    g.drawText (steep ? "36 dB" : "12 dB", r.withTrimmedTop (r.getHeight() * 0.58f), juce::Justification::centredTop);
}

juce::String SpectrumView::describe (Drag d) const
{
    const bool flats = ctx.useFlats();
    auto noteTxt = [&] (float n)
    {
        return juce::String (music::noteName (juce::roundToInt (n), flats)) + U ("  ·  ") + P::formatHz ((float) ctx.tuning().noteToHz (n));
    };
    switch (d)
    {
        case Drag::RangeLow:  return U ("Tempero Range (grave): ") + noteTxt (rangeLow.get());
        case Drag::RangeHigh: return U ("Tempero Range (agudo): ") + noteTxt (rangeHigh.get());
        case Drag::RangeBoth: return U ("Tempero Range: ") + music::noteName (juce::roundToInt (rangeLow.get()), flats) + " – "
                                   + music::noteName (juce::roundToInt (rangeHigh.get()), flats);
        case Drag::LowCut:    return "Low Cut: " + P::formatHz (lowCut.get());
        case Drag::HighCut:   return "High Cut: " + P::formatHz (highCut.get());
        case Drag::CutBoth:   return "Wet: " + P::formatHz (lowCut.get()) + U (" – ") + P::formatHz (highCut.get());
        case Drag::None:      break;
    }
    return {};
}

void SpectrumView::paint (juce::Graphics& g)
{
    const auto acc = ctx.accent.current;
    drawFilterButton (g, lowBtn, false);
    drawFilterButton (g, highBtn, true);

    auto well = plot.expanded (6.0f, 2.0f);
    g.setColour (col::well);
    g.fillRoundedRectangle (well, 8.0f);
    g.setColour (col::line);
    g.drawRoundedRectangle (well.reduced (0.5f), 8.0f, 1.0f);

    g.saveState();
    g.reduceClipRegion (plot.toNearestInt());

    // ---- grelha
    g.setFont (Fonts::text (11.5f));
    for (float f : { 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f })
    {
        const float x = xForHz (f);
        g.setColour (col::line.withAlpha (0.55f));
        g.fillRect (x, plot.getY() + 18.0f, 1.0f, plot.getHeight() - 18.0f);
        g.setColour (col::textFaint);
        g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f),
                    juce::Rectangle<float> (x + 3.0f, plot.getBottom() - 15.0f, 30.0f, 13.0f), juce::Justification::centredLeft);
    }
    // oitavas (C1..C9) no topo
    for (int oct = 1; oct <= 9; ++oct)
    {
        const float x = xForNote ((float) (12 * (oct + 1)));
        g.setColour (col::textFaint.withAlpha (0.8f));
        g.fillRect (x, plot.getY() + 14.0f, 1.0f, 4.0f);
        g.drawText ("C" + juce::String (oct), juce::Rectangle<float> (x - 12.0f, plot.getY() + 1.0f, 24.0f, 12.0f), juce::Justification::centred);
    }

    // ---- espectro
    const float* curve = ctx.analyzer.curve();
    juce::Path line, fill;
    for (int i = 0; i < WetAnalyzer::kPoints; ++i)
    {
        const float x = plot.getX() + plot.getWidth() * (float) i / (float) (WetAnalyzer::kPoints - 1);
        const float y = yForDb (curve[i]);
        if (i == 0) line.startNewSubPath (x, y); else line.lineTo (x, y);
    }
    fill = line;
    fill.lineTo (plot.getRight(), plot.getBottom());
    fill.lineTo (plot.getX(), plot.getBottom());
    fill.closeSubPath();

    const float lo = juce::jmin (rangeLow.get(), rangeHigh.get());
    const float hi = juce::jmax (rangeLow.get(), rangeHigh.get());
    const float xLo = xForNote (lo - 0.5f), xHi = xForNote (hi + 0.5f);

    // fora do range: neutro/cinzento
    {
        g.saveState();
        juce::RectangleList<int> outside;
        outside.addWithoutMerging (plot.withRight (xLo).toNearestInt());
        outside.addWithoutMerging (plot.withLeft (xHi).toNearestInt());
        g.reduceClipRegion (outside);
        g.setGradientFill (juce::ColourGradient (col::textDim.withAlpha (0.30f), 0, plot.getY(), col::textDim.withAlpha (0.03f), 0, plot.getBottom(), false));
        g.fillPath (fill);
        g.setColour (col::textDim.withAlpha (0.55f));
        g.strokePath (line, juce::PathStrokeType (1.2f));
        g.restoreState();
    }
    // dentro do range: acento
    {
        g.saveState();
        g.reduceClipRegion (plot.withLeft (xLo).withRight (xHi).toNearestInt());
        g.setGradientFill (juce::ColourGradient (acc.withAlpha (0.45f), 0, plot.getY() + 10.0f, acc.withAlpha (0.04f), 0, plot.getBottom(), false));
        g.fillPath (fill);
        g.setColour (acc.withAlpha (0.95f));
        g.strokePath (line, juce::PathStrokeType (1.4f));
        g.restoreState();
    }

    // ---- bandas das notas seleccionadas (quando há Temper)
    const float t = temper.get() / 100.0f;
    if (std::abs (t) > 0.01f)
    {
        const uint16_t pcs = ctx.displayedPitchClasses();
        const float bw = juce::jmax (0.18f, dsp::ModalBank::bandwidthSemitones (noteWidth.get() / 100.0f));
        for (int n = juce::roundToInt (std::ceil (lo)); n <= (int) hi; ++n)
        {
            if (((pcs >> (n % 12)) & 1u) == 0) continue;
            const float x0 = xForNote ((float) n - bw * 0.5f), x1 = xForNote ((float) n + bw * 0.5f);
            const auto band = juce::Rectangle<float> (x0, plot.getY() + 18.0f, juce::jmax (1.0f, x1 - x0), plot.getHeight() - 18.0f);
            if (t > 0.0f)
            {
                g.setColour (acc.withAlpha (0.05f + 0.10f * t));
                g.fillRect (band);
                g.setColour (acc.withAlpha (0.25f * t));
                g.fillRect (band.withX (band.getCentreX() - 0.5f).withWidth (1.0f));
            }
            else
            {
                g.setColour (juce::Colours::black.withAlpha (0.25f + 0.35f * -t));
                g.fillRect (band);
                g.setColour (acc.withAlpha (0.35f * -t));
                g.fillRect (band.withWidth (1.0f));
            }
        }
    }

    // ---- filtros wet: curva de resposta e escurecimento da zona cortada
    {
        const float lc = lowCut.get(), hc = highCut.get();
        const int nL = lowSlope.get() > 0.5f ? 6 : 2, nH = highSlope.get() > 0.5f ? 6 : 2;
        juce::Path resp, shade;
        const float top = plot.getY() + 18.0f, bottom = plot.getBottom() - 2.0f;
        auto yResp = [&] (float db) { return top + juce::jlimit (0.0f, 1.0f, -db / 48.0f) * (bottom - top); };
        shade.startNewSubPath (plot.getX(), plot.getY());
        bool penDown = false;
        for (float x = plot.getX(); x <= plot.getRight() + 0.5f; x += 2.0f)
        {
            const float f = hzForX (x);
            float db = 0.0f;
            if (lc > 20.5f)    db += -10.0f * std::log10 (1.0f + std::pow (lc / f, 2.0f * (float) nL));
            if (hc < 19900.0f) db += -10.0f * std::log10 (1.0f + std::pow (f / hc, 2.0f * (float) nH));
            const float y = yResp (db);
            // só desenha a curva onde o filtro está mesmo a cortar
            if (db < -0.25f) { if (! penDown) resp.startNewSubPath (x, y); else resp.lineTo (x, y); penDown = true; }
            else penDown = false;
            shade.lineTo (x, y);
        }
        shade.lineTo (plot.getRight(), plot.getY());
        shade.closeSubPath();
        if (lc > 20.5f || hc < 19900.0f)
        {
            g.setColour (juce::Colours::black.withAlpha (0.38f));
            g.fillPath (shade);
            g.setColour (col::silver.withAlpha (0.55f));
            g.strokePath (resp, juce::PathStrokeType (1.3f));
        }

        // pegas na base
        for (int k = 0; k < 2; ++k)
        {
            const bool high = k == 1;
            const float x = xForHz (high ? hc : lc);
            const bool act = high ? hc < 19900.0f : lc > 20.5f;
            const bool hot = drag == (high ? Drag::HighCut : Drag::LowCut) || hoverPart == (high ? Drag::HighCut : Drag::LowCut);
            auto pill = juce::Rectangle<float> (28.0f, 14.0f).withCentre ({ juce::jlimit (plot.getX() + 14.0f, plot.getRight() - 14.0f, x), plot.getBottom() - 9.0f });
            g.setColour (hot ? col::panelHi.brighter (0.1f) : col::panelHi);
            g.fillRoundedRectangle (pill, 7.0f);
            g.setColour (act || hot ? col::silver.withAlpha (0.85f) : col::lineHi);
            g.drawRoundedRectangle (pill, 7.0f, 1.0f);
            g.setColour (act || hot ? col::text : col::textDim);
            g.setFont (Fonts::caps (10.0f, 0.05f));
            g.drawText (high ? "HC" : "LC", pill, juce::Justification::centred);
        }
    }

    // ---- Temper Range: escurece fora e desenha as pegas
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillRect (plot.withRight (xLo));
    g.fillRect (plot.withLeft (xHi));

    const bool flats = ctx.useFlats();
    for (int k = 0; k < 2; ++k)
    {
        const bool high = k == 1;
        const float x = high ? xHi : xLo;
        const bool hot = drag == (high ? Drag::RangeHigh : Drag::RangeLow) || hoverPart == (high ? Drag::RangeHigh : Drag::RangeLow)
                      || drag == Drag::RangeBoth || hoverPart == Drag::RangeBoth;
        g.setColour (acc.withAlpha (hot ? 0.95f : 0.65f));
        g.fillRect (x - 0.75f, plot.getY() + 16.0f, 1.5f, plot.getHeight() - 16.0f);
        const auto txt = juce::String (music::noteName (juce::roundToInt (high ? hi : lo), flats));
        auto tab = juce::Rectangle<float> (36.0f, 16.0f).withY (plot.getY() + 1.0f);
        tab = high ? tab.withRightX (juce::jmax (x, plot.getX() + 72.0f)) : tab.withX (juce::jmin (x, plot.getRight() - 72.0f));
        g.setColour (acc.withAlpha (hot ? 0.95f : 0.75f));
        g.fillRoundedRectangle (tab, 4.0f);
        g.setColour (col::bg);
        g.setFont (Fonts::label (12.5f));
        g.drawText (txt, tab, juce::Justification::centred);
    }

    // ---- balão com o valor (arrasto / hover)
    const auto info = describe (drag != Drag::None ? drag : hoverPart);
    if (info.isNotEmpty())
    {
        g.setFont (Fonts::label (13.0f));
        const float w = juce::GlyphArrangement::getStringWidth (Fonts::label (13.0f), info) + 18.0f;
        auto bubble = juce::Rectangle<float> (w, 20.0f).withCentre ({ juce::jlimit (plot.getX() + w * 0.5f + 4.0f, plot.getRight() - w * 0.5f - 4.0f, lastX),
                                                                       plot.getY() + 30.0f });
        g.setColour (col::panelHi.withAlpha (0.96f));
        g.fillRoundedRectangle (bubble, 5.0f);
        g.setColour (col::lineHi);
        g.drawRoundedRectangle (bubble, 5.0f, 1.0f);
        g.setColour (col::text);
        g.drawText (info, bubble, juce::Justification::centred);
    }

    g.restoreState();
}

SpectrumView::Drag SpectrumView::partAt (juce::Point<float> p) const
{
    if (! plot.expanded (4.0f).contains (p)) return Drag::None;
    const float lo = juce::jmin (rangeLow.get(), rangeHigh.get());
    const float hi = juce::jmax (rangeLow.get(), rangeHigh.get());
    const float xLo = xForNote (lo - 0.5f), xHi = xForNote (hi + 0.5f);
    const float xLc = juce::jlimit (plot.getX() + 14.0f, plot.getRight() - 14.0f, xForHz (lowCut.get()));
    const float xHc = juce::jlimit (plot.getX() + 14.0f, plot.getRight() - 14.0f, xForHz (highCut.get()));

    if (p.y >= plot.getBottom() - 20.0f)
    {
        const float dl = std::abs (p.x - xLc), dh = std::abs (p.x - xHc);
        if (dl < 16.0f || dh < 16.0f) return dl <= dh ? Drag::LowCut : Drag::HighCut;
        if (p.x > xLc && p.x < xHc) return Drag::CutBoth;
        return p.x <= xLc ? Drag::LowCut : Drag::HighCut;
    }
    const float tabL0 = juce::jmin (xLo, plot.getRight() - 72.0f), tabH1 = juce::jmax (xHi, plot.getX() + 72.0f);
    if (p.y <= plot.getY() + 18.0f)
    {
        if (p.x >= tabL0 - 2.0f && p.x <= tabL0 + 38.0f) return Drag::RangeLow;
        if (p.x >= tabH1 - 38.0f && p.x <= tabH1 + 2.0f) return Drag::RangeHigh;
        if (p.x > xLo && p.x < xHi) return Drag::RangeBoth;
        return Drag::None;
    }
    if (std::abs (p.x - xLo) < 6.0f) return Drag::RangeLow;
    if (std::abs (p.x - xHi) < 6.0f) return Drag::RangeHigh;
    return Drag::None;
}

void SpectrumView::mouseMove (const juce::MouseEvent& e)
{
    lastX = e.position.x;
    const auto h = partAt (e.position);
    const bool overBtn = lowBtn.contains (e.position) || highBtn.contains (e.position);
    setMouseCursor (overBtn ? juce::MouseCursor::PointingHandCursor
                    : h == Drag::None ? juce::MouseCursor::NormalCursor : juce::MouseCursor::LeftRightResizeCursor);
    if (h != hoverPart) { hoverPart = h; repaint(); }
}

void SpectrumView::mouseExit (const juce::MouseEvent&)
{
    if (hoverPart != Drag::None) { hoverPart = Drag::None; repaint(); }
}

void SpectrumView::mouseDown (const juce::MouseEvent& e)
{
    if (lowBtn.contains (e.position))  { lowSlope.setOnce (lowSlope.get() > 0.5f ? 0.0f : 1.0f); repaint(); return; }
    if (highBtn.contains (e.position)) { highSlope.setOnce (highSlope.get() > 0.5f ? 0.0f : 1.0f); repaint(); return; }

    drag = partAt (e.position);
    dragStart = e.position;
    lastX = e.position.x;
    switch (drag)
    {
        case Drag::RangeLow:  rangeLow.begin();  startA = juce::jmin (rangeLow.get(), rangeHigh.get()); break;
        case Drag::RangeHigh: rangeHigh.begin(); startA = juce::jmax (rangeLow.get(), rangeHigh.get()); break;
        case Drag::RangeBoth: rangeLow.begin(); rangeHigh.begin(); startA = rangeLow.get(); startB = rangeHigh.get(); break;
        case Drag::LowCut:    lowCut.begin();  startA = lowCut.get(); break;
        case Drag::HighCut:   highCut.begin(); startA = highCut.get(); break;
        case Drag::CutBoth:   lowCut.begin(); highCut.begin(); startA = lowCut.get(); startB = highCut.get(); break;
        case Drag::None:      break;
    }
    repaint();
}

void SpectrumView::mouseDrag (const juce::MouseEvent& e)
{
    if (drag == Drag::None) return;
    const float k = e.mods.isShiftDown() ? 0.15f : 1.0f;
    const float x = dragStart.x + (e.position.x - dragStart.x) * k;
    lastX = e.position.x;
    const float dxNotes = noteForX (x) - noteForX (dragStart.x);
    const float ratio = hzForX (x) / hzForX (dragStart.x);
    const float nMin = (float) music::kNoteMin, nMax = (float) music::kNoteMax;

    switch (drag)
    {
        case Drag::RangeLow:
            rangeLow.set (juce::jlimit (nMin, juce::jmax (rangeLow.get(), rangeHigh.get()) - 1.0f, std::round ((startA + dxNotes) * 4.0f) / 4.0f));
            break;
        case Drag::RangeHigh:
            rangeHigh.set (juce::jlimit (juce::jmin (rangeLow.get(), rangeHigh.get()) + 1.0f, nMax, std::round ((startA + dxNotes) * 4.0f) / 4.0f));
            break;
        case Drag::RangeBoth:
        {
            const float d = juce::jlimit (nMin - startA, nMax - startB, std::round (dxNotes));
            rangeLow.set (startA + d);
            rangeHigh.set (startB + d);
            break;
        }
        case Drag::LowCut:  lowCut.set (juce::jmin (startA * ratio, highCut.get())); break;
        case Drag::HighCut: highCut.set (juce::jmax (startA * ratio, lowCut.get())); break;
        case Drag::CutBoth:
        {
            const float r = juce::jlimit (20.0f / startA, 20000.0f / startB, ratio);
            lowCut.set (startA * r);
            highCut.set (startB * r);
            break;
        }
        case Drag::None: break;
    }
    repaint();
}

void SpectrumView::mouseUp (const juce::MouseEvent&)
{
    switch (drag)
    {
        case Drag::RangeLow:  rangeLow.end();  break;
        case Drag::RangeHigh: rangeHigh.end(); break;
        case Drag::RangeBoth: rangeLow.end(); rangeHigh.end(); break;
        case Drag::LowCut:    lowCut.end(); break;
        case Drag::HighCut:   highCut.end(); break;
        case Drag::CutBoth:   lowCut.end(); highCut.end(); break;
        case Drag::None:      break;
    }
    drag = Drag::None;
    repaint();
}

void SpectrumView::mouseDoubleClick (const juce::MouseEvent& e)
{
    switch (partAt (e.position))
    {
        case Drag::RangeLow:  rangeLow.setOnce (rangeLow.defaultValue()); break;
        case Drag::RangeHigh: rangeHigh.setOnce (rangeHigh.defaultValue()); break;
        case Drag::RangeBoth: rangeLow.setOnce (rangeLow.defaultValue()); rangeHigh.setOnce (rangeHigh.defaultValue()); break;
        case Drag::LowCut:    lowCut.setOnce (lowCut.defaultValue()); break;
        case Drag::HighCut:   highCut.setOnce (highCut.defaultValue()); break;
        case Drag::CutBoth:   lowCut.setOnce (lowCut.defaultValue()); highCut.setOnce (highCut.defaultValue()); break;
        case Drag::None:      break;
    }
    repaint();
}
} // namespace roomx::ui
