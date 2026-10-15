// =============================================================================
//  ROOM-X — presets de fábrica (60) + tutoriais (11). Nomes e definições
//  originais Piradex. Valores em unidades reais (%, ms, Hz, x...).
//  Parâmetros não indicados ficam no valor por defeito.
// =============================================================================
#include "PresetManager.h"
#include "Parameters.h"
#include "../dsp/SpaceLibrary.h"

namespace roomx::state
{
namespace
{
using namespace roomx::music;
namespace P = roomx::params;

uint16_t pcMask (std::initializer_list<int> pcs)
{
    uint16_t m = 0;
    for (int p : pcs) m = (uint16_t) (m | (1u << (p % 12)));
    return m;
}

Harmony H (int root, Category c, const char* typeName)
{
    int idx = 0;
    const bool found = findType (c, typeName, idx);
    jassert (found); juce::ignoreUnused (found);   // nome de tipo inválido num preset
    return Harmony { ((root % 12) + 12) % 12, c, idx };
}

uint16_t chord (int root, const char* t)  { return H (root, Category::Chord, t).mask(); }
uint16_t scale (int root, const char* t)  { return H (root, Category::Scale, t).mask(); }
uint16_t mode  (int root, const char* t)  { return H (root, Category::Mode, t).mask(); }

enum { C = 0, Cs = 1, D = 2, Eb = 3, E = 4, F = 5, Fs = 6, G = 7, Ab = 8, A = 9, Bb = 10, B = 11 };

int spaceIndex (const char* name)
{
    for (int i = 0; i < dsp::kNumSpaces; ++i)
        if (juce::String (dsp::space (i).name) == name) return i;
    jassertfalse;
    return 0;
}

struct PB
{
    juce::ValueTree t { pids::preset };

