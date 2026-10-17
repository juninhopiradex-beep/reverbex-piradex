#include "SequencePanel.h"
#include "Panels.h"
#include "../PluginProcessor.h"

namespace roomx::ui
{
namespace P = roomx::params;
using namespace roomx::music;

static juce::String gridLabel (const juce::String& n)
{
    if (n == "Bar")  return tr ("Compasso", "Bar");
    if (n == "Beat") return tr ("Tempo", "Beat");
    return n;
}

SequencePanel::SequencePanel (Context& c, state::SequenceModel& model)
    : ctx (c), seq (model),
      bars (c.apvts, P::seqBars), grid (c.apvts, P::seqGrid), transposeP (c.apvts, P::seqTranspose),
      offset (c.apvts, P::seqOffset), armed (c.apvts, P::seqArmed)
{
    setWantsKeyboardFocus (true);
    for (auto* b : { &barsDown, &barsUp, &trDown, &trUp, &retrig, &arm })
    {
        b->accent = &c.accent;
        addAndMakeVisible (b);
    }
    barsDown.onClick = [this] { bars.setOnce (bars.get() > 8 ? bars.get() - 4 : bars.get() - 1); clampView(); };
    barsUp.onClick   = [this] { bars.setOnce (bars.get() >= 8 ? bars.get() + 4 : bars.get() + 1); clampView(); };
    trDown.onClick   = [this] { transposeP.setOnce (transposeP.get() - 1); };
    trUp.onClick     = [this] { transposeP.setOnce (transposeP.get() + 1); };
    retrig.onClick   = [this] { ctx.proc.requestRetrigger(); };
    arm.setClickingTogglesState (true);
    armAtt = std::make_unique<APVTS::ButtonAttachment> (c.apvts, P::seqArmed, arm);

    gridBtn.getProperties().set ("fontSize", 14.0);
    gridBtn.onClick = [this]
    {
        juce::PopupMenu m;
        const auto& divs = P::gridDivisions();
        for (int i = 0; i < divs.size(); ++i)
            m.addItem (i + 1, gridLabel (divs[i].name), true, (int) grid.get() == i);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (gridBtn), [this] (int r)
        {
            if (r > 0) grid.setOnce ((float) (r - 1));
        });
    };
    addAndMakeVisible (gridBtn);
    viewLen = totalBeats();
    reload();
}

// ---------------------------------------------------------------------------
double SequencePanel::totalBeats() const { return (double) juce::jlimit (1, kSeqMaxBars, (int) bars.get()) * kBeatsPerBar; }
double SequencePanel::gridBeats() const
{
    const auto& d = P::gridDivisions();
    return d[juce::jlimit (0, d.size() - 1, (int) grid.get())].beats;
}
double SequencePanel::snap (double b) const { const double g = gridBeats(); return std::round (b / g) * g; }

float SequencePanel::xForBeat (double b) const
{
    return gridArea.getX() + (float) ((b - viewStart) / viewLen) * gridArea.getWidth();
}
double SequencePanel::beatForX (float x) const
{
    return viewStart + (double) ((x - gridArea.getX()) / gridArea.getWidth()) * viewLen;
}
int SequencePanel::rowForY (float y) const
{
    return juce::jlimit (0, kSeqRows - 1, (int) ((y - gridArea.getY()) / (gridArea.getHeight() / (float) kSeqRows)));
}
juce::Rectangle<float> SequencePanel::rowRect (int r) const
{
    const float h = gridArea.getHeight() / (float) kSeqRows;
    return { gridArea.getX(), gridArea.getY() + h * (float) r, gridArea.getWidth(), h };
}
juce::Rectangle<float> SequencePanel::regionRect (const SeqRegion& r) const
{
    auto row = rowRect (r.row);
    const float x0 = xForBeat (r.start), x1 = xForBeat (r.start + r.length);
    return { x0, row.getY() + 1.5f, juce::jmax (2.0f, x1 - x0), row.getHeight() - 3.0f };
}
int SequencePanel::regionAt (juce::Point<float> p) const
{
    for (int i = (int) regs.size(); --i >= 0;)
        if (regionRect (regs[(size_t) i]).expanded (2.0f, 0.0f).contains (p)) return i;
    return -1;
}

