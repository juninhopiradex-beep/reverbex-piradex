// =============================================================================
//  ROOM-X — testes de integração com o plugin real
//   · licença: PBKDF2 (vectores RFC), senha errada / master / beta / expirada
//   · plugin bloqueado = áudio bit-transparente
//   · reverb a funcionar depois de desbloquear, bypass transparente
//   · estado: guardar / repor / duplicar instância, campos futuros ignorados
//   · 60+ presets de fábrica + 11 tutoriais carregam e soam (sem NaN)
//   · taxas 44.1–192 kHz e blocos 16–4096 (incl. tamanhos irregulares)
//   · capturas da interface (Early / All / Late, Sequence, MIDI, login)
//
//  Senhas reais (opcional, nunca no repositório):
//    ROOMX_TEST_MASTER=... ROOMX_TEST_BETA=... ./roomx_plugin_test [pasta-capturas]
// =============================================================================
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "licensing/License.h"
#include <cstdio>
#include <random>

using namespace roomx;
namespace P = roomx::params;

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    std::printf ("  %-64s %s\n", what.toRawUTF8(), ok ? "OK" : "FALHOU");
    if (! ok) ++failures;
}

static void check (bool ok, const char* what) { check (ok, juce::String::fromUTF8 (what)); }

static void setParam (RoomXProcessor& p, const char* id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}
static float getParam (RoomXProcessor& p, const char* id)
{
    auto* prm = p.apvts.getParameter (id);
    return prm->convertFrom0to1 (prm->getValue());
}

static void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

static void fillNoise (juce::AudioBuffer<float>& b, unsigned seed)
{
    std::mt19937 g (seed);
    std::uniform_real_distribution<float> d (-0.5f, 0.5f);
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i) b.setSample (c, i, d (g));
}

static bool allFinite (const juce::AudioBuffer<float>& b, float& peak)
{
    peak = 0.0f;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float v = b.getSample (c, i);
            if (! std::isfinite (v)) return false;
            peak = juce::jmax (peak, std::abs (v));
        }
    return true;
}

// -----------------------------------------------------------------------------
static void testLicensing()
{
    std::printf ("[licença]\n");
    using L = lic::LocalPasswordLicenseProvider;
    auto hex = [] (const juce::MemoryBlock& m) { return juce::String::toHexString (m.getData(), (int) m.getSize(), 0); };
    const juce::MemoryBlock salt ("salt", 4);
    check (hex (L::pbkdf2 ("password", salt, 1, 32)) == "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b", "PBKDF2-HMAC-SHA256 vector RFC (c=1)");
    check (hex (L::pbkdf2 ("password", salt, 4096, 32)) == "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a", "PBKDF2-HMAC-SHA256 vector RFC (c=4096)");

    auto& lic = lic::License::get();
    lic.lock();
    check (! lic.isUnlocked(), "bloqueado depois de lock()");

    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    bool expired = false;
    check (lic.matchPassword ("senha-errada", expired) < 0 && ! expired, "senha errada recusada");
    std::printf ("    (verificação PBKDF2 de todas as chaves: %.0f ms)\n", juce::Time::getMillisecondCounterHiRes() - t0);
    check (lic.applyMatch (-1, false, false) == lic::Result::Wrong, "applyMatch(-1) = Wrong");

    lic::KeyEntry past { "x", lic::Kind::Beta, "", "", 2020, 1, 1 };
    lic::KeyEntry future { "y", lic::Kind::Beta, "", "", 2099, 12, 31 };
    check (L::isExpired (past) && ! L::isExpired (future), "validade BETA (passado expira, futuro não)");

    for (int i = 0; i < L::numKeys(); ++i)
    {
        const auto& k = L::key (i);
        check (juce::String (k.saltHex).length() == 32 && juce::String (k.hashHex).length() == 64,
               juce::String ("chave '") + k.id + "' só tem salt+hash (sem senha em claro)");
    }

    for (auto [env, kind] : { std::pair<const char*, lic::Kind> { "ROOMX_TEST_MASTER", lic::Kind::Master },
                              std::pair<const char*, lic::Kind> { "ROOMX_TEST_BETA",   lic::Kind::Beta } })
    {
        const auto pw = juce::SystemStats::getEnvironmentVariable (env, {});
        if (pw.isEmpty()) { std::printf ("    (%s não definida — teste saltado)\n", env); continue; }
        const int idx = lic.matchPassword (pw, expired);
        check (idx >= 0 && L::key (idx).kind == kind, juce::String (env) + " aceite com o tipo certo");
        if (idx >= 0)
        {
            check (lic.applyMatch (idx, expired, false) == lic::Result::Ok && lic.isUnlocked(), juce::String (env) + " desbloqueia");
            check (lic.statusText().isNotEmpty(), "texto de estado: " + lic.statusText());
            lic.lock();
        }
    }
    int secs = 0;
    for (int i = 0; i < 5; ++i) lic::License::registerFailure();
    check (! lic::License::canAttempt (secs) && secs > 0, "5 falhas -> espera anti força-bruta");
    lic::License::registerSuccess();
}

