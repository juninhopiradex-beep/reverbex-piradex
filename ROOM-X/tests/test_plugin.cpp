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
//  Capturas de marketing a 2× (ROOMX_CAPTURAS=pasta): presets reais, sinal
//  musical (acorde em plucks + bombo + ruído rosa) e transporte simulado.
// -----------------------------------------------------------------------------
struct FakePlayHead : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 120.0;
    bool playing = false;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setIsPlaying (playing);
        p.setBpm (bpm);
        p.setPpqPosition (ppq);
        p.setTimeSignature (TimeSignature { 4, 4 });
        return p;
    }
};

struct MusicSignal
{
    double fs = 48000.0;
    long long n = 0;
    float pink[7] {};
    std::mt19937 rng { 7 };
    std::vector<int> notes { 53, 57, 60, 64 };       // F maj7
    float sample (int ch)
    {
        const double t = (double) n / fs;
        const double beat = t * 2.0;                  // 120 bpm
        const double ph = beat - std::floor (beat);
        // plucks do acorde, um a cada colcheia, com envelope
        const double eighth = t * 4.0;
        const int idx = (int) std::floor (eighth) % (int) notes.size();
        const double pe = eighth - std::floor (eighth);
        const double f = 440.0 * std::pow (2.0, (notes[(size_t) idx] + 12 * (idx % 2) - 69) / 12.0);
        double pl = 0.0;
        for (int h = 1; h <= 6; ++h) pl += std::sin (2.0 * juce::MathConstants<double>::pi * f * h * t) / (h * 1.4);
        pl *= std::exp (-pe * 5.0) * 0.22;
        // bombo
        const double kf = 50.0 + 90.0 * std::exp (-ph * 30.0);
        const double kick = std::sin (2.0 * juce::MathConstants<double>::pi * kf * ph * 0.5) * std::exp (-ph * 9.0) * 0.45;
        // ruído rosa suave
        std::uniform_real_distribution<float> d (-1.0f, 1.0f);
        const float w = d (rng);
        pink[0] = 0.99886f * pink[0] + w * 0.0555179f; pink[1] = 0.99332f * pink[1] + w * 0.0750759f;
        pink[2] = 0.96900f * pink[2] + w * 0.1538520f; pink[3] = 0.86650f * pink[3] + w * 0.3104856f;
        const float pk = (pink[0] + pink[1] + pink[2] + pink[3]) * 0.02f;
        if (ch == 1) ++n;
        return (float) (pl * (ch == 0 ? 1.0 : 0.85) + kick) + pk;
    }
};

static void marketing (const juce::File& dir)
{
    std::printf ("[capturas de marketing a 2x] -> %s\n", dir.getFullPathName().toRawUTF8());
    dir.createDirectory();
#if defined (ROOMX_ALLOW_TEST_UNLOCK)
    static_cast<lic::LocalPasswordLicenseProvider&> (lic::License::get()).unlockForTests();
#endif
    auto shot = [&] (const char* presetName, const char* file, std::function<void (RoomXProcessor&)> tweak, bool play, std::vector<int> chord)
    {
        RoomXProcessor p;
        FakePlayHead head;
        p.setPlayHead (&head);
        p.prepareToPlay (48000.0, 512);
        for (int i = 0; i < p.presets->entries().size(); ++i)
            if (p.presets->entries()[i].name == presetName) p.presets->load (i);
        if (tweak) tweak (p);
        MusicSignal sig;
        sig.notes = std::move (chord);
        head.playing = play;
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buf (2, 512);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (RoomXEditor::kBaseW, RoomXEditor::kBaseH);
        for (int k = 0; k < 600; ++k)        // ~6.4 s de música
        {
            for (int i = 0; i < 512; ++i) { buf.setSample (0, i, sig.sample (0)); buf.setSample (1, i, sig.sample (1)); }
            p.processBlock (buf, midi);
            head.ppq += 512.0 / 48000.0 * 2.0;
            if (k % 3 == 0) pump (10);
        }
        pump (500);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        auto f = dir.getChildFile (file);
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("    %s  %s (%dx%d)\n", file, p.presets->currentName().toRawUTF8(), img.getWidth(), img.getHeight());
        p.setPlayHead (nullptr);
    };

    shot ("Major 7 Hall", "roomx-principal@2x.png", nullptr, false, { 53, 57, 60, 64 });
    shot ("06 Early Only", "roomx-early@2x.png", nullptr, false, { 50, 53, 57, 60 });
    shot ("Pluck Shimmer Tail", "roomx-late@2x.png", nullptr, false, { 55, 59, 62, 66 });
    shot ("Carved Vocal Space", "roomx-negativo@2x.png", nullptr, false, { 55, 59, 62, 67 });
    shot ("Pop Progression Hall", "roomx-sequence@2x.png", nullptr, true, { 48, 52, 55, 60 });
    shot ("MIDI Chord Follower", "roomx-midi@2x.png", [] (RoomXProcessor& pr)
    {
        juce::MidiBuffer m;
        for (int n : { 45, 52, 57, 60, 64 }) m.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
        juce::AudioBuffer<float> b (2, 512);
        b.clear();
        pr.processBlock (b, m);
    }, false, { 45, 52, 57, 60, 64 });
}