void SequencePanel::reload()
{
    regs.clear();
    const int n = seq.getNumRegions();
    for (int i = 0; i < n; ++i) regs.push_back (seq.getRegion (i));
    if (selected >= (int) regs.size()) selected = -1;
}

void SequencePanel::clampView()
{
    const double total = totalBeats();
    viewLen = juce::jlimit (juce::jmin (4.0, total), total, viewLen);
    viewStart = juce::jlimit (0.0, juce::jmax (0.0, total - viewLen), viewStart);
    repaint();
}

// Regiões sem sobreposição: a região "keep" ganha; as outras são aparadas/divididas
void SequencePanel::writeAll (std::vector<SeqRegion> list, int keep)
{
    const double total = totalBeats();
    std::vector<SeqRegion> out;
    SeqRegion k {};
    const bool hasKeep = juce::isPositiveAndBelow (keep, (int) list.size());
    if (hasKeep)
    {
        k = list[(size_t) keep];
        k.start = juce::jlimit (0.0, juce::jmax (0.0, total - 1.0 / 16.0), k.start);
        k.length = juce::jlimit (1.0 / 16.0, total - k.start, k.length);
    }
    for (int i = 0; i < (int) list.size(); ++i)
    {
        if (i == keep) continue;
        auto r = list[(size_t) i];
        if (! hasKeep) { out.push_back (r); continue; }
        const double ks = k.start, ke = k.start + k.length, rs = r.start, re = r.start + r.length;
        if (re <= ks + 1e-9 || rs >= ke - 1e-9) { out.push_back (r); continue; }
        if (rs < ks - 1e-9) out.push_back ({ r.row, rs, ks - rs });
        if (re > ke + 1e-9) out.push_back ({ r.row, ke, re - ke });
    }
    if (hasKeep) out.push_back (k);
    std::sort (out.begin(), out.end(), [] (const SeqRegion& a, const SeqRegion& b) { return a.start < b.start; });
    if ((int) out.size() > kSeqMaxRegions) out.resize ((size_t) kSeqMaxRegions);

    const int existing = seq.getNumRegions();
    for (int i = 0; i < (int) out.size(); ++i)
    {
        if (i < existing) seq.setRegion (i, out[(size_t) i]);
        else              seq.addRegion (out[(size_t) i]);
    }
    if (existing > (int) out.size())
    {
        juce::Array<int> rm;
        for (int i = (int) out.size(); i < existing; ++i) rm.add (i);
        seq.removeRegions (rm);
    }
    selected = -1;
    if (hasKeep)
        for (int i = 0; i < (int) out.size(); ++i)
            if (std::abs (out[(size_t) i].start - k.start) < 1e-9 && out[(size_t) i].row == k.row) selected = i;
    reload();
    repaint();
}

// ---------------------------------------------------------------------------
void SequencePanel::resized()
{
    auto r = getLocalBounds().toFloat().reduced (14.0f, 8.0f);
    side = r.removeFromRight (226.0f);
    r.removeFromRight (14.0f);
    labels = r.removeFromLeft (112.0f);
    r.removeFromLeft (6.0f);
    ruler = r.removeFromTop (16.0f);
    nav = r.removeFromBottom (12.0f);
    r.removeFromBottom (5.0f);
    gridArea = r;
    labels = labels.withTrimmedTop (16.0f).withTrimmedBottom (17.0f);

    auto s = side.reduced (10.0f, 4.0f);
    s.removeFromTop (22.0f);
    const float rowH = (s.getHeight() - 36.0f) / 4.0f;
    auto rowBars = s.removeFromTop (rowH), rowGrid = s.removeFromTop (rowH), rowOff = s.removeFromTop (rowH), rowTr = s.removeFromTop (rowH);
    auto place = [] (juce::Rectangle<float> row, juce::Component& minus, juce::Component& plus)
    {
        auto v = row.removeFromRight (104.0f);
        minus.setBounds (v.removeFromLeft (26.0f).withSizeKeepingCentre (26.0f, 26.0f).toNearestInt());
        plus.setBounds (v.removeFromRight (26.0f).withSizeKeepingCentre (26.0f, 26.0f).toNearestInt());
    };
    place (rowBars, barsDown, barsUp);
    place (rowTr, trDown, trUp);
    gridBtn.setBounds (rowGrid.removeFromRight (104.0f).withSizeKeepingCentre (104.0f, 26.0f).toNearestInt());
    offsetBox = rowOff.removeFromRight (104.0f).withSizeKeepingCentre (104.0f, 26.0f);
    auto btns = s.withTrimmedTop (4.0f);
    arm.setBounds (btns.removeFromRight (32.0f).withSizeKeepingCentre (32.0f, 32.0f).toNearestInt());
    btns.removeFromRight (8.0f);
    retrig.setBounds (btns.removeFromRight (32.0f).withSizeKeepingCentre (32.0f, 32.0f).toNearestInt());
    clampView();
}

