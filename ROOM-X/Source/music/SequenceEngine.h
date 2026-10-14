// =============================================================================
//  ROOM-X — sequenciador harmónico (dados POD + motor de reprodução)
//
//  A thread de mensagens edita uma ValueTree (com undo) e compila-a para
//  SequenceData (POD). O motor de áudio recebe uma cópia sem locks
//  bloqueantes e decide, por bloco, qual linha (harmonia) está activa.
// =============================================================================
#pragma once

#include "MusicTheory.h"

namespace roomx::music
{
constexpr int kSeqRows       = 8;
constexpr int kSeqMaxRegions = 256;
constexpr int kBeatsPerBar   = 4;      // grelha 4/4
constexpr int kSeqMaxBars    = 64;

struct SeqRegion
{
    int    row    = 0;
    double start  = 0.0;   // em tempos (semínimas) desde o início da sequência
    double length = 4.0;
};

struct SequenceData
{
    Harmony   rows[kSeqRows];
    SeqRegion regions[kSeqMaxRegions];
    int       numRegions  = 0;
    int       selectedRow = 0;     // usada com o transporte parado / desarmado

    static SequenceData makeDefault();
};

struct SequenceTransport
{
    bool   hostPlaying  = false;
    double ppq          = 0.0;     // posição do host em semínimas
    int    bars         = 4;
    double offsetBeats  = 0.0;     // "session offset"
    double retrigPpq    = 0.0;     // posição em que foi carregado o "retrigger"
    bool   armed        = true;    // segue o transporte do DAW
    int    transpose    = 0;       // semitons
};

struct SequenceState
{
    int      activeRow   = -1;     // -1 = nenhuma
    double   positionBeats = -1.0; // -1 = parado
    uint16_t mask        = 0;
};

class SequenceEngine
{
public:
    // Pura: sem estado escondido além da "última linha" (para manter a harmonia nas pausas)
    SequenceState evaluate (const SequenceData& d, const SequenceTransport& t) noexcept;
    void reset() noexcept { lastRow = -1; }

    static double wrapPosition (double ppq, const SequenceTransport& t) noexcept;
    static int    regionAt (const SequenceData& d, double pos) noexcept;

private:
    int lastRow = -1;
};
} // namespace roomx::music
