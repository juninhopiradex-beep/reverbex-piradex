#include "Panels.h"
#include "SpectrumView.h"
#include "../PluginProcessor.h"
#include "../licensing/License.h"

namespace roomx::ui
{
namespace P = roomx::params;
using namespace roomx::music;

juce::String describeMask (uint16_t mask, bool flats)
{
    mask &= kAllPitchClasses;
    if (mask == 0) return U ("Nenhuma nota");
    if (mask == kAllPitchClasses) return U ("Cromática (todas)");
    for (auto cat : { Category::Chord, Category::Scale, Category::Mode, Category::Interval })
        for (int root = 0; root < 12; ++root)
            for (int i = 0; i < numTypes (cat); ++i)
            {
                Harmony h { root, cat, i };
                if (h.mask() == mask) return juce::String::fromUTF8 (h.label (flats).c_str());
            }
    juce::StringArray names;
    for (int pc = 0; pc < 12; ++pc)
        if ((mask >> pc) & 1u) names.add (pitchName (pc, flats));
    return names.joinIntoString (" ");
}

// =============================================================================
//  NotePads — 12 teclas (disposição de piano), clique e arrasto para pintar
// =============================================================================
NotePads::NotePads (Context& c) : ctx (c), notes (c.apvts, P::notes)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

juce::Rectangle<float> NotePads::padFor (int pc) const
{
    static const int whiteIndex[12] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
    static const float blackPos[12]  = { 0, 0.5f, 0, 1.5f, 0, 0, 3.5f, 0, 4.5f, 0, 5.5f, 0 };
    const auto b = getLocalBounds().toFloat();
    const float gap = 7.0f;
    const float pw = juce::jmin (58.0f, (b.getWidth() - 6.0f * gap) / 7.0f);
    const float total = 7.0f * pw + 6.0f * gap;
    const float x0 = b.getCentreX() - total * 0.5f;
    const float ph = (b.getHeight() - gap) * 0.5f;
    if (whiteIndex[pc] >= 0)
        return { x0 + (float) whiteIndex[pc] * (pw + gap), b.getY() + ph + gap, pw, ph };
    return { x0 + blackPos[pc] * (pw + gap) + gap * 0.5f, b.getY(), pw, ph };
}

int NotePads::padAt (juce::Point<float> p) const
{
    for (int pc = 0; pc < 12; ++pc)
        if (padFor (pc).contains (p)) return pc;
    return -1;
}

void NotePads::paint (juce::Graphics& g)
{
    const int mask = (int) notes.get();
    const uint16_t active = ctx.displayedPitchClasses();
    const bool manual = ctx.noteMode() == (int) P::NoteMode::Manual;
    const bool flats = ctx.useFlats();
    for (int pc = 0; pc < 12; ++pc)
    {
        auto r = padFor (pc);
        const bool on = ((mask >> pc) & 1) != 0;
        const bool sounding = ((active >> pc) & 1u) != 0 && manual;
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (r.translated (0, 2.0f), 7.0f);
        if (on)
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a5261), r.getX(), r.getY(), juce::Colour (0xff2b313c), r.getX(), r.getBottom(), false));
        else
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff20242c), r.getX(), r.getY(), juce::Colour (0xff171a20), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (on ? col::silver.withAlpha (0.75f) : col::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);

        // LED (acento quando a nota está activa)
        auto led = juce::Rectangle<float> (r.getWidth() * 0.42f, 3.0f).withCentre ({ r.getCentreX(), r.getBottom() - 8.0f });
        g.setColour (sounding ? ctx.accent.current : (on ? col::silver.withAlpha (0.35f) : col::line));
        g.fillRoundedRectangle (led, 1.5f);
        if (sounding)
        {
            g.setColour (ctx.accent.current.withAlpha (0.25f));
            g.fillRoundedRectangle (led.expanded (3.0f, 2.5f), 3.0f);
        }

        g.setColour (on ? col::text : col::textDim);
        g.setFont (on ? Fonts::title (17.0f) : Fonts::label (16.0f));
        g.drawText (juce::String::fromUTF8 (pitchName (pc, flats)), r.withTrimmedBottom (10.0f), juce::Justification::centred);
    }
}

