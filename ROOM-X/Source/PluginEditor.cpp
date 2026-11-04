#include "PluginEditor.h"
#include "licensing/License.h"

using namespace roomx;
using namespace roomx::ui;
namespace P = roomx::params;

static const juce::Identifier kUiScale { "uiScale" };
static const juce::Identifier kAnalyzerOn { "analyzerOn" };

// =============================================================================
//  Context (implementação partilhada pelos componentes)
// =============================================================================
namespace roomx::ui
{
bool Context::useFlats() const           { return proc.useFlats(); }
music::Tuning Context::tuning() const    { return proc.currentTuning(); }
int  Context::noteMode() const           { return (int) apvts.getRawParameterValue (P::noteMode)->load(); }
juce::UndoManager& Context::undo() const { return proc.undoManager; }

uint16_t Context::displayedPitchClasses() const
{
    const auto mode = (P::NoteMode) noteMode();
    if (mode == P::NoteMode::Manual)
        return (uint16_t) ((int) apvts.getRawParameterValue (P::notes)->load() & music::kAllPitchClasses);
    if (mode == P::NoteMode::Sequence && proc.ui.seqPosition.load() < 0.0f)
    {
        // transporte parado: linha seleccionada (mesmo sem áudio a correr)
        const auto row = proc.sequence.getRow (proc.sequence.getSelectedRow());
        return music::rotatePitchMask (row.mask(), (int) apvts.getRawParameterValue (P::seqTranspose)->load());
    }
    music::NoteSet ns;
    for (int i = 0; i < 3; ++i) ns.w[i] = proc.ui.notes[i].load();
    return ns.pitchClasses();
}
} // namespace roomx::ui

// =============================================================================
RoomXEditor::RoomXEditor (RoomXProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    accent.current = accent.target = col::forTarget ((int) proc.apvts.getRawParameterValue (P::target)->load());
    analyzer.enabled = (bool) proc.apvts.state.getProperty (kAnalyzerOn, true);

    addAndMakeVisible (content);
    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &topBar, &spaceSel, &inputFader, &preDelay, &preDiv, &mix, &decay, &size, &preSync, &killDry, &mixLock,
             &noteScape, &target, &position, &density, &offset, &outputFader, &dualMono, &soloBank,
             &spectrum, &noteMode, &notesLock, &manualPanel, &sequencePanel, &midiPanel, &bypass, &testSig, &measure, &abMatch })
        content.addAndMakeVisible (c);
    content.addChildComponent (login);

    // ---- comportamentos específicos
    target.colourForIndex = [] (int i) { return col::forTarget (i); };
    target.onChange = [this] (int) { accent.target = col::forTarget (target.getIndex()); };

    mix.setValueText ([this] (double v)
    {
        const bool kill = proc.apvts.getRawParameterValue (P::killDry)->load() > 0.5f;
        return juce::String (juce::roundToInt (v)) + (kill ? "% wet" : "%");
    });
    for (auto* b : { &mixLock, &notesLock })
    {
        b->accent = &accent;
        b->setClickingTogglesState (true);
        b->drawBackground = false;
        b->onStateChange = [b] { b->setIcon (b->getToggleState() ? Icon::Lock : Icon::Unlock); };
    }
    mixLockAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, P::mixLock, mixLock);
    notesLockAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, P::notesLock, notesLock);
    for (auto* b : { &mixLock, &notesLock }) b->setIcon (b->getToggleState() ? Icon::Lock : Icon::Unlock);

    piradex::i18n::reload();             // outro plugin Piradex pode ter mudado o idioma
    lang.bind ([this] { applyLanguage(); });
    lang.onChange = [this] { content.repaint(); };
    bypass.getProperties().set ("danger", true);
    preSync.onStateChange = [this] { updateVisibility(); };
    noteMode.onChange = [this] (int) { updateVisibility(); };

    topBar.onInfo = [this]
    {
        showCallout (std::make_unique<InfoPanel> (ctx, [this] { logout(); }), topBar);
    };
    topBar.onTuning = [this] { showCallout (std::make_unique<TuningPanel> (ctx), topBar.tuning); };

    login.onUnlocked = [this]
    {
        login.setVisible (false);
        proc.ui.licensed = true;
        repaint();
    };
    wasLicensed = lic::License::get().isUnlocked();
    login.setVisible (! wasLicensed);

    addMouseListener (this, true);
    setWantsKeyboardFocus (true);

    // ---- tamanho / escala
    const double scale = juce::jlimit (0.7, 1.5, (double) proc.apvts.state.getProperty (kUiScale, 0.9));
    setResizable (true, true);
    setResizeLimits ((int) (kBaseW * 0.7), (int) (kBaseH * 0.7), (int) (kBaseW * 1.5), (int) (kBaseH * 1.5));
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) kBaseW / (double) kBaseH);
    setSize (juce::roundToInt (kBaseW * scale), juce::roundToInt (kBaseH * scale));

    updateVisibility();
    startTimerHz (30);
}