void SequencePanel::tick()
{
    reload();
    const auto& divs = P::gridDivisions();
    const juce::String gname = gridLabel (divs[juce::jlimit (0, divs.size() - 1, (int) grid.get())].name);
    if (gridBtn.getButtonText() != gname) gridBtn.setButtonText (gname);
    if (viewLen > totalBeats() + 1e-9 || viewStart + viewLen > totalBeats() + 1e-9) clampView();

    // segue o cursor de reprodução quando sai da vista
    const float pos = ctx.proc.ui.seqPosition.load();
    if (pos >= 0.0f && mode == Mode::None && ((double) pos < viewStart || (double) pos > viewStart + viewLen))
    {
        viewStart = std::floor ((double) pos / 4.0) * 4.0;
        clampView();
    }
    repaint();
}

void SequencePanel::paint (juce::Graphics& g)
{
    const auto acc = ctx.accent.current;
    const bool flats = ctx.useFlats();
    const float playPos = ctx.proc.ui.seqPosition.load();
    const bool playing = playPos >= 0.0f;
    const int activeRow = playing ? ctx.proc.ui.seqActiveRow.load() : seq.getSelectedRow();
    const double total = totalBeats();

    // ---- etiquetas das linhas
    for (int r = 0; r < kSeqRows; ++r)
    {
        const float h = labels.getHeight() / (float) kSeqRows;
        auto lr = juce::Rectangle<float> (labels.getX(), labels.getY() + h * (float) r, labels.getWidth(), h).reduced (0.0f, 1.0f);
        const bool active = r == activeRow;
        g.setColour (active ? acc.withAlpha (0.12f) : col::well);
        g.fillRoundedRectangle (lr, 4.0f);
        if (! playing && active)
        {
            g.setColour (col::silver.withAlpha (0.5f));
            g.drawRoundedRectangle (lr.reduced (0.5f), 4.0f, 1.0f);
        }
        if (active)
        {
            juce::Path tri;
            tri.addTriangle (lr.getX() + 6.0f, lr.getCentreY() - 4.0f, lr.getX() + 6.0f, lr.getCentreY() + 4.0f, lr.getX() + 12.0f, lr.getCentreY());
            g.setColour (acc);
            g.fillPath (tri);
        }
        g.setColour (active ? col::text : col::textDim);
        g.setFont (Fonts::label (juce::jmin (14.0f, h * 0.82f)));
        g.drawText (harmonyLabel (seq.getRow (r), flats), lr.withTrimmedLeft (17.0f), juce::Justification::centredLeft);
    }

    // ---- grelha
    g.setColour (col::well);
    g.fillRoundedRectangle (gridArea.expanded (2.0f), 4.0f);
    g.saveState();
    g.reduceClipRegion (gridArea.toNearestInt());
    for (int r = 0; r < kSeqRows; ++r)
        if (r % 2 == 1) { g.setColour (juce::Colours::white.withAlpha (0.018f)); g.fillRect (rowRect (r)); }

    const double gb = gridBeats();
    const double first = std::floor (viewStart / gb) * gb;
    const float pxPerGrid = (float) (gb / viewLen) * gridArea.getWidth();
    for (double b = first; b <= viewStart + viewLen + 1e-9; b += gb)
    {
        const float x = xForBeat (b);
        const bool bar = std::abs (std::fmod (b, 4.0)) < 1e-6;
        const bool beat = std::abs (b - std::round (b)) < 1e-6;
        if (! bar && ! beat && pxPerGrid < 5.0f) continue;
        g.setColour (bar ? col::lineHi : (beat ? col::line : col::line.withAlpha (0.45f)));
        g.fillRect (x, gridArea.getY(), 1.0f, gridArea.getHeight());
    }
    if (total < viewStart + viewLen)
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRect (gridArea.withLeft (xForBeat (total)));
    }

    // regiões
    for (int i = 0; i < (int) regs.size(); ++i)
    {
        const auto& rg = regs[(size_t) i];
        auto rr = regionRect (rg);
        const bool sel = i == selected;
        const bool hot = playing && (double) playPos >= rg.start && (double) playPos < rg.start + rg.length;
        g.setColour (hot ? acc.withAlpha (0.85f) : col::silver.withAlpha (0.58f));
        g.fillRoundedRectangle (rr, 3.0f);
        if (sel)
        {
            g.setColour (acc);
            g.drawRoundedRectangle (rr.expanded (1.0f), 3.0f, 1.5f);
        }
        if (rr.getWidth() > 46.0f)
        {
            g.setColour (col::bg.withAlpha (0.85f));
            g.setFont (Fonts::label (juce::jmin (13.0f, rr.getHeight() * 0.85f)));
            g.drawText (harmonyLabel (seq.getRow (rg.row), flats), rr.reduced (6.0f, 0.0f), juce::Justification::centredLeft);
        }
    }

    // cursor de reprodução
    if (playing)
    {
        const float x = xForBeat (playPos);
        g.setColour (acc);
        g.fillRect (x - 0.75f, gridArea.getY(), 1.5f, gridArea.getHeight());
    }
    g.restoreState();

    // ---- régua
    g.setFont (Fonts::text (11.5f));
    for (int bar = (int) (viewStart / 4.0); bar * 4.0 <= viewStart + viewLen && bar * 4 < (int) total; ++bar)
    {
        const float x = xForBeat (bar * 4.0);
        const int stepBars = viewLen > 96.0 ? 8 : (viewLen > 48.0 ? 4 : (viewLen > 24.0 ? 2 : 1));
        if (bar % stepBars != 0) continue;
        g.setColour (col::textFaint);
        g.drawText (juce::String (bar + 1), juce::Rectangle<float> (x + 3.0f, ruler.getY(), 30.0f, ruler.getHeight()), juce::Justification::centredLeft);
    }
    if (playing)
    {
        const float x = xForBeat (playPos);
        if (x >= gridArea.getX() && x <= gridArea.getRight())
        {
            juce::Path tri;
            tri.addTriangle (x - 5.0f, ruler.getY() + 3.0f, x + 5.0f, ruler.getY() + 3.0f, x, ruler.getBottom() - 1.0f);
            g.setColour (acc);
            g.fillPath (tri);
        }
    }

    // ---- navegador
    g.setColour (col::well);
    g.fillRoundedRectangle (nav, 5.0f);
    for (const auto& rg : regs)
    {
        const float x0 = nav.getX() + (float) (rg.start / total) * nav.getWidth();
        const float w  = (float) (rg.length / total) * nav.getWidth();
        g.setColour (col::silver.withAlpha (0.35f));
        g.fillRect (x0, nav.getY() + 3.0f + (float) rg.row * 0.75f, juce::jmax (1.0f, w - 1.0f), 2.0f);
    }
    auto win = juce::Rectangle<float> (nav.getX() + (float) (viewStart / total) * nav.getWidth(), nav.getY(),
                                       (float) (viewLen / total) * nav.getWidth(), nav.getHeight());
    g.setColour (col::text.withAlpha (0.10f));
    g.fillRoundedRectangle (win, 5.0f);
    g.setColour (col::lineHi);
    g.drawRoundedRectangle (win.reduced (0.5f), 5.0f, 1.0f);
    if (playing)
    {
        g.setColour (acc);
        g.fillRect (nav.getX() + playPos / (float) total * nav.getWidth() - 0.5f, nav.getY(), 1.5f, nav.getHeight());
    }

    // ---- lateral (timing)
    drawPanel (g, side, 8.0f);
    auto s = side.reduced (10.0f, 4.0f);
    g.setColour (col::text);
    g.setFont (Fonts::caps (13.0f));
    g.drawText (tr ("TEMPO", "TIMING"), s.removeFromTop (22.0f), juce::Justification::centredLeft);
    const float rowH = (s.getHeight() - 36.0f) / 4.0f;
    auto drawRow = [&] (const juce::String& name, const juce::String& value, bool withButtons)
    {
        auto row = s.removeFromTop (rowH);
        g.setColour (col::textDim);
        g.setFont (Fonts::label (14.0f));
        g.drawText (name, row, juce::Justification::centredLeft);
        if (withButtons)
        {
            g.setColour (col::text);
            g.setFont (Fonts::label (15.0f));
            g.drawText (value, row.removeFromRight (104.0f).reduced (26.0f, 0.0f), juce::Justification::centred);
        }
    };
    drawRow (tr ("Compassos (4/4)", "Bars (4/4)"), juce::String ((int) bars.get()), true);
    drawRow (tr ("Grelha", "Grid"), {}, false);
    drawRow (tr ("Desvio", "Offset"), {}, false);
    const int tp = (int) transposeP.get();
    drawRow (tr ("Transpor", "Transpose"), (tp > 0 ? "+" : "") + juce::String (tp), true);

    g.setColour (mode == Mode::Offset ? col::panelHi.brighter (0.08f) : col::panelHi);
    g.fillRoundedRectangle (offsetBox, 6.0f);
    g.setColour (mode == Mode::Offset ? acc : col::line);
    g.drawRoundedRectangle (offsetBox.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (col::text);
    g.setFont (Fonts::label (14.0f));
    g.drawText (P::beatsToBarBeat (offset.get()), offsetBox, juce::Justification::centred);

    g.setColour (col::textFaint);
    g.setFont (Fonts::text (12.0f));
    g.drawText (armed.get() > 0.5f ? tr ("Segue o DAW", "Follows the DAW") : tr ("Desarmado: usa a linha seleccionada", "Disarmed: uses the selected row"),
                s.withTrimmedTop (4.0f).withTrimmedRight (82.0f), juce::Justification::centredLeft);
}