void NotePads::mouseDown (const juce::MouseEvent& e)
{
    const int pc = padAt (e.position);
    if (pc < 0) return;
    const int mask = (int) notes.get();
    paintOn = ((mask >> pc) & 1) == 0;
    last = pc;
    notes.begin();
    notes.set ((float) (paintOn ? (mask | (1 << pc)) : (mask & ~(1 << pc))));
    if (ctx.noteMode() != (int) P::NoteMode::Manual)
        ParamRef (ctx.apvts, P::noteMode).setOnce ((float) P::NoteMode::Manual);
    repaint();
}

void NotePads::mouseDrag (const juce::MouseEvent& e)
{
    const int pc = padAt (e.position);
    if (pc < 0 || pc == last) return;
    last = pc;
    const int mask = (int) notes.get();
    notes.set ((float) (paintOn ? (mask | (1 << pc)) : (mask & ~(1 << pc))));
    repaint();
}

void NotePads::mouseUp (const juce::MouseEvent&)
{
    if (last >= 0) notes.end();
    last = -1;
}

// =============================================================================
//  ScaleBrowser
// =============================================================================
static const char* kCatNamesPt[] = { "Intervalos", "Acordes", "Escalas", "Modos" };

ScaleBrowser::ScaleBrowser (Context& c, std::function<void (const Harmony&)> onPick, Harmony initial)
    : ctx (c), pick (std::move (onPick)), h (initial)
{
    setSize (600, 340);
    addAndMakeVisible (scroll);
    scroll.setAutoHide (true);
    scroll.addListener (this);
}

void ScaleBrowser::resized()
{
    auto r = getLocalBounds().toFloat().reduced (14.0f);
    r.removeFromTop (26.0f);
    catArea = r.removeFromLeft (130.0f);
    r.removeFromLeft (12.0f);
    rootArea = r.removeFromTop (34.0f);
    r.removeFromTop (10.0f);
    typeArea = r;
    scroll.setBounds (typeArea.removeFromRight (8.0f).toNearestInt());
    const int n = numTypes (h.category);
    scroll.setRangeLimits (0.0, (double) n);
    scroll.setCurrentRange (firstRow, (double) visibleRows());
}

int ScaleBrowser::visibleRows() const { return juce::jmax (1, (int) (typeArea.getHeight() / kRowH)); }

juce::Rectangle<float> ScaleBrowser::catRect (int i) const  { return { catArea.getX(), catArea.getY() + (float) i * 38.0f, catArea.getWidth(), 34.0f }; }
juce::Rectangle<float> ScaleBrowser::rootRect (int i) const
{
    const float w = rootArea.getWidth() / 12.0f;
    return { rootArea.getX() + w * (float) i, rootArea.getY(), w - 3.0f, rootArea.getHeight() };
}
juce::Rectangle<float> ScaleBrowser::typeRect (int i) const
{
    return { typeArea.getX(), typeArea.getY() + ((float) i - (float) firstRow) * kRowH, typeArea.getWidth() - 10.0f, kRowH - 2.0f };
}