// -----------------------------------------------------------------------------
static void testTransparencyAndReverb()
{
    std::printf ("[áudio]\n");
    auto& lic = lic::License::get();
    lic.lock();
    RoomXProcessor p;
    setParam (p, P::mix, 80.0f);
    setParam (p, P::temper, 70.0f);
    p.prepareToPlay (48000.0, 512);
    juce::MidiBuffer midi;

    juce::AudioBuffer<float> in (2, 512), buf (2, 512);
    bool bitExact = true;
    for (int blk = 0; blk < 50; ++blk)
    {
        fillNoise (in, (unsigned) blk + 1);
        buf.makeCopyOf (in);
        p.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i)
                if (buf.getSample (c, i) != in.getSample (c, i)) bitExact = false;
    }
    check (bitExact, "sem licença: saída == entrada (bit a bit)");

#if defined (ROOMX_ALLOW_TEST_UNLOCK)
    static_cast<lic::LocalPasswordLicenseProvider&> (lic).unlockForTests();
#endif
    check (lic.isUnlocked(), "desbloqueado para os testes");

    // impulso -> cauda
    setParam (p, P::mix, 100.0f);
    setParam (p, P::killDry, 1.0f);
    setParam (p, P::temper, 0.0f);
    p.prepareToPlay (48000.0, 512);
    double tailEnergy = 0.0;
    for (int blk = 0; blk < 200; ++blk)
    {
        buf.clear();
        if (blk == 0) { buf.setSample (0, 0, 1.0f); buf.setSample (1, 0, 1.0f); }
        p.processBlock (buf, midi);
        if (blk > 10)
            for (int c = 0; c < 2; ++c) tailEnergy += buf.getRMSLevel (c, 0, 512);
    }
    check (tailEnergy > 0.01, "com licença: o impulso gera cauda de reverb");

    // bypass: depois do fade, saída == entrada
    setParam (p, P::bypass, 1.0f);
    bool bypassOk = true;
    for (int blk = 0; blk < 20; ++blk)
    {
        fillNoise (in, 100 + (unsigned) blk);
        buf.makeCopyOf (in);
        p.processBlock (buf, midi);
        if (blk > 3)
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    if (std::abs (buf.getSample (c, i) - in.getSample (c, i)) > 1.0e-6f) bypassOk = false;
    }
    check (bypassOk, "bypass: saída == entrada após o fade");
    setParam (p, P::bypass, 0.0f);

    // Kill Dry + Mix 0 -> silêncio
    setParam (p, P::mix, 0.0f);
    float peak = 1.0f;
    for (int blk = 0; blk < 30; ++blk)
    {
        fillNoise (buf, 300 + (unsigned) blk);
        p.processBlock (buf, midi);
        allFinite (buf, peak);
    }
    check (peak < 1.0e-4f, "Kill Dry + Mix 0% = silêncio");

    // MIDI: nota tocada aparece no conjunto de notas
    setParam (p, P::killDry, 0.0f);
    setParam (p, P::mix, 50.0f);
    setParam (p, P::noteMode, 2.0f);
    setParam (p, P::temper, 80.0f);
    juce::MidiBuffer m2;
    m2.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);
    fillNoise (buf, 7);
    p.processBlock (buf, m2);
    music::NoteSet ns;
    for (int i = 0; i < 3; ++i) ns.w[i] = p.ui.notes[i].load();
    check (ns.test (64) && ns.test (76) && ! ns.test (52), "MIDI E4 + Octave Repeat -> E4, E5... (não abaixo)");
    m2.clear();
    m2.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
    m2.addEvent (juce::MidiMessage::noteOff (1, 64), 1);
    p.processBlock (buf, m2);
    for (int i = 0; i < 3; ++i) ns.w[i] = p.ui.notes[i].load();
    check (ns.test (64), "pedal CC64 mantém a nota depois do note-off");
    m2.clear();
    m2.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);
    p.processBlock (buf, m2);
    for (int i = 0; i < 3; ++i) ns.w[i] = p.ui.notes[i].load();
    check (ns.empty(), "soltar o pedal liberta a nota");
}