// -----------------------------------------------------------------------------
//  Idioma PT/EN: tr(), troca imediata, persistência no ficheiro Piradex
// -----------------------------------------------------------------------------
static void testLanguage()
{
    std::printf ("[idioma PT/EN]\n");
    namespace L = piradex::i18n;
    const auto before = L::current();
    L::set (L::Language::PT, false);
    check (L::tr ("Guardar", "Save") == "Guardar", "tr devolve PT");
    check (L::tr ("Afinação", "Tuning") == juce::String::fromUTF8 ("Afinação"), "tr PT em UTF-8");
    L::set (L::Language::EN, false);
    check (L::tr ("Guardar", "Save") == "Save", "tr devolve EN");

    struct Count : L::Listener { int n = 0; void languageChanged() override { ++n; } } count;
    L::addListener (&count);
    L::set (L::Language::PT, false);
    L::set (L::Language::PT, false);
    check (count.n == 1, "aviso de troca só quando muda");
    L::removeListener (&count);

    // persistência (ficheiro comum a todos os plugins Piradex)
    L::set (L::Language::EN, true);
    L::set (L::Language::PT, false);
    L::reload();
    check (L::current() == L::Language::EN, "idioma guardado e relido do ficheiro Piradex");

    // o editor troca na hora e os nomes de automação não mudam
    RoomXProcessor p;
    p.prepareToPlay (48000.0, 512);
    const auto nameBefore = p.apvts.getParameter (P::decay)->getName (64);
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        L::set (L::Language::PT, false);
        pump (50);
        L::set (L::Language::EN, false);
        pump (50);
    }
    check (p.apvts.getParameter (P::decay)->getName (64) == nameBefore, "nomes de automação fixos");
    L::set (before, true);
}