// ---------------------------------------------------------------------------
void SequencePanel::showRowBrowser (int row)
{
    seq.setSelectedRow (row);
    auto browser = std::make_unique<ScaleBrowser> (ctx, [this, row] (const Harmony& h)
    {
        seq.setRow (row, h);
        repaint();
    }, seq.getRow (row));
    const float h = labels.getHeight() / (float) kSeqRows;
    auto area = juce::Rectangle<float> (labels.getX(), labels.getY() + h * (float) row, labels.getWidth(), h);
    juce::CallOutBox::launchAsynchronously (std::move (browser), localAreaToGlobal (area.toNearestInt()), nullptr);
}

void SequencePanel::showRegionMenu (int index)
{
    juce::PopupMenu m;
    if (index >= 0)
    {
        m.addItem (1, tr ("Duplicar a seguir", "Duplicate after"));
        m.addItem (2, tr ("Apagar", "Delete"));
        m.addSeparator();
    }
    m.addItem (3, tr ("Limpar a sequência", "Clear sequence"));
    m.showMenuAsync ({}, [this, index] (int r)
    {
        ctx.undo().beginNewTransaction ("Sequence edit");
        if (r == 1 && juce::isPositiveAndBelow (index, (int) regs.size()))
        {
            auto list = regs;
            auto copy = regs[(size_t) index];
            copy.start += copy.length;
            if (copy.start < totalBeats()) { list.push_back (copy); writeAll (list, (int) list.size() - 1); }
        }
        else if (r == 2 && index >= 0) { seq.removeRegions ({ index }); selected = -1; reload(); }
        else if (r == 3) { seq.clearRegions(); selected = -1; reload(); }
        repaint();
    });
}

