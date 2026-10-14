// =============================================================================
//  ROOM-X — testes numéricos do núcleo DSP e da camada musical (sem JUCE)
//  g++ -O2 -std=c++17 -ISource tests/test_dsp.cpp Source/dsp/*.cpp Source/music/*.cpp
// =============================================================================
#include "dsp/ReverbEngine.h"
#include "music/SequenceEngine.h"
#include "music/MidiNoteState.h"

#include <chrono>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

using namespace roomx;
using namespace roomx::dsp;
using namespace roomx::music;

static int failures = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++failures; std::printf ("  FALHOU: " __VA_ARGS__); std::printf ("   (%s:%d)\n", __FILE__, __LINE__); } } while (0)

static double goertzelDb (const std::vector<float>& x, double f, double fs, size_t start = 0)
{
    const double w = 2.0 * kPi * f / fs, c = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0;
    for (size_t i = start; i < x.size(); ++i)
    {
        // janela de Hann
        const double h = 0.5 - 0.5 * std::cos (2.0 * kPi * (double) (i - start) / (double) (x.size() - start - 1));
        const double s = x[i] * h + c * s1 - s2;
        s2 = s1; s1 = s;
    }
    const double p = s1 * s1 + s2 * s2 - c * s1 * s2;
    return 10.0 * std::log10 (p + 1e-30);
}

static std::vector<float> noise (size_t n, unsigned seed)
{
    std::mt19937 g (seed);
    std::uniform_real_distribution<float> d (-0.5f, 0.5f);
    std::vector<float> v (n);
    for (auto& s : v) s = d (g);
    return v;
}

// ------------------------------------------------------------------ banco modal
static void testModalBank()
{
    std::printf ("[banco modal]\n");
    const double fs = 48000.0;
    const size_t N = 96000;

    auto run = [&] (float temper, uint16_t pcs, float lo = kNoteMin, float hi = kNoteMax)
    {
        ModalBank b; b.prepare (fs);
        TemperSettings s;
        s.temper = temper; s.noteWidth = 0.5f; s.notes = NoteSet::fromPitchClasses (pcs);
        s.rangeLow = lo; s.rangeHigh = hi;
        auto L = noise (N, 1), R = noise (N, 2);
        const auto inCopy = L;
        for (size_t i = 0; i < N; i += 64)
        {
            const int n = (int) std::min<size_t> (64, N - i);
            b.process (L.data() + i, R.data() + i, n, s);
        }
        return std::make_pair (L, inCopy);
    };

    // Temper = 0 -> reconstrução exacta
    {
        auto [y, x] = run (0.0f, 0x091);
        double maxDiff = 0;
        for (size_t i = 0; i < N; ++i) maxDiff = std::max (maxDiff, (double) std::fabs (y[i] - x[i]));
        CHECK (maxDiff == 0.0, "Temper 0 devia ser bit-exact (diff %g)\n", maxDiff);
    }

    const uint16_t aOnly = 1u << 9;   // A
    // Temper +100: A (440) muito acima de A# (466.16)
    {
        auto [y, x] = run (1.0f, aOnly);
        const double ga = goertzelDb (y, 440.0, fs, 24000), gb = goertzelDb (y, 466.16, fs, 24000);
        const double xa = goertzelDb (x, 440.0, fs, 24000), xb = goertzelDb (x, 466.16, fs, 24000);
        const double rel = (ga - gb) - (xa - xb);
        std::printf ("  Temper +100, nota A: A vs A# = %+.1f dB\n", rel);
        CHECK (rel > 10.0, "Temper positivo não destacou a nota (%.1f dB)\n", rel);
    }
    // Temper -100: A cortado em relação a A#
    {
        auto [y, x] = run (-1.0f, aOnly);
        const double ga = goertzelDb (y, 440.0, fs, 24000), gb = goertzelDb (y, 466.16, fs, 24000);
        const double xa = goertzelDb (x, 440.0, fs, 24000), xb = goertzelDb (x, 466.16, fs, 24000);
        const double rel = (ga - gb) - (xa - xb);
        std::printf ("  Temper -100, nota A: A vs A# = %+.1f dB\n", rel);
        CHECK (rel < -15.0, "Temper negativo não cortou a nota (%.1f dB)\n", rel);
    }
    // Temper Range: A4 fora do intervalo (C5..D#10) -> A4 intacto, A5 tratado
    {
        auto [y, x] = run (1.0f, aOnly, 72.0f, (float) kNoteMax);
        const double d392  = goertzelDb (y, 392.0, fs, 24000)  - goertzelDb (x, 392.0, fs, 24000);
        const double d1245 = goertzelDb (y, 1244.5, fs, 24000) - goertzelDb (x, 1244.5, fs, 24000);
        const double d880  = goertzelDb (y, 880.0, fs, 24000)  - goertzelDb (x, 880.0, fs, 24000);
        std::printf ("  Range C5+: G4 (fora) %+.1f dB, D#6 (dentro, não seleccionada) %+.1f dB, A5 (seleccionada) %+.1f dB\n", d392, d1245, d880);
        CHECK (std::fabs (d392) < 1.5, "fora do Range devia ficar intacto (%.1f)\n", d392);
        CHECK (d1245 < -15.0, "dentro do Range D#6 devia ser atenuado (%.1f)\n", d1245);
        CHECK (d880 > d1245 + 20.0, "A5 devia sobressair\n");
    }
}