// -----------------------------------------------------------------------------
static void testState()
{
    std::printf ("[estado]\n");
    RoomXProcessor a;
    setParam (a, P::temper, -42.0f);
    setParam (a, P::noteWidth, 17.0f);
    setParam (a, P::target, 2.0f);
    setParam (a, P::space, 12.0f);
    setParam (a, P::notes, (float) 0x0A5);
    setParam (a, P::tuneRef, 432.0f);
    setParam (a, P::rangeLow, 40.0f);
    a.sequence.setRow (3, music::Harmony { 9, music::Category::Mode, 1 });
    a.sequence.addRegion ({ 3, 16.0, 8.0 });
    a.setUseFlats (true);

    juce::MemoryBlock mb;
    a.getStateInformation (mb);
    RoomXProcessor b;
    b.setStateInformation (mb.getData(), (int) mb.getSize());
    pump (50);

    check (std::abs (getParam (b, P::temper) + 42.0f) < 0.05f, "Temper reposto");
    check (std::abs (getParam (b, P::noteWidth) - 17.0f) < 0.05f, "Note Width reposto");
    check ((int) getParam (b, P::target) == 2 && (int) getParam (b, P::space) == 12, "Target e espaço repostos");
    check ((int) getParam (b, P::notes) == 0x0A5, "notas Manual repostas");
    check (std::abs (getParam (b, P::tuneRef) - 432.0f) < 0.05f, "afinação A4 = 432 Hz reposta");
    check (b.sequence.getRow (3).category == music::Category::Mode && b.sequence.getRow (3).root == 9, "linha da sequência reposta");
    check (b.sequence.getNumRegions() == a.sequence.getNumRegions(), "regiões da sequência repostas");
    check (b.useFlats(), "preferência de bemóis reposta");

    // estado com campos desconhecidos (versão futura) não falha
    auto xml = a.apvts.copyState().createXml();
    xml->setAttribute ("stateVersion", 99);
    xml->createNewChildElement ("FUTURE_THING")->setAttribute ("x", 1);
    juce::MemoryBlock mb2;
    juce::AudioProcessor::copyXmlToBinary (*xml, mb2);
    RoomXProcessor c;
    c.setStateInformation (mb2.getData(), (int) mb2.getSize());
    check (std::abs (getParam (c, P::temper) + 42.0f) < 0.05f, "estado de versão futura carrega o que conhece");

    // estado sem sequência (sessão antiga) -> sequência por defeito
    auto xml3 = a.apvts.copyState().createXml();
    xml3->deleteAllChildElementsWithTagName ("SEQUENCE");
    juce::MemoryBlock mb3;
    juce::AudioProcessor::copyXmlToBinary (*xml3, mb3);
    RoomXProcessor d;
    d.setStateInformation (mb3.getData(), (int) mb3.getSize());
    check (d.sequence.getNumRegions() == 4, "sessão sem sequência -> sequência por defeito");

    // undo/redo de parâmetros
    RoomXProcessor u;
    u.undoManager.beginNewTransaction();
    setParam (u, P::size, 150.0f);
    pump (120);
    u.undoManager.beginNewTransaction();
    u.undoManager.undo();
    pump (60);
    check (std::abs (getParam (u, P::size) - 100.0f) < 0.5f, "Undo repõe Size");
}