void ScaleBrowser::paint (juce::Graphics& g)
{
    const auto acc = ctx.accent.current;
    const bool flats = ctx.useFlats();
    g.setColour (col::text);
    g.setFont (Fonts::caps (14.0f));
    g.drawText (U ("ESCOLHER ESCALA / ACORDE"), getLocalBounds().reduced (14).removeFromTop (22), juce::Justification::centredLeft);
    g.setColour (col::textDim);
    g.setFont (Fonts::text (13.5f));
    g.drawText (U ("Aplica-se logo · as notas podem ser editadas depois"), getLocalBounds().reduced (14).removeFromTop (22), juce::Justification::centredRight);

    for (int i = 0; i < kNumCategories; ++i)
    {
        auto r = catRect (i);
        const bool on = (int) h.category == i;
        g.setColour (on ? acc.withAlpha (0.15f) : (i == hoverCat ? col::panelHi : col::well));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (on ? acc.withAlpha (0.8f) : col::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        g.setColour (on ? col::text : col::textDim);
        g.setFont (Fonts::label (15.0f));
        g.drawText (U (kCatNamesPt[i]), r.reduced (12.0f, 0.0f), juce::Justification::centredLeft);
    }

    for (int i = 0; i < 12; ++i)
    {
        auto r = rootRect (i);
        const bool on = h.root == i;
        g.setColour (on ? col::silver.withAlpha (0.18f) : (i == hoverRoot ? col::panelHi : col::well));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (on ? col::silver.withAlpha (0.8f) : col::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
        g.setColour (on ? col::text : col::textDim);
        g.setFont (Fonts::label (14.0f));
        g.drawText (pitchName (i, flats), r, juce::Justification::centred);
    }

    g.saveState();
    g.reduceClipRegion (typeArea.toNearestInt());
    const int n = numTypes (h.category);
    for (int i = (int) firstRow; i < n && i < (int) firstRow + visibleRows() + 1; ++i)
    {
        auto r = typeRect (i);
        const bool on = h.typeIndex == i;
        if (on || i == hoverType)
        {
            g.setColour (on ? acc.withAlpha (0.16f) : col::panelHi);
            g.fillRoundedRectangle (r, 4.0f);
        }
        const auto& t = type (h.category, i);
        g.setColour (on ? col::text : col::text.withAlpha (0.85f));
        g.setFont (Fonts::label (14.5f));
        g.drawText (juce::String::fromUTF8 (t.longName), r.reduced (10.0f, 0.0f), juce::Justification::centredLeft);
        // mini-mapa das 12 notas
        const uint16_t m = rotatePitchMask (t.mask, h.root);
        for (int pc = 0; pc < 12; ++pc)
        {
            auto dot = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ r.getRight() - 12.0f - (float) (11 - pc) * 10.0f, r.getCentreY() });
            g.setColour (((m >> pc) & 1u) ? (on ? acc : col::silver.withAlpha (0.8f)) : col::line);
            g.fillEllipse (dot);
        }
    }
    g.restoreState();
}

void ScaleBrowser::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < kNumCategories; ++i)
        if (catRect (i).contains (e.position))
        {
            h.category = (Category) i;
            h.typeIndex = 0;
            firstRow = 0.0;
            resized();
            pick (h);
            repaint();
            return;
        }
    for (int i = 0; i < 12; ++i)
        if (rootRect (i).contains (e.position)) { h.root = i; pick (h); repaint(); return; }
    if (typeArea.contains (e.position))
    {
        const int i = (int) firstRow + (int) ((e.position.y - typeArea.getY()) / kRowH);
        if (juce::isPositiveAndBelow (i, numTypes (h.category))) { h.typeIndex = i; pick (h); repaint(); }
    }
}

void ScaleBrowser::mouseMove (const juce::MouseEvent& e)
{
    hoverCat = hoverRoot = hoverType = -1;
    for (int i = 0; i < kNumCategories; ++i) if (catRect (i).contains (e.position)) hoverCat = i;
    for (int i = 0; i < 12; ++i) if (rootRect (i).contains (e.position)) hoverRoot = i;
    if (typeArea.contains (e.position)) hoverType = (int) firstRow + (int) ((e.position.y - typeArea.getY()) / kRowH);
    repaint();
}

void ScaleBrowser::mouseExit (const juce::MouseEvent&) { hoverCat = hoverRoot = hoverType = -1; repaint(); }

void ScaleBrowser::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& d)
{
    const double maxFirst = juce::jmax (0.0, (double) (numTypes (h.category) - visibleRows()));
    firstRow = juce::jlimit (0.0, maxFirst, std::round (firstRow - d.deltaY * 8.0));
    scroll.setCurrentRangeStart (firstRow, juce::dontSendNotification);
    repaint();
}

