// =============================================================================
//  ROOM-X — processadores auxiliares do caminho "wet":
//   * FrequencyShifter : Offset (-500..+500 Hz) por banda lateral única (SSB)
//   * WetFilters       : low-cut / high-cut 12 ou 36 dB/oct (SVF em cascata)
//   * CrossfadeDelay   : pre-delay sem "zipper" nem glissando (crossfade de 2 cabeças)
// =============================================================================
#pragma once

#include "DspCommon.h"

namespace roomx::dsp
{
// ---------------------------------------------------------------------------
//  Transformador de Hilbert por dois ramos de passa-tudo (coeficientes de
//  domínio público de O. Niemitalo) + modulação em quadratura.
// ---------------------------------------------------------------------------
class FrequencyShifter
{
public:
    void prepare (double sampleRate) { fs = sampleRate; reset(); }
    void reset() noexcept
    {
        for (auto& v : xa) for (auto& q : v) q = 0;
        for (auto& v : ya) for (auto& q : v) q = 0;
        for (auto& v : xb) for (auto& q : v) q = 0;
        for (auto& v : yb) for (auto& q : v) q = 0;
        bDelay = 0.0f;
    }

    // phaseInc partilhado entre canais (calculado pelo motor)
    inline float process (float x, float cosP, float sinP) noexcept
    {
        static constexpr float a[4] = { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
        static constexpr float b[4] = { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };

        float ia = x, ib = x;
        for (int k = 0; k < 4; ++k)
        {
            const float a2 = a[k] * a[k];
            const float oa = a2 * (ia + ya[k][1]) - xa[k][1];
            xa[k][1] = xa[k][0]; xa[k][0] = ia;
            ya[k][1] = ya[k][0]; ya[k][0] = flushDenormal (oa);
            ia = oa;

            const float b2 = b[k] * b[k];
            const float ob = b2 * (ib + yb[k][1]) - xb[k][1];
            xb[k][1] = xb[k][0]; xb[k][0] = ib;
            yb[k][1] = yb[k][0]; yb[k][0] = flushDenormal (ob);
            ib = ob;
        }
        const float q = bDelay;      // ramo B atrasado 1 amostra
        bDelay = ib;
        return ia * cosP - q * sinP;
    }

private:
    double fs = 48000.0;
    float xa[4][2] {}, ya[4][2] {}, xb[4][2] {}, yb[4][2] {};
    float bDelay = 0.0f;
};

// ---------------------------------------------------------------------------
class WetFilters
{
public:
    void prepare (double sampleRate) { fs = sampleRate; reset(); lastLow = lastHigh = -1.0f; }
    void reset() noexcept
    {
        for (auto& ch : lc) for (auto& s : ch) s.reset();
        for (auto& ch : hc) for (auto& s : ch) s.reset();
    }

    void set (float lowHz, int lowSlope36, float highHz, int highSlope36) noexcept
    {
        if (lowHz != lastLow || lowSlope36 != lastLowSlope)
        {
            lastLow = lowHz; lastLowSlope = lowSlope36;
            lowOn = lowHz > 20.5f;
            setCascade (lcC, lowHz, lowSlope36 != 0, nLow);
        }
        if (highHz != lastHigh || highSlope36 != lastHighSlope)
        {
            lastHigh = highHz; lastHighSlope = highSlope36;
            highOn = highHz < 19900.0f && highHz < 0.47 * fs;
            setCascade (hcC, highHz, highSlope36 != 0, nHigh);
        }
    }

    inline void process (float& l, float& r) noexcept
    {
        if (lowOn)
            for (int i = 0; i < nLow; ++i) { l = lc[0][i].highpass (lcC[i], l); r = lc[1][i].highpass (lcC[i], r); }
        if (highOn)
            for (int i = 0; i < nHigh; ++i) { l = hc[0][i].lowpass (hcC[i], l); r = hc[1][i].lowpass (hcC[i], r); }
    }

private:
    double fs = 48000.0;
    SvfCoeffs lcC[3], hcC[3];
    SvfState  lc[2][3], hc[2][3];
    int nLow = 1, nHigh = 1;
    bool lowOn = false, highOn = false;
    float lastLow = -1.0f, lastHigh = -1.0f;
    int lastLowSlope = -1, lastHighSlope = -1;

    void setCascade (SvfCoeffs* c, float hz, bool steep, int& count) noexcept
    {
        if (steep)
        {   // Butterworth de 6.ª ordem: Q das 3 secções
            static constexpr double q[3] = { 0.5176380902, 0.7071067812, 1.9318516526 };
            for (int i = 0; i < 3; ++i) c[i].set (fs, hz, q[i]);
            count = 3;
        }
        else
        {
            c[0].set (fs, hz, 0.7071067812);
            count = 1;
        }
    }
};

// ---------------------------------------------------------------------------
class CrossfadeDelay
{
public:
    void prepare (double sampleRate, double maxSeconds)
    {
        fs = sampleRate;
        for (auto& l : line) l.allocate ((int) (maxSeconds * fs) + 8);
        fadeLen = std::max (64, (int) (0.035 * fs));
        reset();
    }
    void reset() noexcept
    {
        for (auto& l : line) l.clear();
        cur = next = target; fadePos = -1;
    }
    void setDelaySamples (int d) noexcept { target = std::clamp (d, 0, line[0].capacity() - 2); }

    inline void process (float& l, float& r) noexcept
    {
        line[0].push (l); line[1].push (r);
        if (fadePos < 0 && target != cur)
        {
            next = target;
            fadePos = 0;
        }
        if (fadePos >= 0)
        {
            const float f = (float) fadePos / (float) fadeLen;
            const float a = std::sqrt (1.0f - f), b = std::sqrt (f);   // potência constante
            l = a * read (0, cur) + b * read (0, next);
            r = a * read (1, cur) + b * read (1, next);
            if (++fadePos >= fadeLen) { cur = next; fadePos = -1; }
        }
        else
        {
            l = read (0, cur);
            r = read (1, cur);
        }
    }

private:
    double fs = 48000.0;
    DelayLine line[2];
    int cur = 0, next = 0, target = 0, fadePos = -1, fadeLen = 1024;
    inline float read (int ch, int d) const noexcept { return line[ch].readInt (d + 1); }
};
} // namespace roomx::dsp
