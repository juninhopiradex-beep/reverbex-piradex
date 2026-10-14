// =============================================================================
//  ROOM-X — estado das notas MIDI (todas as entradas/canais, pedal CC64)
// =============================================================================
#pragma once

#include "MusicTheory.h"
#include <cstring>

namespace roomx::music
{
class MidiNoteState
{
public:
    void reset() noexcept
    {
        std::memset (held, 0, sizeof (held));
        std::memset (sustained, 0, sizeof (sustained));
        pedal = false;
    }

    void noteOn (int note) noexcept
    {
        if (note < 0 || note > 127) return;
        held[note] = true;
        if (pedal) sustained[note] = true;
    }

    void noteOff (int note) noexcept
    {
        if (note < 0 || note > 127) return;
        held[note] = false;
    }

    void sustain (bool down) noexcept
    {
        pedal = down;
        if (down)
        {
            for (int i = 0; i < 128; ++i) sustained[i] = sustained[i] || held[i];
        }
        else
        {
            std::memset (sustained, 0, sizeof (sustained));
        }
    }

    void allNotesOff() noexcept { reset(); }

    bool isActive (int note) const noexcept { return held[note] || sustained[note]; }

    // octaveRepeat: duplica cada nota em todas as oitavas ACIMA dela (não abaixo)
    NoteSet notes (bool octaveRepeat) const noexcept
    {
        NoteSet s;
        for (int n = 0; n < 128; ++n)
        {
            if (! isActive (n)) continue;
            if (octaveRepeat)
                for (int k = n; k <= kNoteMax; k += 12) s.set (k);
            else
                s.set (n);
        }
        return s;
    }

private:
    bool held[128] {};
    bool sustained[128] {};
    bool pedal = false;
};
} // namespace roomx::music