// =============================================================================
//  ManualPanel
// =============================================================================
ManualPanel::ManualPanel (Context& c) : ctx (c), notes (c.apvts, P::notes), pads (c)
{
    addAndMakeVisible (pads);
    for (auto* b : { &choose, &clear, &all, &invert })
    {
        b->getProperties().set ("fontSize", 14.0);
        addAndMakeVisible (b);
    }
    for (auto* b : { &down, &up }) { b->accent = &c.accent; addAndMakeVisible (b); }

    choose.onClick = [this]
    {
        auto browser = std::make_unique<ScaleBrowser> (ctx, [this] (const Harmony& h)
        {
            lastPicked = h;
            notes.setOnce ((float) h.mask());
            if (ctx.noteMode() != (int) P::NoteMode::Manual)
                ParamRef (ctx.apvts, P::noteMode).setOnce ((float) P::NoteMode::Manual);
            repaint();
        }, lastPicked);
        juce::CallOutBox::launchAsynchronously (std::move (browser), choose.getScreenBounds(), nullptr);
    };
    clear.onClick  = [this] { notes.setOnce (0.0f); repaint(); };
    all.onClick    = [this] { notes.setOnce ((float) kAllPitchClasses); repaint(); };
    invert.onClick = [this] { notes.setOnce ((float) ((~(int) notes.get()) & kAllPitchClasses)); repaint(); };
    down.onClick   = [this] { transpose (-1); };
    up.onClick     = [this] { transpose (+1); };
}

void ManualPanel::transpose (int semis)
{
    notes.setOnce ((float) rotatePitchMask ((uint16_t) (int) notes.get(), semis));
    lastPicked.root = (lastPicked.root + semis + 12) % 12;
    repaint();
}

void ManualPanel::resized()
{
    auto r = getLocalBounds().reduced (18, 12);
    auto left = r.removeFromLeft (190);
    auto right = r.removeFromRight (190);
    choose.setBounds (left.withSizeKeepingCentre (176, 36).translated (0, 14));
    pads.setBounds (r.reduced (10, 0).withTrimmedBottom (20));

    auto tr = right.withSizeKeepingCentre (180, 36).translated (0, -6);
    tr.removeFromLeft (84);
    down.setBounds (tr.removeFromLeft (36));
    tr.removeFromLeft (8);
    up.setBounds (tr.removeFromLeft (36));
    auto row = right.withSizeKeepingCentre (180, 28).translated (0, 38);
    const int w = (row.getWidth() - 8) / 3;
    clear.setBounds (row.removeFromLeft (w)); row.removeFromLeft (4);
    all.setBounds (row.removeFromLeft (w));   row.removeFromLeft (4);
    invert.setBounds (row);
}

void ManualPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (18, 12);
    auto right = r.removeFromRight (190);
    g.setColour (col::textDim);
    g.setFont (Fonts::caps (13.0f, 0.08f));
    g.drawText ("TRANSPOSE", right.withSizeKeepingCentre (180, 36).translated (0, -6).removeFromLeft (84), juce::Justification::centredLeft);

    auto left = r.removeFromLeft (190);
    g.setColour (col::textFaint);
    g.setFont (Fonts::caps (12.0f, 0.1f));
    g.drawText (U ("SELECÇÃO ACTUAL"), left.withSizeKeepingCentre (176, 18).translated (0, -32), juce::Justification::centred);
    g.setColour (col::text);
    g.setFont (Fonts::label (15.5f));
    g.drawFittedText (describeMask ((uint16_t) (int) notes.get(), ctx.useFlats()), left.withSizeKeepingCentre (186, 22).translated (0, -12),
                      juce::Justification::centred, 1);

    g.setColour (col::textFaint);
    g.setFont (Fonts::text (12.5f));
    g.drawText (U ("Clique ou arraste para pintar notas · também no círculo"), pads.getBounds().withY (pads.getBottom() + 2).withHeight (18),
                juce::Justification::centred);
}

// =============================================================================
//  MidiPanel
// =============================================================================
MidiPanel::MidiPanel (Context& c)
    : ctx (c), octave (c.apvts, P::midiOctave, "Octave Repeat"), release (c.apvts, P::midiRelease, "RELEASE")
{
    octave.setTooltip (U ("Ligado: cada nota tocada repete-se nas oitavas acima\nDesligado: só o registo exacto que tocou"));
    addAndMakeVisible (octave);
    addAndMakeVisible (release);
}

void MidiPanel::resized()
{
    auto r = getLocalBounds().reduced (18, 12);
    auto left = r.removeFromLeft (190);
    ledArea = left.toFloat().withSizeKeepingCentre (170, 40).translated (0, -18);
    auto right = r.removeFromRight (190);
    octave.setBounds (right.removeFromLeft (110).withSizeKeepingCentre (108, 32));
    release.setBounds (right.withSizeKeepingCentre (80, 96));
    keysArea = r.toFloat().reduced (14.0f, 8.0f);
}