// ------------------------------------------------------------------ espaços
static double measureRt60 (const std::vector<float>& ir, double fs)
{
    // integração de Schroeder, ajuste -5..-25 dB (T20 x 3)
    std::vector<double> e (ir.size());
    double acc = 0;
    for (size_t i = ir.size(); i-- > 0;) { acc += (double) ir[i] * ir[i]; e[i] = acc; }
    const double e0 = e[0];
    double t5 = -1, t25 = -1;
    for (size_t i = 0; i < e.size(); ++i)
    {
        const double db = 10.0 * std::log10 (e[i] / e0 + 1e-30);
        if (t5 < 0 && db <= -5.0)  t5 = (double) i / fs;
        if (t25 < 0 && db <= -25.0) { t25 = (double) i / fs; break; }
    }
    if (t5 < 0 || t25 < 0) return -1;
    return (t25 - t5) * 3.0;
}

static void testSpaces()
{
    std::printf ("[espaços]\n");
    const double fs = 48000.0;
    for (int si = 0; si < kNumSpaces; ++si)
    {
        ReverbEngine eng;
        eng.prepare (fs, 512);
        EngineParams p;
        p.spaceIndex = si;
        const float expected = ReverbEngine::effectiveRt60 (si, 1.0f, 1.0f);
        const size_t N = (size_t) (fs * std::min (30.0, std::max (3.0, expected * 2.2)));
        std::vector<float> L (N, 0.0f), R (N, 0.0f);
        L[0] = R[0] = 1.0f;
        std::vector<float> oL (N), oR (N);
        for (size_t i = 0; i < N; i += 512)
        {
            const int n = (int) std::min<size_t> (512, N - i);
            eng.process (L.data() + i, R.data() + i, oL.data() + i, oR.data() + i, n, p);
        }
        double peak = 0, energy = 0, tailEnergy = 0, corr = 0, eL = 0, eR = 0;
        bool finite = true;
        for (size_t i = 0; i < N; ++i)
        {
            finite = finite && std::isfinite (oL[i]) && std::isfinite (oR[i]);
            peak = std::max (peak, (double) std::max (std::fabs (oL[i]), std::fabs (oR[i])));
            energy += oL[i] * oL[i];
            if (i > N * 9 / 10) tailEnergy += oL[i] * oL[i];
            corr += oL[i] * oR[i]; eL += oL[i] * oL[i]; eR += oR[i] * oR[i];
        }
        const double rt = measureRt60 (oL, fs);
        const double ic = corr / std::sqrt (eL * eR + 1e-30);
        std::printf ("  %-16s RT60 alvo %6.2f s, medido %6.2f s, pico %.3f, energia %.3f, correl L/R %+.2f\n",
                     space (si).name, expected, rt, peak, energy, ic);
        CHECK (finite, "%s: NaN/Inf\n", space (si).name);
        CHECK (peak < 4.0, "%s: pico demasiado alto %.2f\n", space (si).name, peak);
        CHECK (energy > 0.005, "%s: sem energia\n", space (si).name);
        CHECK (tailEnergy < energy * 0.2, "%s: não decai\n", space (si).name);
        if (rt > 0 && expected < 10.0)
            CHECK (rt > expected * 0.35 && rt < expected * 2.2, "%s: RT60 %.2f longe de %.2f\n", space (si).name, rt, expected);
    }
}