void SequencePanel::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const auto p = e.position;

    if (offsetBox.contains (p))
    {
        mode = Mode::Offset;
        offsetStart = offset.get();
        offset.begin();
        return;
    }
    if (labels.contains (p))
    {
        const int row = juce::jlimit (0, kSeqRows - 1, (int) ((p.y - labels.getY()) / (labels.getHeight() / (float) kSeqRows)));
        showRowBrowser (row);
        repaint();
        return;
    }
    if (nav.expanded (0.0f, 3.0f).contains (p))
    {
        const double total = totalBeats();
        const float x0 = nav.getX() + (float) (viewStart / total) * nav.getWidth();
        const float x1 = nav.getX() + (float) ((viewStart + viewLen) / total) * nav.getWidth();
        navStartView = viewStart; navStartLen = viewLen;
        if (std::abs (p.x - x0) < 6.0f)      mode = Mode::NavL;
        else if (std::abs (p.x - x1) < 6.0f) mode = Mode::NavR;
        else
        {
            mode = Mode::Nav;
            if (p.x < x0 || p.x > x1)
            {
                viewStart = (double) ((p.x - nav.getX()) / nav.getWidth()) * total - viewLen * 0.5;
                navStartView = viewStart;
                clampView();
                navStartView = viewStart;
            }
        }
        return;
    }
    if (! gridArea.contains (p)) return;

    const int hit = regionAt (p);
    if (e.mods.isPopupMenu()) { selected = hit; repaint(); showRegionMenu (hit); return; }

    ctx.undo().beginNewTransaction ("Sequence edit");
    if (hit >= 0)
    {
        selected = hit;
        dragOrig = regs[(size_t) hit];
        auto rr = regionRect (dragOrig);
        if (p.x - rr.getX() < 6.0f && rr.getWidth() > 14.0f)      mode = Mode::ResizeL;
        else if (rr.getRight() - p.x < 6.0f)                       mode = Mode::ResizeR;
        else
        {
            mode = Mode::Move;
            dragAnchorBeat = beatForX (p.x);
            if (e.mods.isAltDown())
            {
                const int idx = seq.addRegion (dragOrig);
                if (idx >= 0) { reload(); selected = idx; }
            }
        }
    }
    else
    {
        mode = Mode::Create;
        const double gbeats = gridBeats();
        const double start = juce::jlimit (0.0, totalBeats() - gbeats, std::floor (beatForX (p.x) / gbeats) * gbeats);
        dragOrig = { rowForY (p.y), start, gbeats };
        dragAnchorBeat = start;
        const int idx = seq.addRegion (dragOrig);
        if (idx >= 0) { reload(); selected = idx; }
        else mode = Mode::None;
    }
    repaint();
}