// -----------------------------------------------------------------------------
static void testPresets()
{
    std::printf ("[presets]\n");
    RoomXProcessor p;
    auto& pm = *p.presets;
    int factory = 0, tutorial = 0;
    for (auto& e : pm.entries())
        if (e.factory) { ++factory; if (e.bank == "Tutorial") ++tutorial; }
    check (factory - tutorial >= 60, "presets de fábrica >= 60 (" + juce::String (factory - tutorial) + ")");
    check (tutorial >= 11, "presets tutoriais >= 11 (" + juce::String (tutorial) + ")");

    juce::StringArray names;
    bool unique = true;
    for (auto& e : pm.entries()) { if (names.contains (e.name)) unique = false; names.add (e.name); }
    check (unique, "nomes de preset únicos");

    p.prepareToPlay (48000.0, 256);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf (2, 256);
    bool ok = true;
    float worst = 0.0f;
    juce::String worstName;
    for (int i = 0; i < pm.entries().size(); ++i)
    {
        if (! pm.entries()[i].factory) continue;
        pm.load (i);
        for (int blk = 0; blk < 40; ++blk)
        {
            fillNoise (buf, (unsigned) (i * 100 + blk));
            buf.applyGain (0.5f);
            p.processBlock (buf, midi);
            float pk;
            if (! allFinite (buf, pk)) { ok = false; std::printf ("    NaN em '%s'\n", pm.entries()[i].name.toRawUTF8()); break; }
            if (pk > worst) { worst = pk; worstName = pm.entries()[i].name; }
        }
        if (pm.isDirty()) { ok = false; std::printf ("    '%s' fica 'sujo' logo depois de carregar\n", pm.entries()[i].name.toRawUTF8()); }
    }
    check (ok, "todos os presets carregam, soam e não ficam 'sujos'");
    check (worst < 8.0f, "pico máximo razoável: " + juce::String (worst, 2) + " (" + worstName + ")");

    // Note Lock: as notas não mudam ao carregar outro preset
    setParam (p, P::notesLock, 1.0f);
    setParam (p, P::notes, 0x001);
    pm.load (5);
    check ((int) getParam (p, P::notes) == 0x001, "Note Lock protege as notas");
    setParam (p, P::notesLock, 0.0f);

    // A/B
    setParam (p, P::decay, 2.0f);
    pm.selectSlot (1);
    setParam (p, P::decay, 5.0f);
    pm.selectSlot (0);
    const float a = getParam (p, P::decay);
    pm.selectSlot (1);
    const float b = getParam (p, P::decay);
    check (std::abs (a - 2.0f) < 0.01f && std::abs (b - 5.0f) < 0.01f, "A/B guarda dois estados");
    pm.copyBtoA();
    pm.selectSlot (0);
    check (std::abs (getParam (p, P::decay) - 5.0f) < 0.01f, "Copiar B -> A");

    // gravar / importar
    auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("roomx_test_preset.roomx");
    if (auto xml = pm.capture ("Teste", "User").createXml()) xml->writeTo (tmp);
    setParam (p, P::decay, 9.0f);
    check (pm.loadFile (tmp) && std::abs (getParam (p, P::decay) - 5.0f) < 0.01f, "ler preset .roomx de ficheiro");
    tmp.deleteFile();
}