// ------------------------------------------------------------------ stress
static void testStress()
{
    std::printf ("[stress: automação aleatória, 44.1/96/192 kHz, blocos 16..4096]\n");
    const double rates[] = { 44100.0, 96000.0, 192000.0 };
    const int blocks[] = { 16, 4096, 333 };
    std::mt19937 g (42);
    std::uniform_real_distribution<float> u (0.0f, 1.0f);
    for (int ri = 0; ri < 3; ++ri)
    {
        const double fs = rates[ri];
        ReverbEngine eng;
        eng.prepare (fs, 4096);
        EngineParams p;
        const size_t N = (size_t) (fs * 8.0);
        auto in = noise (N, 7);
        std::vector<float> L (N), R (N);
        double peak = 0;
        bool finite = true;
        for (size_t i = 0; i < N;)
        {
            const int n = (int) std::min<size_t> ((size_t) blocks[ri], N - i);
            if (u (g) < 0.2f)
            {
                p.spaceIndex = (int) (u (g) * kNumSpaces);
                p.decay = 0.01f + u (g) * u (g) * 30.0f;
                p.size = 0.5f + 1.5f * u (g);
                p.density = 0.01f + 0.99f * u (g);
                p.offsetHz = -500.0f + 1000.0f * u (g);
                p.position = -99.0f + 198.0f * u (g);
                p.preDelayMs = 2000.0f * u (g) * u (g);
                p.temper.temper = -1.0f + 2.0f * u (g);
                p.temper.noteWidth = 0.01f + 0.98f * u (g);
                p.temper.notes = NoteSet::fromPitchClasses ((uint16_t) (g() & 0xFFF));
                p.target = (Target) (g() % 3);
                p.dualMono = u (g) < 0.3f;
                p.eco = u (g) < 0.3f;
                p.lowCutHz = 20.0f + 500.0f * u (g);
                p.highCutHz = 2000.0f + 18000.0f * u (g);
                p.lowCut36 = (int) (g() & 1); p.highCut36 = (int) (g() & 1);
                p.temper.rangeLow = kNoteMin + 50.0f * u (g);
                p.temper.rangeHigh = p.temper.rangeLow + 10.0f + 60.0f * u (g);
            }
            eng.process (in.data() + i, in.data() + i, L.data() + i, R.data() + i, n, p);
            for (int k = 0; k < n; ++k)
            {
                finite = finite && std::isfinite (L[i + k]) && std::isfinite (R[i + k]);
                peak = std::max (peak, (double) std::fabs (L[i + k]));
            }
            i += (size_t) n;
        }
        std::printf ("  %.1f kHz: pico %.2f, finito %s\n", fs / 1000.0, peak, finite ? "sim" : "NÃO");
        CHECK (finite, "NaN/Inf a %.0f Hz\n", fs);
        CHECK (peak < 40.0, "pico excessivo %.1f\n", peak);
    }
}

// ------------------------------------------------------------------ CPU
static void testCpu()
{
    std::printf ("[CPU, 48 kHz, 10 s de áudio]\n");
    struct Case { const char* name; bool dual; bool eco; float temper; uint16_t pcs; float width; };
    const Case cases[] = {
        { "normal (Temper 0)",               false, false, 0.0f, 0x091, 0.5f },
        { "Temper 60, acorde maior",         false, false, 0.6f, 0x091, 0.5f },
        { "pior caso: 12 notas, All, Dual",  true,  false, 0.9f, 0xFFF, 0.99f },
        { "pior caso em Eco",                true,  true,  0.9f, 0xFFF, 0.99f },
    };
    for (const auto& c : cases)
    {
        const double fs = 48000.0;
        ReverbEngine eng; eng.prepare (fs, 256);
        EngineParams p;
        p.spaceIndex = 7; p.dualMono = c.dual; p.eco = c.eco;
        p.temper.temper = c.temper; p.temper.noteWidth = c.width;
        p.temper.notes = NoteSet::fromPitchClasses (c.pcs);
        const size_t N = (size_t) (fs * 10.0);
        auto in = noise (N, 3);
        std::vector<float> L (N), R (N);
        const auto t0 = std::chrono::steady_clock::now();
        for (size_t i = 0; i < N; i += 256)
            eng.process (in.data() + i, in.data() + i, L.data() + i, R.data() + i, (int) std::min<size_t> (256, N - i), p);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  %-34s %5.1f %% de um núcleo\n", c.name, 100.0 * secs / 10.0);
    }
}