void SequencePanel::mouseDrag (const juce::MouseEvent& e)
{
    const auto p = e.position;
    const double total = totalBeats();
    const double gb = gridBeats();

    switch (mode)
    {
        case Mode::Offset:
        {
            const float k = e.mods.isShiftDown() ? 0.05f : 0.25f;
            offset.set (std::round ((offsetStart - (float) e.getDistanceFromDragStartY() * k) * 4.0f) / 4.0f);
            repaint();
            return;
        }
        case Mode::Nav:
            viewStart = navStartView + (double) (e.getDistanceFromDragStartX() / nav.getWidth()) * total;
            clampView();
            return;
        case Mode::NavL:
        {
            const double end = navStartView + navStartLen;
            const double ns = juce::jlimit (0.0, end - 4.0, navStartView + (double) (e.getDistanceFromDragStartX() / nav.getWidth()) * total);
            viewStart = ns; viewLen = end - ns;
            clampView();
            return;
        }
        case Mode::NavR:
            viewLen = juce::jmax (4.0, navStartLen + (double) (e.getDistanceFromDragStartX() / nav.getWidth()) * total);
            clampView();
            return;
        case Mode::None:
            return;
        case Mode::Create: case Mode::Move: case Mode::ResizeL: case Mode::ResizeR:
            break;
    }

    if (! juce::isPositiveAndBelow (selected, (int) regs.size())) return;
    SeqRegion r = dragOrig;
    const double beat = beatForX (p.x);
    if (mode == Mode::Create)
    {
        const double endB = std::ceil (beat / gb) * gb;
        if (endB > dragAnchorBeat) { r.start = dragAnchorBeat; r.length = juce::jmax (gb, endB - dragAnchorBeat); }
        else { r.start = juce::jmax (0.0, std::floor (beat / gb) * gb); r.length = dragAnchorBeat + gb - r.start; }
        r.row = rowForY (p.y);
    }
    else if (mode == Mode::Move)
    {
        const double delta = std::round ((beat - dragAnchorBeat) / gb) * gb;
        r.start = juce::jlimit (0.0, juce::jmax (0.0, total - r.length), dragOrig.start + delta);
        r.row = rowForY (p.y);
    }
    else if (mode == Mode::ResizeL)
    {
        const double end = dragOrig.start + dragOrig.length;
        r.start = juce::jlimit (0.0, end - gb, snap (beat));
        r.length = end - r.start;
    }
    else if (mode == Mode::ResizeR)
    {
        const double end = juce::jlimit (dragOrig.start + gb, total, snap (beat));
        r.length = end - dragOrig.start;
    }
    r.length = juce::jmin (r.length, total - r.start);
    seq.setRegion (selected, r);
    regs[(size_t) selected] = r;
    repaint();
}

