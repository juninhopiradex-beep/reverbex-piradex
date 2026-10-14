#include "MusicTheory.h"
#include <initializer_list>

namespace roomx::music
{
namespace
{
constexpr uint16_t M (std::initializer_list<int> iv)
{
    uint16_t m = 0;
    for (int i : iv) m = (uint16_t) (m | (1u << (i % 12)));
    return m;
}

const HarmonyType kIntervals[] =
{
    { "Unison",   "Unison (root only)", M ({ 0 }) },
    { "m2",       "Minor 2nd",          M ({ 0, 1 }) },
    { "M2",       "Major 2nd",          M ({ 0, 2 }) },
    { "m3",       "Minor 3rd",          M ({ 0, 3 }) },
    { "M3",       "Major 3rd",          M ({ 0, 4 }) },
    { "P4",       "Perfect 4th",        M ({ 0, 5 }) },
    { "Tritone",  "Tritone",            M ({ 0, 6 }) },
    { "P5",       "Perfect 5th",        M ({ 0, 7 }) },
    { "m6",       "Minor 6th",          M ({ 0, 8 }) },
    { "M6",       "Major 6th",          M ({ 0, 9 }) },
    { "m7",       "Minor 7th",          M ({ 0, 10 }) },
    { "M7",       "Major 7th",          M ({ 0, 11 }) },
};

const HarmonyType kChords[] =
{
    { "maj",    "Major triad",        M ({ 0, 4, 7 }) },
    { "min",    "Minor triad",        M ({ 0, 3, 7 }) },
    { "dim",    "Diminished triad",   M ({ 0, 3, 6 }) },
    { "aug",    "Augmented triad",    M ({ 0, 4, 8 }) },
    { "sus2",   "Suspended 2nd",      M ({ 0, 2, 7 }) },
    { "sus4",   "Suspended 4th",      M ({ 0, 5, 7 }) },
    { "5",      "Power (root + 5th)", M ({ 0, 7 }) },
    { "6",      "Major 6th",          M ({ 0, 4, 7, 9 }) },
    { "m6",     "Minor 6th",          M ({ 0, 3, 7, 9 }) },
    { "maj7",   "Major 7th",          M ({ 0, 4, 7, 11 }) },
    { "m7",     "Minor 7th",          M ({ 0, 3, 7, 10 }) },
    { "7",      "Dominant 7th",       M ({ 0, 4, 7, 10 }) },
    { "dim7",   "Diminished 7th",     M ({ 0, 3, 6, 9 }) },
    { "m7b5",   "Half-diminished",    M ({ 0, 3, 6, 10 }) },
    { "mMaj7",  "Minor-major 7th",    M ({ 0, 3, 7, 11 }) },
    { "7sus4",  "Dominant 7 sus4",    M ({ 0, 5, 7, 10 }) },
    { "add9",   "Major add 9",        M ({ 0, 2, 4, 7 }) },
    { "madd9",  "Minor add 9",        M ({ 0, 2, 3, 7 }) },
    { "maj9",   "Major 9th",          M ({ 0, 2, 4, 7, 11 }) },
    { "m9",     "Minor 9th",          M ({ 0, 2, 3, 7, 10 }) },
    { "9",      "Dominant 9th",       M ({ 0, 2, 4, 7, 10 }) },
    { "m11",    "Minor 11th",         M ({ 0, 2, 3, 5, 7, 10 }) },
    { "13",     "Dominant 13th",      M ({ 0, 2, 4, 7, 9, 10 }) },
    { "7#9",    "Dominant 7 #9",      M ({ 0, 3, 4, 7, 10 }) },
    { "quartal","Stacked 4ths",       M ({ 0, 5, 10 }) },
};

const HarmonyType kScales[] =
{
    { "Major",        "Major",                 M ({ 0, 2, 4, 5, 7, 9, 11 }) },
    { "Nat Minor",    "Natural minor",         M ({ 0, 2, 3, 5, 7, 8, 10 }) },
    { "Harm Minor",   "Harmonic minor",        M ({ 0, 2, 3, 5, 7, 8, 11 }) },
    { "Mel Minor",    "Melodic minor",         M ({ 0, 2, 3, 5, 7, 9, 11 }) },
    { "Maj Penta",    "Major pentatonic",      M ({ 0, 2, 4, 7, 9 }) },
    { "Min Penta",    "Minor pentatonic",      M ({ 0, 3, 5, 7, 10 }) },
    { "Blues",        "Minor blues",           M ({ 0, 3, 5, 6, 7, 10 }) },
    { "Maj Blues",    "Major blues",           M ({ 0, 2, 3, 4, 7, 9 }) },
    { "Whole Tone",   "Whole tone",            M ({ 0, 2, 4, 6, 8, 10 }) },
    { "Chromatic",    "Chromatic (all 12)",    kAllPitchClasses },
    { "Dim HW",       "Diminished half-whole", M ({ 0, 1, 3, 4, 6, 7, 9, 10 }) },
    { "Dim WH",       "Diminished whole-half", M ({ 0, 2, 3, 5, 6, 8, 9, 11 }) },
    { "Augmented",    "Augmented (hexatonic)", M ({ 0, 3, 4, 7, 8, 11 }) },
    { "Bebop Dom",    "Bebop dominant",        M ({ 0, 2, 4, 5, 7, 9, 10, 11 }) },
    { "Hung Minor",   "Hungarian minor",       M ({ 0, 2, 3, 6, 7, 8, 11 }) },
    { "Dbl Harm",     "Double harmonic",       M ({ 0, 1, 4, 5, 7, 8, 11 }) },
    { "Hijaz",        "Hijaz / Phrygian dominant", M ({ 0, 1, 4, 5, 7, 8, 10 }) },
    { "Hirajoshi",    "Hirajoshi",             M ({ 0, 2, 3, 7, 8 }) },
    { "In Sen",       "In Sen",                M ({ 0, 1, 5, 7, 10 }) },
    { "Egyptian",     "Egyptian (suspended pentatonic)", M ({ 0, 2, 5, 7, 10 }) },
    { "Prometheus",   "Prometheus",            M ({ 0, 2, 4, 6, 9, 10 }) },
    { "Neap Minor",   "Neapolitan minor",      M ({ 0, 1, 3, 5, 7, 8, 11 }) },
    { "Persian",      "Persian",               M ({ 0, 1, 4, 5, 6, 8, 11 }) },
    { "Enigmatic",    "Enigmatic",             M ({ 0, 1, 4, 6, 8, 10, 11 }) },
};

const HarmonyType kModes[] =
{
    { "Ionian",       "Ionian (I)",            M ({ 0, 2, 4, 5, 7, 9, 11 }) },
    { "Dorian",       "Dorian (II)",           M ({ 0, 2, 3, 5, 7, 9, 10 }) },
    { "Phrygian",     "Phrygian (III)",        M ({ 0, 1, 3, 5, 7, 8, 10 }) },
    { "Lydian",       "Lydian (IV)",           M ({ 0, 2, 4, 6, 7, 9, 11 }) },
    { "Mixolydian",   "Mixolydian (V)",        M ({ 0, 2, 4, 5, 7, 9, 10 }) },
    { "Aeolian",      "Aeolian (VI)",          M ({ 0, 2, 3, 5, 7, 8, 10 }) },
    { "Locrian",      "Locrian (VII)",         M ({ 0, 1, 3, 5, 6, 8, 10 }) },
    { "Lydian Dom",   "Lydian dominant",       M ({ 0, 2, 4, 6, 7, 9, 10 }) },
    { "Lydian Aug",   "Lydian augmented",      M ({ 0, 2, 4, 6, 8, 9, 11 }) },
    { "Mixo b6",      "Mixolydian b6",         M ({ 0, 2, 4, 5, 7, 8, 10 }) },
    { "Dorian b2",    "Dorian b2",             M ({ 0, 1, 3, 5, 7, 9, 10 }) },
    { "Locrian #2",   "Locrian #2",            M ({ 0, 2, 3, 5, 6, 8, 10 }) },
    { "Altered",      "Altered (super-locrian)", M ({ 0, 1, 3, 4, 6, 8, 10 }) },
};

template <size_t N> constexpr int count (const HarmonyType (&)[N]) { return (int) N; }
} // namespace

int numTypes (Category c) noexcept
{
    switch (c)
    {
        case Category::Interval: return count (kIntervals);
        case Category::Chord:    return count (kChords);
        case Category::Scale:    return count (kScales);
        case Category::Mode:     return count (kModes);
    }
    return 0;
}

const HarmonyType& type (Category c, int index) noexcept
{
    const int n = numTypes (c);
    if (index < 0) index = 0;
    if (index >= n) index = n - 1;
    switch (c)
    {
        case Category::Interval: return kIntervals[index];
        case Category::Chord:    return kChords[index];
        case Category::Scale:    return kScales[index];
        case Category::Mode:     return kModes[index];
    }
    return kChords[0];
}

const char* categoryName (Category c) noexcept
{
    switch (c)
    {
        case Category::Interval: return "Interval";
        case Category::Chord:    return "Chord";
        case Category::Scale:    return "Scale";
        case Category::Mode:     return "Mode";
    }
    return "";
}

bool findType (Category c, const std::string& shortName, int& indexOut) noexcept
{
    for (int i = 0; i < numTypes (c); ++i)
        if (shortName == type (c, i).shortName) { indexOut = i; return true; }
    return false;
}

const char* pitchName (int pc, bool useFlats)
{
    static const char* sharps[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* flats[]  = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    pc = ((pc % 12) + 12) % 12;
    return useFlats ? flats[pc] : sharps[pc];
}

std::string noteName (int midiNote, bool useFlats)
{
    const int octave = midiNote / 12 - 1;
    return std::string (pitchName (midiNote % 12, useFlats)) + std::to_string (octave);
}

std::string Harmony::label (bool useFlats) const
{
    return std::string (pitchName (root, useFlats)) + " " + type (category, typeIndex).shortName;
}

} // namespace roomx::music