// -----------------------------------------------------------------------------
static void testRates()
{
    std::printf ("[taxas / blocos]\n");
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
    {
        RoomXProcessor p;
        setParam (p, P::temper, 85.0f);
        setParam (p, P::offset, 120.0f);
        setParam (p, P::target, 1.0f);
        setParam (p, P::mix, 60.0f);
        bool ok = true;
        for (int bs : { 16, 64, 333, 1024, 4096 })
        {
            p.prepareToPlay (sr, bs);
            juce::MidiBuffer midi;
            juce::AudioBuffer<float> buf (2, bs);
            for (int blk = 0; blk < 8; ++blk)
            {
                fillNoise (buf, (unsigned) (bs + blk));
                // o host pode mandar blocos mais pequenos do que o preparado
                juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), 2, juce::jmax (1, bs - blk * 3));
                p.processBlock (view, midi);
                float pk;
                if (! allFinite (view, pk) || pk > 20.0f) ok = false;
            }
        }
        check (ok, juce::String (sr / 1000.0, 1) + " kHz, blocos 16..4096: estável e finito");
    }

    // entrada mono -> saída estéreo
    RoomXProcessor p;
    p.setPlayConfigDetails (1, 2, 48000.0, 512);
    p.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    fillNoise (buf, 3);
    buf.clear (1, 0, 512);
    p.processBlock (buf, midi);
    float pk;
    check (allFinite (buf, pk), "layout mono -> estéreo");
}

// -----------------------------------------------------------------------------
static void snapshots (const juce::File& dir)
{
    std::printf ("[capturas da interface] -> %s\n", dir.getFullPathName().toRawUTF8());
    dir.createDirectory();
    RoomXProcessor p;
    p.prepareToPlay (48000.0, 512);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf (2, 512);

    auto render = [&] (const char* name, int w)
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (w, juce::roundToInt (w * (double) RoomXEditor::kBaseH / RoomXEditor::kBaseW));
        // alimenta áudio para o analisador/NoteScape
        for (int k = 0; k < 40; ++k)
        {
            fillNoise (buf, (unsigned) k);
            buf.applyGain (0.4f);
            p.processBlock (buf, midi);
            pump (12);
        }
        pump (400);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto f = dir.getChildFile (name);
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("    %s (%dx%d)\n", name, img.getWidth(), img.getHeight());
    };

    auto& lic = lic::License::get();
    lic.lock();
    render ("roomx_login.png", 1180);
#if defined (ROOMX_ALLOW_TEST_UNLOCK)
    static_cast<lic::LocalPasswordLicenseProvider&> (lic).unlockForTests();
#endif

    p.presets->load (0);
    setParam (p, P::notes, 0x091);          // C E G
    setParam (p, P::temper, 60.0f);
    setParam (p, P::noteWidth, 58.0f);
    setParam (p, P::rangeLow, 36.0f);
    setParam (p, P::rangeHigh, 108.0f);
    setParam (p, P::lowCut, 90.0f);
    setParam (p, P::highCut, 12000.0f);
    setParam (p, P::target, 1.0f);
    render ("roomx_all.png", 1180);
    setParam (p, P::target, 0.0f);
    render ("roomx_early.png", 1180);
    setParam (p, P::target, 2.0f);
    setParam (p, P::temper, -55.0f);
    render ("roomx_late_negative.png", 1180);
    setParam (p, P::target, 1.0f);
    setParam (p, P::temper, 60.0f);
    setParam (p, P::noteMode, 1.0f);
    render ("roomx_sequence.png", 1180);
    setParam (p, P::noteMode, 2.0f);
    render ("roomx_midi.png", 900);
}

// -----------------------------------------------------------------------------
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    testLicensing();
    testTransparencyAndReverb();
    testState();
    testPresets();
    testRates();
    if (argc > 1) snapshots (juce::File (juce::String (argv[1])));

    lic::License::get().lock();       // não deixar a máquina de testes desbloqueada

    // opcional: deixar esta máquina "lembrada" (para correr o pluginval com o DSP activo)
    if (juce::SystemStats::getEnvironmentVariable ("ROOMX_TEST_KEEP_UNLOCKED", {}).isNotEmpty())
    {
        bool expired = false;
        const int idx = lic::License::get().matchPassword (juce::SystemStats::getEnvironmentVariable ("ROOMX_TEST_MASTER", {}), expired);
        if (idx >= 0) lic::License::get().applyMatch (idx, expired, true);
        std::printf ("  (máquina deixada desbloqueada: %s)\n", idx >= 0 ? "sim" : "não");
    }
    std::printf (failures == 0 ? "\nTODOS OS TESTES PASSARAM\n" : "\n%d TESTE(S) FALHARAM\n", failures);
    return failures == 0 ? 0 : 1;
}