void SequencePanel::mouseUp (const juce::MouseEvent&)
{
    if (mode == Mode::Offset) offset.end();
    const bool edit = mode == Mode::Create || mode == Mode::Move || mode == Mode::ResizeL || mode == Mode::ResizeR;
    mode = Mode::None;
    if (edit)
    {
        reload();
        writeAll (regs, selected);
    }
    repaint();
}

void SequencePanel::mouseMove (const juce::MouseEvent& e)
{
    const auto p = e.position;
    auto cur = juce::MouseCursor::NormalCursor;
    if (offsetBox.contains (p)) cur = juce::MouseCursor::UpDownResizeCursor;
    else if (labels.contains (p)) cur = juce::MouseCursor::PointingHandCursor;
    else if (gridArea.contains (p))
    {
        const int hit = regionAt (p);
        if (hit >= 0)
        {
            auto rr = regionRect (regs[(size_t) hit]);
            cur = (p.x - rr.getX() < 6.0f || rr.getRight() - p.x < 6.0f) ? juce::MouseCursor::LeftRightResizeCursor
                                                                              : juce::MouseCursor::DraggingHandCursor;
        }
        else cur = juce::MouseCursor::CrosshairCursor;
    }
    setMouseCursor (cur);
}

void SequencePanel::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (offsetBox.contains (e.position)) { offset.setOnce (0.0f); repaint(); return; }
    const int hit = regionAt (e.position);
    if (hit >= 0 && gridArea.contains (e.position))
    {
        ctx.undo().beginNewTransaction ("Delete region");
        seq.removeRegions ({ hit });
        selected = -1;
        reload();
        repaint();
    }
}

void SequencePanel::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& d)
{
    if (! gridArea.contains (e.position) && ! nav.contains (e.position)) return;
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        const double anchor = beatForX (e.position.x);
        const double nl = juce::jlimit (4.0, totalBeats(), viewLen * (d.deltaY > 0 ? 0.8 : 1.25));
        viewStart = anchor - (anchor - viewStart) * (nl / viewLen);
        viewLen = nl;
    }
    else
        viewStart -= (double) (d.deltaX != 0.0f ? d.deltaX : d.deltaY) * viewLen * 0.3;
    clampView();
}

bool SequencePanel::keyPressed (const juce::KeyPress& k)
{
    if ((k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) && juce::isPositiveAndBelow (selected, (int) regs.size()))
    {
        ctx.undo().beginNewTransaction ("Delete region");
        seq.removeRegions ({ selected });
        selected = -1;
        reload();
        repaint();
        return true;
    }
    return false;
}
} // namespace roomx::ui