void MidiPanel::tick()
{
    const int a = ctx.proc.ui.midiActivity.load();
    if (a != lastActivity) { lastActivity = a; led = 1.0f; }
    else led *= 0.85f;
    repaint();
}

void MidiPanel::paint (juce::Graphics& g)
{
    // actividade
    {
        auto r = ledArea;
        g.setColour (col::textDim);
        g.setFont (Fonts::caps (13.0f, 0.08f));
        g.drawText (U ("ENTRADA MIDI"), r.removeFromTop (18.0f), juce::Justification::centred);
        auto meter = r.withSizeKeepingCentre (120.0f, 8.0f).translated (0, 4);
        g.setColour (col::well);
        g.fillRoundedRectangle (meter, 4.0f);
        g.setColour (ctx.accent.current.withAlpha (0.25f + 0.75f * led));
        g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * juce::jmax (0.04f, led)), 4.0f);
        g.setColour (col::textFaint);
        g.setFont (Fonts::text (12.5f));
        g.drawText (U ("Todos os canais · Sustain CC64"), ledArea.translated (0, 40).withHeight (18), juce::Justification::centred);
    }

    // teclado C1..C8 com as notas que estão a "temperar"
    NoteSet ns;
    for (int i = 0; i < 3; ++i) ns.w[i] = ctx.proc.ui.notes[i].load();
    const bool midiMode = ctx.noteMode() == (int) P::NoteMode::Midi;
    const int lo = 24, hi = 108;
    int whites = 0;
    for (int n = lo; n <= hi; ++n) if (pitchName (n % 12, false)[1] == '\0') ++whites;
    const float ww = keysArea.getWidth() / (float) whites;
    float x = keysArea.getX();
    juce::Array<std::pair<juce::Rectangle<float>, bool>> blacks;
    for (int n = lo; n <= hi; ++n)
    {
        const bool white = pitchName (n % 12, false)[1] == '\0';
        const bool on = midiMode && ns.test (n);
        if (white)
        {
            auto k = juce::Rectangle<float> (x, keysArea.getY(), ww - 1.0f, keysArea.getHeight());
            g.setColour (on ? ctx.accent.current : juce::Colour (0xff8d94a1));
            g.fillRoundedRectangle (k, 2.0f);
            if (n % 12 == 0)
            {
                g.setColour (col::bg.withAlpha (0.6f));
                g.setFont (Fonts::label (10.5f));
                g.drawText ("C" + juce::String (n / 12 - 1), k.removeFromBottom (14.0f), juce::Justification::centred);
            }
            x += ww;
        }
        else
            blacks.add ({ juce::Rectangle<float> (x - ww * 0.32f, keysArea.getY(), ww * 0.64f, keysArea.getHeight() * 0.6f), on });
    }
    for (auto& [k, on] : blacks)
    {
        g.setColour (on ? ctx.accent.current.darker (0.2f) : juce::Colour (0xff14161b));
        g.fillRoundedRectangle (k, 2.0f);
    }
    if (! midiMode)
    {
        g.setColour (col::bg.withAlpha (0.55f));
        g.fillRect (keysArea);
    }
}

// =============================================================================
//  TuningPanel
// =============================================================================
TuningPanel::TuningPanel (Context& c)
    : ctx (c), ref (c.apvts, P::tuneRef, "A4"), cents (c.apvts, P::tuneCents, "FINE", true)
{
    setSize (330, 230);
    addAndMakeVisible (ref);
    addAndMakeVisible (cents);
    for (auto* b : { &reset, &sharps, &flats }) { b->getProperties().set ("fontSize", 14.0); addAndMakeVisible (b); }
    reset.onClick = [this]
    {
        ParamRef (ctx.apvts, P::tuneRef).setOnce (440.0f);
        ParamRef (ctx.apvts, P::tuneCents).setOnce (0.0f);
    };
    sharps.setClickingTogglesState (true);
    flats.setClickingTogglesState (true);
    sharps.setRadioGroupId (77);
    flats.setRadioGroupId (77);
    sharps.setToggleState (! ctx.useFlats(), juce::dontSendNotification);
    flats.setToggleState (ctx.useFlats(), juce::dontSendNotification);
    sharps.onClick = [this] { ctx.proc.setUseFlats (false); };
    flats.onClick  = [this] { ctx.proc.setUseFlats (true); };
}