RoomXEditor::~RoomXEditor()
{
    stopTimer();
    removeMouseListener (this);
    if (&juce::LookAndFeel::getDefaultLookAndFeel() == &lnf)
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

// Textos fixos dos controlos no idioma actual (os nomes de automação no DAW não mudam)
void RoomXEditor::applyLanguage()
{
    inputFader.setTitle (tr ("ENTRADA", "INPUT"));
    outputFader.setTitle (tr ("SAÍDA", "OUTPUT"));
    const struct { Knob* k; const char* pt; const char* en; } knobs[] = {
        { &preDelay, "PRÉ-ATRASO", "PRE-DELAY" }, { &preDiv, "PRÉ-ATRASO", "PRE-DELAY" }, { &mix, "MISTURA", "MIX" },
        { &decay, "DECAIMENTO", "DECAY" }, { &size, "TAMANHO", "SIZE" }, { &position, "POSIÇÃO", "POSITION" },
        { &density, "DENSIDADE", "DENSITY" }, { &offset, "DESVIO", "OFFSET" } };
    for (auto& k : knobs) { k.k->title = tr (k.pt, k.en); k.k->repaint(); }

    target.caption = tr ("ALVO", "TARGET");
    target.setLabels ({ tr ("Inicial", "Early"), tr ("Tudo", "All"), tr ("Cauda", "Late") });
    noteMode.setLabels ({ "Manual", tr ("Sequência", "Sequence"), "MIDI" });

    preSync.setButtonText ("Sync");
    killDry.setButtonText (tr ("Sem seco", "Kill Dry"));
    dualMono.setButtonText (tr ("Mono duplo", "Dual Mono"));
    soloBank.setButtonText (tr ("Só banco", "Bank Solo"));
    soloBank.setTooltip (tr ("Ouvir só o banco modal (ressonância), sem o sinal seco", "Hear only the modal bank (resonance), without the dry signal"));
    bypass.setButtonText ("Bypass");
    preSync.setTooltip (tr ("Pré-atraso sincronizado com o andamento do DAW", "Pre-delay synced to the DAW tempo"));
    killDry.setTooltip (tr ("Remove o sinal seco (ideal em envio/aux)", "Removes the dry signal (ideal on a send/aux)"));
    dualMono.setTooltip (tr ("Esquerda e direita como dois espaços mono independentes", "Left and right as two independent mono spaces"));
    bypass.setTooltip (tr ("Ignorar o efeito (sem cliques)", "Bypass the effect (click-free)"));

    testSig.setLabels ({ tr ("Desligado", "Off"), tr ("Impulso", "Impulse"), tr ("Ruído rosa", "Pink noise") });
    measure.setTooltip (tr ("Dispara um impulso e mede o decaimento real do plugin (Schroeder)",
                            "Fires an impulse and measures the plugin's real decay (Schroeder)"));
    measure.onClick = [this] { proc.armRt60Measurement(); };

    abMatch.setButtonText (tr ("Igualar A/B", "Match A/B"));
    abMatch.setClickingTogglesState (true);
    abMatch.setToggleState (proc.abMatch(), juce::dontSendNotification);
    abMatch.setTooltip (tr ("Iguala o nível do slot B ao do A (aproximação pela energia da resposta medida em cada slot)",
                            "Matches slot B's level to A (approximation from the measured response energy of each slot)"));
    abMatch.onClick = [this] { proc.setAbMatch (abMatch.getToggleState()); };
}

void RoomXEditor::showCallout (std::unique_ptr<juce::Component> c, juce::Component& anchor)
{
    juce::CallOutBox::launchAsynchronously (std::move (c), anchor.getScreenBounds(), nullptr);
}

void RoomXEditor::logout()
{
    lic::License::get().lock();
    proc.ui.licensed = false;
    login.setVisible (true);
    login.toFront (true);
}

void RoomXEditor::updateVisibility()
{
    const bool sync = proc.apvts.getRawParameterValue (P::preDelaySync)->load() > 0.5f;
    preDelay.setVisible (! sync);
    preDiv.setVisible (sync);
    const int m = (int) proc.apvts.getRawParameterValue (P::noteMode)->load();
    manualPanel.setVisible (m == (int) P::NoteMode::Manual);
    sequencePanel.setVisible (m == (int) P::NoteMode::Sequence);
    midiPanel.setVisible (m == (int) P::NoteMode::Midi);
}

// =============================================================================
void RoomXEditor::resized()
{
    const float scale = (float) getWidth() / (float) kBaseW;
    content.setBounds (0, 0, kBaseW, kBaseH);
    content.setTransform (juce::AffineTransform::scale (scale));
    proc.apvts.state.setProperty (kUiScale, scale, nullptr);
    layoutContent();
}

void RoomXEditor::layoutContent()
{
    topBar.setBounds (0, 0, kBaseW, 54);
    spaceSel.setBounds (0, 60, kBaseW, 30);

    // ---- zona principal
    const int top = 96, h = 350;
    leftPanel  = { 14, top, 372, h };
    rightPanel = { kBaseW - 14 - 372, top, 372, h };

    {
        auto r = leftPanel.reduced (10, 12);
        // coluna do INPUT: o Dual Mono fica no fundo da coluna, junto ao fader de entrada
        auto inCol = r.removeFromLeft (64);
        inputFader.setBounds (inCol.withTrimmedBottom (30));
        dualMono.setBounds (inCol.removeFromBottom (24).withSizeKeepingCentre (76, 22));
        r.removeFromLeft (8);
        const int cw = r.getWidth() / 2, rh = 128;
        auto row1 = r.removeFromTop (rh), row2 = r.removeFromTop (rh);
        auto pre = row1.removeFromLeft (cw);
        preDelay.setBounds (pre.reduced (6, 0));
        preDiv.setBounds (pre.reduced (6, 0));
        mix.setBounds (row1.reduced (6, 0));
        mixLock.setBounds (mix.getRight() - 30, mix.getY() - 2, 24, 24);
        // Kill Dry junto ao MIX (canto superior esquerdo da célula), não debaixo de SIZE
        killDry.setBounds (mix.getX() + 2, mix.getY() - 2, 72, 22);
        decay.setBounds (row2.removeFromLeft (cw).reduced (6, 0));
        size.setBounds (row2.reduced (6, 0));
        r.removeFromTop (14);
        auto row3 = r.removeFromTop (30);
        preSync.setBounds (row3.removeFromLeft (cw).withSizeKeepingCentre (96, 28));
    }

    noteScape.setBounds (juce::Rectangle<int> (h + 6, h + 6).withCentre ({ kBaseW / 2, top + h / 2 }));

    {
        auto r = rightPanel.reduced (10, 12);
        outputFader.setBounds (r.removeFromRight (64));
        r.removeFromRight (8);
        const int cw = r.getWidth() / 2, rh = 128;
        auto row1 = r.removeFromTop (rh), row2 = r.removeFromTop (rh);
        target.setBounds (row1.removeFromLeft (cw).reduced (10, 8).withTrimmedBottom (4));
        position.setBounds (row1.reduced (6, 0));
        density.setBounds (row2.removeFromLeft (cw).reduced (6, 0));
        offset.setBounds (row2.reduced (6, 0));
        r.removeFromTop (14);
        auto row3 = r.removeFromTop (30);
        soloBank.setBounds (row3.withSizeKeepingCentre (110, 28));
    }

    // ---- espectro
    spectrum.setBounds (14, 454, kBaseW - 28, 98);

    // ---- selecção de notas
    tabsRow = { 14, 558, kBaseW - 28, 36 };
    noteMode.setBounds (tabsRow.withSizeKeepingCentre (390, 34));
    notesLock.setBounds (noteMode.getX() - 40, tabsRow.getY() + 3, 30, 30);
    lowerPanel = { 14, 598, kBaseW - 28, 170 };
    for (juce::Component* c : { (juce::Component*) &manualPanel, (juce::Component*) &sequencePanel, (juce::Component*) &midiPanel })
        c->setBounds (lowerPanel);

    // ---- rodapé
    footer = { 0, kBaseH - 28, kBaseW, 28 };
    bypass.setBounds (footer.getRight() - 104, footer.getY() + 3, 90, 22);
    testSig.setBounds (footer.getRight() - 336, footer.getY() + 3, 226, 22);
    measure.setBounds (footer.getRight() - 448, footer.getY() + 3, 102, 22);
    abMatch.setBounds (footer.getRight() - 560, footer.getY() + 3, 100, 22);

    login.setBounds (0, 0, kBaseW, kBaseH);
}

void RoomXEditor::Content::paint (juce::Graphics& g)
{
    auto& e = owner;
    g.fillAll (col::bg);

    // brilho subtil por trás do NoteScape
    const auto c = e.noteScape.getBounds().toFloat().getCentre();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff161a21), c.x, c.y, col::bg, c.x + 420.0f, c.y, true));
    g.fillRect (getLocalBounds().withTrimmedTop (54).withBottom (450));

    drawPanel (g, e.leftPanel.toFloat(), 14.0f);
    drawPanel (g, e.rightPanel.toFloat(), 14.0f);
    drawPanel (g, e.lowerPanel.toFloat(), 12.0f);

    // separador e títulos da zona de notas
    g.setColour (col::textFaint);
    g.setFont (Fonts::caps (12.5f, 0.14f));
    g.drawText (tr ("SELECÇÃO DE NOTAS", "NOTE SELECTION"), e.tabsRow.withRight (e.notesLock.getX() - 12), juce::Justification::centredRight);
    g.drawText (e.ctx.noteMode() == 1 ? tr ("SEGUE O TRANSPORTE DO DAW", "FOLLOWS THE DAW TRANSPORT") : (e.ctx.noteMode() == 2 ? tr ("TOQUE NOTAS OU ACORDES", "PLAY NOTES OR CHORDS") : tr ("ESCOLHA AS NOTAS", "PICK THE NOTES")),
                e.tabsRow.withLeft (e.noteMode.getRight() + 12), juce::Justification::centredLeft);

    // legendas dos cadeados (antes só havia o ícone)
    g.setColour (col::textFaint);
    g.setFont (Fonts::caps (9.5f, 0.06f));
    g.drawText (tr ("BLOQ.", "LOCK"), juce::Rectangle<int> (e.mixLock.getX() - 6, e.mixLock.getBottom() - 4, e.mixLock.getWidth() + 12, 12), juce::Justification::centred);
    g.drawText (tr ("BLOQ.", "LOCK"), juce::Rectangle<int> (e.notesLock.getX() - 6, e.notesLock.getBottom() - 4, e.notesLock.getWidth() + 12, 12), juce::Justification::centred);

    // rodapé
    g.setColour (juce::Colour (0xff0a0b0e));
    g.fillRect (e.footer);
    g.setColour (col::line);
    g.fillRect (e.footer.withHeight (1));
    auto f = e.footer.reduced (16, 0);
    g.setColour (col::textFaint);
    g.setFont (Fonts::text (14.0f));
    g.drawText (U ("ROOM-X by Piradex  ·  v") + ROOMX_VERSION_STRING + U ("  ·  Beatfreak Developer"), f, juce::Justification::centredLeft);
    auto& L = lic::License::get();
    if (L.isUnlocked())
    {
        g.setColour (L.kind() == lic::Kind::Beta ? col::warn : col::textDim);
        g.drawText (L.statusText(), f.withTrimmedRight (110), juce::Justification::centred);
    }
    juce::String right;
    if (e.proc.apvts.getRawParameterValue (P::eco)->load() > 0.5f) right << "ECO  ";
    if (std::abs (e.proc.apvts.getRawParameterValue (P::tuneRef)->load() - 440.0f) > 0.05f
        || std::abs (e.proc.apvts.getRawParameterValue (P::tuneCents)->load()) > 0.05f)
        right << "A4 " << juce::String (e.proc.apvts.getRawParameterValue (P::tuneRef)->load(), 1) << " Hz  ";
    g.setColour (col::warn);
    g.drawText (right, f.withTrimmedRight (580), juce::Justification::centredRight);   // deixa espaço aos controlos do sinal de teste
}

void RoomXEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
}

// =============================================================================
void RoomXEditor::timerCallback()
{
    ++tickCount;

    // estado da medição do RT60 no botão do rodapé
    {
        const int st = proc.rt60State();
        if (st == 3)
        {
            const double r = proc.rt60Result();
            measure.setButtonText (r > 0.0 ? "RT60 " + juce::String (r, 2) + " s" : tr ("sem decaimento", "no decay"));
        }
        else if (st == 1 || st == 2)
            measure.setButtonText (tr ("A medir…", "Measuring…"));
        else
            measure.setButtonText (tr ("Medir RT60", "Measure RT60"));
    }
    proc.applyAbMatch();   // o trim de saída acompanha o slot activo

    // licença (ex.: beta expirou com o plugin aberto)
    lic::License::get().checkExpiry();
    const bool lic = lic::License::get().isUnlocked();
    if (lic != wasLicensed)
    {
        wasLicensed = lic;
        login.setVisible (! lic);
        content.repaint();
    }

    // cor de acento
    accent.target = col::forTarget ((int) proc.apvts.getRawParameterValue (P::target)->load());
    if (accent.step (0.24f))
        content.repaint();

    // analisador + visualizadores
    const bool eco = proc.apvts.getRawParameterValue (P::eco)->load() > 0.5f;
    analyzer.eco = eco;
    const float lo = proc.apvts.getRawParameterValue (P::rangeLow)->load();
    const float hi = proc.apvts.getRawParameterValue (P::rangeHigh)->load();
    if (! eco || (tickCount & 1) == 0)
    {
        analyzer.update (proc, juce::jmin (lo, hi), juce::jmax (lo, hi), proc.currentTuning());
        spectrum.repaint();
    }
    noteScape.tick();

    // medidores
    inputFader.setLevels (proc.ui.inPeak[0].exchange (0.0f), proc.ui.inPeak[1].exchange (0.0f));
    outputFader.setLevels (proc.ui.outPeak[0].exchange (0.0f), proc.ui.outPeak[1].exchange (0.0f));

    // painéis
    updateVisibility();
    if (sequencePanel.isVisible()) sequencePanel.tick();
    if (midiPanel.isVisible()) midiPanel.tick();
    if (manualPanel.isVisible() && (tickCount % 3) == 0) manualPanel.repaint();
    if ((tickCount % 6) == 0)
    {
        topBar.tick();
        content.repaint (footer);
        content.repaint (tabsRow);
        spaceSel.repaint();
        mix.repaint();
    }
}

void RoomXEditor::mouseDown (const juce::MouseEvent&)
{
    // cada gesto do utilizador = um passo de undo
    proc.undoManager.beginNewTransaction();
}

bool RoomXEditor::keyPressed (const juce::KeyPress& k)
{
    const auto mods = k.getModifiers();
    if (mods.isCommandDown() && k.getKeyCode() == 'Z')
    {
        if (mods.isShiftDown()) proc.undoManager.redo(); else proc.undoManager.undo();
        return true;
    }
    if (mods.isCommandDown() && k.getKeyCode() == 'Y') { proc.undoManager.redo(); return true; }
    return false;
}
