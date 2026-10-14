#include "SequenceEngine.h"

namespace roomx::music
{
SequenceData SequenceData::makeDefault()
{
    SequenceData d;
    // Campo harmónico de C maior (I ii iii IV V vi vii° I)
    const int roots[kSeqRows] = { 0, 2, 4, 5, 7, 9, 11, 0 };
    const int kinds[kSeqRows] = { 0, 1, 1, 0, 0, 1, 2, 0 };   // maj, min, dim (índices em kChords)
    for (int i = 0; i < kSeqRows; ++i)
        d.rows[i] = Harmony { roots[i], Category::Chord, kinds[i] };

    // I – vi – IV – V, um compasso cada
    const int prog[4] = { 0, 5, 3, 4 };
    for (int i = 0; i < 4; ++i)
        d.regions[i] = SeqRegion { prog[i], i * 4.0, 4.0 };
    d.numRegions  = 4;
    d.selectedRow = 0;
    return d;
}

double SequenceEngine::wrapPosition (double ppq, const SequenceTransport& t) noexcept
{
    const double total = (double) (t.bars < 1 ? 1 : (t.bars > kSeqMaxBars ? kSeqMaxBars : t.bars)) * kBeatsPerBar;
    double p = ppq - t.offsetBeats - t.retrigPpq;
    p = std::fmod (p, total);
    if (p < 0.0) p += total;
    return p;
}

int SequenceEngine::regionAt (const SequenceData& d, double pos) noexcept
{
    int found = -1;
    for (int i = 0; i < d.numRegions && i < kSeqMaxRegions; ++i)
    {
        const auto& r = d.regions[i];
        if (pos >= r.start && pos < r.start + r.length)
            found = i;                       // a última região desenhada ganha
    }
    return found;
}

SequenceState SequenceEngine::evaluate (const SequenceData& d, const SequenceTransport& t) noexcept
{
    SequenceState s;

    if (t.hostPlaying && t.armed)
    {
        s.positionBeats = wrapPosition (t.ppq, t);
        const int ri = regionAt (d, s.positionBeats);
        if (ri >= 0)
            lastRow = d.regions[ri].row;
        // Nas pausas entre regiões mantém-se a última harmonia (mais musical
        // do que cortar todas as ressonâncias); no início usa a linha seleccionada.
        s.activeRow = lastRow >= 0 ? lastRow : d.selectedRow;
    }
    else
    {
        lastRow = -1;
        s.activeRow = d.selectedRow;
    }

    if (s.activeRow < 0 || s.activeRow >= kSeqRows)
        s.activeRow = 0;
    s.mask = rotatePitchMask (d.rows[s.activeRow].mask(), t.transpose);
    return s;
}
} // namespace roomx::music