// ------------------------------------------------------------------ música
static void testMusic()
{
    std::printf ("[teoria musical / sequenciador / MIDI]\n");
    int idx = 0;
    CHECK (findType (Category::Chord, "maj", idx) && idx == 0, "acorde maj\n");
    Harmony h { 7, Category::Chord, 0 };   // G maj
    CHECK (h.mask() == ((1u << 7) | (1u << 11) | (1u << 2)), "G maj = G B D\n");
    CHECK (h.label (false) == "G maj", "etiqueta\n");
    CHECK (rotatePitchMask (0x001, 13) == 0x002, "transposição com wrap\n");
    CHECK (rotatePitchMask (0x800, 1) == 0x001, "B + 1 = C\n");
    for (int c = 0; c < kNumCategories; ++c)
        for (int t = 0; t < numTypes ((Category) c); ++t)
            CHECK ((type ((Category) c, t).mask & 1u) != 0, "todos os tipos contêm a tónica\n");

    Tuning tu;
    CHECK (std::fabs (tu.noteToHz (69) - 440.0) < 1e-9, "A4\n");
    tu.referenceHz = 432.0; tu.fineCents = 0;
    CHECK (std::fabs (tu.noteToHz (69) - 432.0) < 1e-9, "A4 = 432\n");
    tu.referenceHz = 440.0; tu.fineCents = 100.0;
    CHECK (std::fabs (tu.noteToHz (69) - 466.1637615) < 1e-3, "+100 cents\n");
    CHECK (noteName (15, false) == "D#0" && noteName (135, true) == "Eb10", "nomes D#0 / Eb10\n");

    auto ns = NoteSet::fromPitchClasses (0x001);
    CHECK (ns.test (24) && ns.test (132) && ! ns.test (25) && ! ns.test (12), "C em todas as oitavas do banco\n");
    CHECK (ns.pitchClasses() == 0x001, "pitchClasses\n");

    MidiNoteState m;
    m.noteOn (36); m.noteOn (64);
    auto a = m.notes (true);
    CHECK (a.test (36) && a.test (48) && a.test (64) && a.test (76) && ! a.test (24) && ! a.test (52), "Octave Repeat só para cima\n");
    auto b = m.notes (false);
    CHECK (b.test (36) && ! b.test (48), "sem Octave Repeat\n");
    m.sustain (true); m.noteOff (36);
    CHECK (m.notes (false).test (36), "pedal segura a nota\n");
    m.sustain (false);
    CHECK (! m.notes (false).test (36) && m.notes (false).test (64), "pedal solto\n");

    auto d = SequenceData::makeDefault();
    SequenceEngine e;
    SequenceTransport t; t.hostPlaying = true; t.bars = 4;
    t.ppq = 0.5;  CHECK (e.evaluate (d, t).activeRow == 0, "compasso 1 = I\n");
    t.ppq = 5.0;  CHECK (e.evaluate (d, t).activeRow == 5, "compasso 2 = vi\n");
    t.ppq = 17.0; CHECK (e.evaluate (d, t).activeRow == 0, "loop de 4 compassos\n");
    t.ppq = 13.0; t.transpose = 2;
    CHECK (e.evaluate (d, t).mask == rotatePitchMask (d.rows[4].mask(), 2), "transposição da sequência\n");
    t.transpose = 0; t.offsetBeats = 4.0; t.ppq = 4.5;
    CHECK (e.evaluate (d, t).activeRow == 0, "session offset\n");
    t.hostPlaying = false; d.selectedRow = 3;
    CHECK (e.evaluate (d, t).activeRow == 3, "parado = linha seleccionada\n");
}

int main()
{
    testMusic();
    testModalBank();
    testSpaces();
    testStress();
    testCpu();
    if (failures == 0) std::printf ("\nTODOS OS TESTES PASSARAM\n");
    else               std::printf ("\n%d FALHA(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