void TuningPanel::resized()
{
    auto r = getLocalBounds().reduced (14);
    r.removeFromTop (24);
    auto knobs = r.removeFromTop (110);
    ref.setBounds (knobs.removeFromLeft (knobs.getWidth() / 2).reduced (10, 0));
    cents.setBounds (knobs.reduced (10, 0));
    r.removeFromTop (8);
    auto row = r.removeFromTop (30);
    sharps.setBounds (row.removeFromLeft (96)); row.removeFromLeft (6);
    flats.setBounds (row.removeFromLeft (96)); row.removeFromLeft (6);
    reset.setBounds (row);
}

void TuningPanel::paint (juce::Graphics& g)
{
    g.setColour (col::text);
    g.setFont (Fonts::caps (14.0f));
    g.drawText (U ("AFINAÇÃO GLOBAL"), getLocalBounds().reduced (14).removeFromTop (20), juce::Justification::centredLeft);
    g.setColour (col::textFaint);
    g.setFont (Fonts::text (12.5f));
    g.drawText (U ("Guardada por instância"), getLocalBounds().reduced (14).removeFromBottom (18), juce::Justification::centredLeft);
}

// =============================================================================
//  InfoPanel
// =============================================================================
InfoPanel::InfoPanel (Context& c, std::function<void()> onLogout) : ctx (c), logout (std::move (onLogout))
{
    setSize (430, 330);
    logoutBtn.getProperties().set ("danger", true);
    logoutBtn.getProperties().set ("fontSize", 14.0);
    logoutBtn.onClick = [this] { if (logout) logout(); };
    addAndMakeVisible (logoutBtn);
    analyzer.setToggleState (ctx.analyzer.enabled, juce::dontSendNotification);
    analyzer.onClick = [this]
    {
        ctx.analyzer.enabled = analyzer.getToggleState();
        ctx.apvts.state.setProperty ("analyzerOn", ctx.analyzer.enabled, nullptr);
    };
    addAndMakeVisible (analyzer);
}

void InfoPanel::resized()
{
    auto r = getLocalBounds().reduced (18);
    logoutBtn.setBounds (r.removeFromBottom (32));
    r.removeFromBottom (8);
    analyzer.setBounds (r.removeFromBottom (26));
}

void InfoPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (18);
    g.setColour (col::text);
    g.setFont (Fonts::title (26.0f));
    g.drawText ("ROOM-X", r.removeFromTop (30), juce::Justification::centredLeft);
    g.setColour (ctx.accent.current);
    g.setFont (Fonts::label (15.0f));
    g.drawText (U ("by Piradex  ·  v") + ROOMX_VERSION_STRING, r.removeFromTop (20), juce::Justification::centredLeft);
    r.removeFromTop (10);

    const auto status = lic::License::get().statusText();
    g.setColour (lic::License::get().kind() == lic::Kind::Beta ? col::warn : col::textDim);
    g.setFont (Fonts::label (14.0f));
    g.drawText (status.isNotEmpty() ? status : U ("Sem sessão"), r.removeFromTop (20), juce::Justification::centredLeft);
    r.removeFromTop (8);

    g.setColour (col::textDim);
    g.setFont (Fonts::text (13.5f));
    g.drawFittedText (U ("Reverb modal musical: escolhe notas, acordes, escalas, uma sequência ou toca MIDI, "
                         "e o TEMPERO realça (+) ou escava (−) essas ressonâncias no espaço. "
                         "TARGET decide se actua nas reflexões (Early), na cauda (Late) ou em tudo.\n\n"
                         "Duplo clique repõe qualquer controlo · Shift + arrastar = ajuste fino.\n"
                         "Suporte: juninhopiradex@hotmail.com  ·  www.beatfreakstudio.com/loja"),
                      r.removeFromTop (140), juce::Justification::topLeft, 9);
}
} // namespace roomx::ui