// -----------------------------------------------------------------------------
//  Capturas PT / EN a 2× (ROOMX_CAPTURAS_IDIOMA=pasta) — mesmo editor, troca
//  de idioma em directo entre as duas fotos.
// -----------------------------------------------------------------------------
static void languageShots (const juce::File& dir)
{
    namespace L = piradex::i18n;
    std::printf ("[capturas PT/EN a 2x] -> %s\n", dir.getFullPathName().toRawUTF8());
    dir.createDirectory();
    const auto before = L::current();
    auto save = [&] (const juce::Image& img, const juce::String& file)
    {
        auto f = dir.getChildFile (file);
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("    %s (%dx%d)\n", file.toRawUTF8(), img.getWidth(), img.getHeight());
    };

    auto run = [&] (const char* presetName, const char* tag, std::function<void (RoomXProcessor&)> tweak, bool play,
                    std::vector<int> chord, bool panels)
    {
        RoomXProcessor p;
        FakePlayHead head;
        p.setPlayHead (&head);
        p.prepareToPlay (48000.0, 512);
        for (int i = 0; i < p.presets->entries().size(); ++i)
            if (p.presets->entries()[i].name == presetName) p.presets->load (i);
        if (tweak) tweak (p);
        MusicSignal sig;
        sig.notes = std::move (chord);
        head.playing = play;
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buf (2, 512);
        L::set (L::Language::PT, false);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (RoomXEditor::kBaseW, RoomXEditor::kBaseH);
        auto feed = [&] (int blocks)
        {
            for (int k = 0; k < blocks; ++k)
            {
                for (int i = 0; i < 512; ++i) { buf.setSample (0, i, sig.sample (0)); buf.setSample (1, i, sig.sample (1)); }
                p.processBlock (buf, midi);
                head.ppq += 512.0 / 48000.0 * 2.0;
                if (k % 3 == 0) pump (10);
            }
        };
        feed (600);
        for (auto lang : { L::Language::PT, L::Language::EN })
        {
            L::set (lang, false);          // troca em directo no mesmo editor
            feed (30);
            pump (400);
            const juce::String suf = lang == L::Language::PT ? "pt" : "en";
            save (ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f), juce::String (tag) + "-" + suf + "@2x.png");
            // janela flutuante sobre fundo opaco (como aparece na CallOutBox)
            auto panelShot = [] (juce::Component& c)
            {
                juce::Image img (juce::Image::ARGB, c.getWidth() * 2, c.getHeight() * 2, true);
                juce::Graphics g (img);
                g.fillAll (juce::Colour (0xff14171d));   // col::panel
                g.addTransform (juce::AffineTransform::scale (2.0f));
                c.paintEntireComponent (g, true);
                return img;
            };
            if (panels)
            {
                auto& ctx = static_cast<RoomXEditor*> (ed.get())->uiContext();
                roomx::ui::InfoPanel info (ctx, {});
                info.setLookAndFeel (&ed->getLookAndFeel());
                save (panelShot (info), "info-" + suf + "@2x.png");
                roomx::ui::TuningPanel tun (ctx);
                tun.setLookAndFeel (&ed->getLookAndFeel());
                save (panelShot (tun), "afinacao-" + suf + "@2x.png");
                roomx::ui::ScaleBrowser sb (ctx, [] (const roomx::music::Harmony&) {}, roomx::music::Harmony { 2, roomx::music::Category::Mode, 1 });
                sb.setLookAndFeel (&ed->getLookAndFeel());
                save (panelShot (sb), "escalas-" + suf + "@2x.png");
                info.setLookAndFeel (nullptr); tun.setLookAndFeel (nullptr); sb.setLookAndFeel (nullptr);
            }
        }
        ed.reset();
        p.setPlayHead (nullptr);
    };

#if defined (ROOMX_ALLOW_TEST_UNLOCK)
    // ecrã de login nas duas línguas
    lic::License::get().lock();
    run ("Major 7 Hall", "login", nullptr, false, { 53, 57, 60, 64 }, false);
    static_cast<lic::LocalPasswordLicenseProvider&> (lic::License::get()).unlockForTests();
#endif
    run ("Major 7 Hall", "principal", [] (RoomXProcessor& pr) { setParam (pr, P::noteMode, 0.0f); }, false, { 53, 57, 60, 64 }, true);
    run ("Pop Progression Hall", "sequence", nullptr, true, { 48, 52, 55, 60 }, false);
    run ("MIDI Chord Follower", "midi", [] (RoomXProcessor& pr)
    {
        juce::MidiBuffer m;
        for (int n : { 45, 52, 57, 60, 64 }) m.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
        juce::AudioBuffer<float> b (2, 512);
        b.clear();
        pr.processBlock (b, m);
    }, false, { 45, 52, 57, 60, 64 }, false);
    L::set (before, false);
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
    testLanguage();
    if (argc > 1) snapshots (juce::File (juce::String (argv[1])));
    {
        const auto capDir = juce::SystemStats::getEnvironmentVariable ("ROOMX_CAPTURAS", {});
        if (capDir.isNotEmpty()) marketing (juce::File (capDir));
        const auto langDir = juce::SystemStats::getEnvironmentVariable ("ROOMX_CAPTURAS_IDIOMA", {});
        if (langDir.isNotEmpty()) languageShots (juce::File (langDir));
    }

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