    PB (const char* name, const char* bank, const char* info = "")
    {
        t.setProperty (pids::name, name, nullptr);
        t.setProperty (pids::bank, bank, nullptr);
        t.setProperty (pids::version, kPresetSchemaVersion, nullptr);
        if (*info) t.setProperty (pids::info, juce::String::fromUTF8 (info), nullptr);
    }
    PB& set (const char* id, double v)
    {
        auto existing = t.getChildWithProperty (pids::id, id);
        if (existing.isValid()) { existing.setProperty (pids::value, v, nullptr); return *this; }
        juce::ValueTree p (pids::param);
        p.setProperty (pids::id, id, nullptr);
        p.setProperty (pids::value, v, nullptr);
        t.appendChild (p, nullptr);
        return *this;
    }
    // atalhos
    PB& sp (const char* s)        { return set (P::space, spaceIndex (s)); }
    PB& mix (double v)            { return set (P::mix, v); }
    PB& dec (double v)            { return set (P::decay, v); }
    PB& size (double v)           { return set (P::size, v); }
    PB& pos (double v)            { return set (P::position, v); }
    PB& dens (double v)           { return set (P::density, v); }
    PB& offs (double v)           { return set (P::offset, v); }
    PB& pre (double ms)           { return set (P::preDelay, ms); }
    PB& preSync (int divIndex)    { set (P::preDelaySync, 1); return set (P::preDelayDiv, divIndex); }
    PB& temper (double v)         { return set (P::temper, v); }
    PB& width (double v)          { return set (P::noteWidth, v); }
    PB& early()                   { return set (P::target, 0); }
    PB& late()                    { return set (P::target, 2); }
    PB& range (double lo, double hi) { set (P::rangeLow, lo); return set (P::rangeHigh, hi); }
    PB& cut (double lo, double hi){ set (P::lowCut, lo); return set (P::highCut, hi); }
    PB& steep()                   { set (P::lowCutSlope, 1); return set (P::highCutSlope, 1); }
    PB& out (double db)           { return set (P::output, db); }
    PB& killDry()                 { return set (P::killDry, 1); }
    PB& dual()                    { return set (P::dualMono, 1); }
    PB& notes (uint16_t m)        { set (P::notes, m); return set (P::noteMode, 0); }
    PB& midi (bool octave, double releaseMs)
    {
        set (P::noteMode, 2); set (P::midiOctave, octave ? 1 : 0); return set (P::midiRelease, releaseMs);
    }
    PB& seq (std::initializer_list<Harmony> rows, std::initializer_list<std::array<double, 3>> regions, int bars, int grid = 2)
    {
        SequenceData d = SequenceData::makeDefault();
        int i = 0;
        for (auto& h : rows) if (i < kSeqRows) d.rows[i++] = h;
        d.numRegions = 0;
        for (auto& r : regions)
            if (d.numRegions < kSeqMaxRegions)
                d.regions[d.numRegions++] = SeqRegion { (int) r[0], r[1], r[2] };
        d.selectedRow = d.numRegions > 0 ? d.regions[0].row : 0;
        auto old = t.getChildWithName (ids::sequence);
        if (old.isValid()) t.removeChild (old, nullptr);
        t.appendChild (SequenceModel::toTree (d), nullptr);
        set (P::seqBars, bars);
        set (P::seqGrid, grid);
        return set (P::noteMode, 1);
    }
    operator juce::ValueTree() const { return t; }
};

// sequência por defeito (I – vi – IV – V em C)
PB& defaultSeq (PB& p, int transposeRoot = 0)
{
    const int r = transposeRoot;
    return p.seq ({ H (r, Category::Chord, "maj"),      H (r + 2, Category::Chord, "min"), H (r + 4, Category::Chord, "min"),
                    H (r + 5, Category::Chord, "maj"),  H (r + 7, Category::Chord, "maj"), H (r + 9, Category::Chord, "min"),
                    H (r + 11, Category::Chord, "dim"), H (r, Category::Chord, "maj7") },
                  { { 0, 0, 4 }, { 5, 4, 4 }, { 3, 8, 4 }, { 4, 12, 4 } }, 4);
}
} // namespace

juce::Array<juce::ValueTree> makeFactoryPresets()
{
    juce::Array<juce::ValueTree> a;
    auto add = [&a] (const PB& p) { a.add (p.t); };

    // =================================================================== Tutorial
    add (PB ("01 Normal Reverb", "Tutorial", "Tempero a 0: o ROOM-X é uma reverb de estúdio normal. Experimente Decay, Size e Position.")
             .sp ("Aurora Hall").mix (30).dec (1.0).size (100));
    add (PB ("02 Positive Tempero - C Major", "Tutorial", "Tempero positivo: só as notas C, E e G ressoam na sala. Suba o Tempero devagar e oiça a sala afinar.")
             .sp ("Aurora Hall").mix (35).temper (70).notes (chord (C, "maj")));
    add (PB ("03 Negative Tempero - Harmonic Carve", "Tutorial", "Tempero negativo: as notas escolhidas são retiradas da sala — abre espaço para a voz ou o baixo nessas notas.")
             .sp ("Grand Arc").mix (35).temper (-85).width (70).notes (chord (C, "maj")));
    add (PB ("04 Narrow Note Width", "Tutorial", "Note Width baixo: ressonâncias puras como sinos, com cauda longa e afinada.")
             .sp ("Celestial Stage").mix (35).temper (85).width (6).notes (chord (A, "min")));
    add (PB ("05 Wide Note Width", "Tutorial", "Note Width alto: as notas alargam-se e a sala fica mais rica e coral.")
             .sp ("Celestial Stage").mix (35).temper (85).width (92).notes (chord (A, "min")));
    add (PB ("06 Early Only", "Tutorial", "Target EARLY (vermelho): só as primeiras reflexões são afinadas; a cauda fica natural.")
             .sp ("Concrete Loft").mix (40).temper (80).width (35).early().pos (-45).notes (chord (D, "m7")));
    add (PB ("07 Late Only", "Tutorial", "Target LATE (azul): o ataque fica natural e só a cauda floresce nas notas escolhidas.")
             .sp ("Grand Arc").mix (35).temper (80).width (40).late().pos (40).notes (chord (D, "m7")));
    add (PB ("08 Manual Scale - D Dorian", "Tutorial", "Choose Scale: escolha uma escala inteira. Aqui, D dórico — toda a sala soa dentro do modo.")
             .sp ("Opera Void").mix (32).temper (60).notes (mode (D, "Dorian")));
    {
        PB p ("09 Sequenced Chords", "Tutorial", "Sequence: a sala muda de acorde com o DAW (I–vi–IV–V). Carregue Play no DAW com o botão Arm ligado.");
        p.sp ("Aurora Hall").mix (35).temper (75).width (45);
        add (defaultSeq (p));
    }
    add (PB ("10 Live MIDI Space", "Tutorial", "MIDI: envie notas MIDI ao ROOM-X e a sala segue o que toca. Use o pedal (CC64) para segurar acordes.")
             .sp ("Neon Cave").mix (40).temper (90).width (30).midi (true, 300));
    add (PB ("11 Density + Offset Design", "Tutorial", "Density baixa + Offset: modos esparsos e deslocados — texturas metálicas e cintilantes.")
             .sp ("Harmonic Dust").mix (45).dens (12).offs (-220).temper (50).width (25).notes (chord (E, "min")));

    // =================================================================== Vocals
    add (PB ("Velvet Vocal", "Vocals").sp ("Velvet Room").mix (22).dec (1.1).pre (18).cut (180, 9000));
    add (PB ("R&B Silk Plate", "Vocals").sp ("Dark Plate").mix (20).dec (0.9).pre (35).cut (220, 11000));
    add (PB ("Trap Vox Plate", "Vocals").sp ("Silver Plate").mix (18).dec (0.7).preSync (4).cut (300, 13000));
    add (PB ("Gospel Hall Lift", "Vocals").sp ("Aurora Hall").mix (28).dec (1.3).pre (40).temper (35).width (55).notes (scale (Ab, "Major")));
    add (PB ("Kizomba Lead Air", "Vocals").sp ("Copper Plate").mix (22).dec (1.0).pre (25).temper (25).notes (chord (A, "m7")));
    add (PB ("Whisper Booth", "Vocals").sp ("Wide Booth").mix (25).dec (0.8).pos (-30).cut (250, 15000));
    add (PB ("Ballad Tail in Key", "Vocals").sp ("Grand Arc").mix (26).dec (1.2).pre (60).late().temper (55).width (50).notes (scale (F, "Maj Penta")));
    add (PB ("Carved Vocal Space", "Vocals").sp ("Opera Void").mix (30).dec (1.0).pre (30).temper (-55).width (60).notes (chord (G, "maj")));

    // =================================================================== Drums
    add (PB ("Afro House Shaker Room", "Drums").sp ("Wood Room").mix (20).dec (0.9).size (90).cut (400, 14000));
    add (PB ("Amapiano Log Room", "Drums").sp ("Tight Chamber").mix (25).dec (1.2).temper (40).width (30).notes (chord (F, "min")).range (36, 72));
    add (PB ("Snare Plate 80s", "Drums").sp ("Silver Plate").mix (25).dec (1.1).pre (12).cut (250, 12000));
    add (PB ("Gated Broken Snare", "Drums").sp ("Broken Space").mix (30).dec (1.0).size (110));
    add (PB ("Concrete Kit", "Drums").sp ("Concrete Loft").mix (22).dec (0.8).pos (-35).cut (120, 16000));
    add (PB ("Tuned Toms Room", "Drums").sp ("Studio A").mix (25).dec (1.4).temper (60).width (25).notes (chord (D, "5")).range (28, 70));
    add (PB ("House Clap Hall", "Drums").sp ("Nordic Hall").mix (20).dec (0.6).preSync (7).cut (350, 15000));
    add (PB ("Trap Hat Air", "Drums").sp ("Air Capsule").mix (25).dec (1.0).cut (2500, 20000));

    // =================================================================== Instruments
    add (PB ("Grand Piano Hall", "Instruments").sp ("Aurora Hall").mix (24).dec (1.0).pre (20));
    add (PB ("Rhodes Velvet", "Instruments").sp ("Velvet Room").mix (28).dec (1.3).temper (30).width (60).notes (chord (Eb, "maj9")));
    add (PB ("Semba Guitar Room", "Instruments").sp ("Wood Room").mix (22).dec (1.1).pos (-15));
    add (PB ("Acoustic Strum Space", "Instruments").sp ("Wide Booth").mix (20).dec (1.4).pre (10));
    add (PB ("Strings Deep Stage", "Instruments").sp ("Deep Auditorium").mix (30).dec (0.9).pre (45));
    add (PB ("Brass Opera", "Instruments").sp ("Opera Void").mix (24).dec (1.0).pre (30));
    add (PB ("Spring Guitar Twang", "Instruments").sp ("Spring Matrix").mix (28).dec (0.9).cut (180, 7000));
    add (PB ("Kalimba Bells", "Instruments").sp ("Glass Tunnel").mix (30).dec (1.2).temper (70).width (12).notes (scale (C, "Maj Penta")));

    // =================================================================== Synths
    add (PB ("Pad Halo C Minor", "Synths").sp ("Celestial Stage").mix (40).dec (1.4).temper (65).width (45).notes (scale (C, "Nat Minor")));
    add (PB ("Pluck Shimmer Tail", "Synths").sp ("Nordic Hall").mix (35).dec (1.3).late().temper (70).width (20).notes (chord (G, "maj7")));
    add (PB ("Lead Wide Plate", "Synths").sp ("Silver Plate").mix (25).dec (1.0).preSync (7).dual());
    add (PB ("Arp Orbit", "Synths").sp ("Orbital Choir").mix (30).dec (0.6).preSync (4));
    add (PB ("Supersaw Stadium", "Synths").sp ("Grand Arc").mix (25).dec (0.8).cut (300, 12000));
    add (PB ("Bass-Safe Synth Room", "Synths").sp ("Studio A").mix (25).dec (1.2).cut (250, 16000).temper (-40).notes (chord (A, "min")).range (15, 55));
    add (PB ("Lo-Fi Keys Room", "Synths").sp ("Vintage Chamber").mix (30).dens (35).cut (300, 6000));

    // =================================================================== Mix FX
    add (PB ("Send Hall 2.5s", "Mix FX").sp ("Aurora Hall").mix (100).killDry().dec (1.05).pre (25).cut (200, 12000));
    add (PB ("Send Plate 1.8s", "Mix FX").sp ("Copper Plate").mix (100).killDry().dec (1.05).pre (15).cut (220, 11000));
    add (PB ("Send Room Glue", "Mix FX").sp ("Studio A").mix (100).killDry().dec (1.0).cut (150, 14000));
    add (PB ("In-Key Bus Glue", "Mix FX").sp ("Opera Void").mix (12).dec (0.8).temper (25).width (65).notes (scale (A, "Nat Minor")));
    add (PB ("Throw Delay Space", "Mix FX").sp ("Frozen Corridor").mix (35).dec (1.0).preSync (10));
    add (PB ("Ducked Afro Tail", "Mix FX").sp ("Neon Cave").mix (20).dec (0.6).cut (350, 9000).temper (-30).notes (chord (F, "min")));
    add (PB ("Mono Dual Space", "Mix FX").sp ("Nordic Hall").mix (22).dual().dec (0.8));

    // =================================================================== Sound Design
    add (PB ("Infinite Bloom Pad", "Sound Design").sp ("Infinite Bloom").mix (50).dec (1.0).temper (55).width (40).notes (chord (D, "sus2")));
    add (PB ("Zero Gravity Drone", "Sound Design").sp ("Zero Gravity").mix (60).dec (1.2).temper (75).width (25).notes (chord (C, "5")));
    add (PB ("Prism Swell", "Sound Design").sp ("Prism Tail").mix (45).dec (1.0).offs (35));
    add (PB ("Harmonic Dust Chimes", "Sound Design").sp ("Harmonic Dust").mix (50).dens (8).temper (80).width (6).notes (scale (E, "Hirajoshi")));
    add (PB ("Metal Container", "Sound Design").sp ("Hollow Steel").mix (40).dens (25).offs (-140));
    add (PB ("Liquid Detune", "Sound Design").sp ("Liquid Room").mix (40).offs (12).dual());
    add (PB ("Transition Riser Space", "Sound Design").sp ("Infinite Bloom").mix (55).dec (0.8).pre (120).cut (500, 18000));

    // =================================================================== Modal Chords
    add (PB ("Major 7 Hall", "Modal Chords").sp ("Aurora Hall").mix (35).temper (75).width (40).notes (chord (F, "maj7")));
    add (PB ("Minor 9 Cave", "Modal Chords").sp ("Neon Cave").mix (35).temper (75).width (40).notes (chord (D, "m9")));
    add (PB ("Sus Fourths Plate", "Modal Chords").sp ("Silver Plate").mix (30).temper (70).width (35).notes (chord (A, "7sus4")));
    add (PB ("Hijaz Corridor", "Modal Chords").sp ("Frozen Corridor").mix (35).temper (75).width (30).notes (scale (E, "Hijaz")));
    add (PB ("Whole Tone Dream", "Modal Chords").sp ("Celestial Stage").mix (40).temper (80).width (30).notes (scale (C, "Whole Tone")));
    add (PB ("Power Fifth Drone", "Modal Chords").sp ("Grand Arc").mix (40).temper (90).width (15).notes (chord (E, "5")));

    // =================================================================== Sequenced Spaces
    {
        PB p ("Pop Progression Hall", "Sequenced Spaces"); p.sp ("Aurora Hall").mix (35).temper (70).width (40);
        add (defaultSeq (p));
    }
    {
        PB p ("Afro House Minor Loop", "Sequenced Spaces"); p.sp ("Neon Cave").mix (30).temper (65).width (35);
        add (p.seq ({ H (A, Category::Chord, "m7"), H (D, Category::Chord, "m7"), H (F, Category::Chord, "maj7"), H (E, Category::Chord, "m7"),
                      H (A, Category::Scale, "Nat Minor"), H (G, Category::Chord, "maj"), H (C, Category::Chord, "maj"), H (E, Category::Chord, "7") },
                    { { 0, 0, 4 }, { 1, 4, 4 }, { 2, 8, 4 }, { 3, 12, 4 } }, 4));
    }
    {
        PB p ("Trap Dark Cycle", "Sequenced Spaces"); p.sp ("Deep Auditorium").mix (28).temper (60).width (30).late();
        add (p.seq ({ H (C, Category::Chord, "min"), H (Ab, Category::Chord, "maj"), H (Eb, Category::Chord, "maj"), H (G, Category::Chord, "min"),
                      H (C, Category::Scale, "Harm Minor"), H (F, Category::Chord, "min"), H (Bb, Category::Chord, "maj"), H (G, Category::Chord, "7") },
                    { { 0, 0, 8 }, { 1, 8, 4 }, { 3, 12, 4 }, { 0, 16, 8 }, { 5, 24, 4 }, { 7, 28, 4 } }, 8));
    }
    {
        PB p ("R&B Neo Soul Turn", "Sequenced Spaces"); p.sp ("Velvet Room").mix (30).temper (60).width (45);
        add (p.seq ({ H (D, Category::Chord, "m9"), H (G, Category::Chord, "13"), H (C, Category::Chord, "maj9"), H (A, Category::Chord, "7#9"),
                      H (F, Category::Chord, "maj7"), H (E, Category::Chord, "m7b5"), H (A, Category::Chord, "7"), H (D, Category::Chord, "m7") },
                    { { 0, 0, 4 }, { 1, 4, 4 }, { 2, 8, 4 }, { 3, 12, 4 } }, 4));
    }
    {
        PB p ("Half-Bar Chord Pulse", "Sequenced Spaces"); p.sp ("Copper Plate").mix (35).temper (75).width (25);
        add (p.seq ({ H (E, Category::Chord, "min"), H (C, Category::Chord, "maj"), H (G, Category::Chord, "maj"), H (D, Category::Chord, "maj"),
                      H (A, Category::Chord, "min"), H (B, Category::Chord, "min"), H (E, Category::Chord, "sus4"), H (E, Category::Chord, "min") },
                    { { 0, 0, 2 }, { 1, 2, 2 }, { 2, 4, 2 }, { 3, 6, 2 }, { 0, 8, 2 }, { 1, 10, 2 }, { 4, 12, 2 }, { 6, 14, 2 } }, 4, 1));
    }

    // =================================================================== MIDI Performance
    add (PB ("MIDI Chord Follower", "MIDI Performance").sp ("Aurora Hall").mix (35).temper (75).width (40).midi (true, 250));
    add (PB ("MIDI Bell Keys", "MIDI Performance").sp ("Glass Tunnel").mix (45).temper (90).width (5).midi (false, 600));
    add (PB ("MIDI Pedal Drone", "MIDI Performance").sp ("Zero Gravity").mix (55).temper (85).width (20).midi (true, 1500));
    add (PB ("MIDI Carve (Negative)", "MIDI Performance").sp ("Grand Arc").mix (35).temper (-80).width (55).midi (true, 120));

    return a;
}
} // namespace roomx::state
