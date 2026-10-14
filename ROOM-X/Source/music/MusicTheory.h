// =============================================================================
//  ROOM-X by Piradex — teoria musical (sem dependências de JUCE)
//
//  Notas: índice MIDI (C4 = 60, A4 = 69). O banco modal cobre D#0 (15) a
//  D#10 (135) — 121 semitons, ~19.4 Hz a ~19.9 kHz com A4 = 440 Hz.
//  Máscaras de classes de altura: 12 bits, bit 0 = C ... bit 11 = B.
// =============================================================================
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <string>

namespace roomx::music
{
constexpr int kNoteMin   = 15;    // D#0
constexpr int kNoteMax   = 135;   // D#10
constexpr int kNumNotes  = kNoteMax - kNoteMin + 1;   // 121
constexpr uint16_t kAllPitchClasses = 0x0FFF;

// ---------------------------------------------------------------------------
//  Máscara de notas absolutas (0..135) — 3 palavras de 64 bits, POD.
// ---------------------------------------------------------------------------
struct NoteSet
{
    uint64_t w[3] { 0, 0, 0 };

    void clear() noexcept                 { w[0] = w[1] = w[2] = 0; }
    void set (int n) noexcept             { if (n >= 0 && n < 192) w[n >> 6] |= (uint64_t (1) << (n & 63)); }
    bool test (int n) const noexcept      { return n >= 0 && n < 192 && ((w[n >> 6] >> (n & 63)) & 1u) != 0; }
    bool empty() const noexcept           { return (w[0] | w[1] | w[2]) == 0; }
    bool operator== (const NoteSet& o) const noexcept { return w[0] == o.w[0] && w[1] == o.w[1] && w[2] == o.w[2]; }
    bool operator!= (const NoteSet& o) const noexcept { return ! (*this == o); }

    // Classe de altura repetida em todas as oitavas do banco
    static NoteSet fromPitchClasses (uint16_t mask) noexcept
    {
        NoteSet s;
        for (int n = kNoteMin; n <= kNoteMax; ++n)
            if ((mask >> (n % 12)) & 1u) s.set (n);
        return s;
    }

    uint16_t pitchClasses() const noexcept
    {
        uint16_t m = 0;
        for (int n = 0; n <= kNoteMax; ++n)
            if (test (n)) m |= (uint16_t) (1u << (n % 12));
        return m;
    }
};

// ---------------------------------------------------------------------------
inline uint16_t rotatePitchMask (uint16_t mask, int semitones) noexcept
{
    int s = ((semitones % 12) + 12) % 12;
    mask &= kAllPitchClasses;
    return (uint16_t) (((mask << s) | (mask >> (12 - s))) & kAllPitchClasses);
}

inline int popCount12 (uint16_t m) noexcept
{
    int c = 0;
    for (int i = 0; i < 12; ++i) c += (m >> i) & 1;
    return c;
}

// Afinação global (por instância)
struct Tuning
{
    double referenceHz = 440.0;   // 400..500
    double fineCents   = 0.0;     // -50..+50

    double noteToHz (double note) const noexcept
    {
        return referenceHz * std::pow (2.0, (note - 69.0 + fineCents / 100.0) / 12.0);
    }
    double hzToNote (double hz) const noexcept
    {
        return 69.0 + 12.0 * std::log2 (hz / referenceHz) - fineCents / 100.0;
    }
};

const char* pitchName (int pitchClass, bool useFlats);
std::string noteName (int midiNote, bool useFlats);          // ex.: "D#0", "A4"

// ---------------------------------------------------------------------------
//  Biblioteca de intervalos / acordes / escalas / modos
// ---------------------------------------------------------------------------
enum class Category : int { Interval = 0, Chord = 1, Scale = 2, Mode = 3 };
constexpr int kNumCategories = 4;

struct HarmonyType
{
    const char* shortName;   // usado na etiqueta ("C maj", "F Dorian")
    const char* longName;    // usado no navegador
    uint16_t    mask;        // relativo à tónica C
};

int                 numTypes (Category c) noexcept;
const HarmonyType&  type (Category c, int index) noexcept;
const char*         categoryName (Category c) noexcept;

// Uma "harmonia" concreta: tónica + tipo
struct Harmony
{
    int root = 0;                      // 0..11
    Category category = Category::Chord;
    int typeIndex = 0;

    uint16_t mask() const noexcept     { return rotatePitchMask (type (category, typeIndex).mask, root); }
    std::string label (bool useFlats) const;
    bool isScaleLike() const noexcept  { return category == Category::Scale || category == Category::Mode; }
};

// Procura um tipo pelo nome curto (para presets). Devolve false se não existir.
bool findType (Category c, const std::string& shortName, int& indexOut) noexcept;

} // namespace roomx::music
