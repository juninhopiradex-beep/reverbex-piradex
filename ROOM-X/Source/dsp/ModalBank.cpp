#include "ModalBank.h"

namespace roomx::dsp
{
using namespace roomx::music;

float ModalBank::bandwidthSemitones (float w) noexcept
{
    // 1 % -> 0.03 semitom (sino puro, cauda longa)  50 % -> ~0.55  99 % -> 1.6 semitons
    const float u = std::clamp ((w - 0.01f) / 0.98f, 0.0f, 1.0f);
    return 0.03f + 1.57f * std::pow (u, 1.6f);
}

void ModalBank::prepare (double sampleRate)
{
    fs = sampleRate;
    cachedWidth = -1.0f;
    reset();
}

void ModalBank::reset() noexcept
{
    for (auto& ch : state)
        for (auto& st : ch) st.reset();
    for (int k = 0; k < kNumNotes; ++k) { gain[k] = 0.0f; active[k] = false; }
    numActive = 0;
    for (auto& ch : splitLow)  for (auto& st : ch) st.reset();
    for (auto& ch : splitHigh) for (auto& st : ch) st.reset();
    temperNow = 0.0f;
}

void ModalBank::updateCoefficients (const TemperSettings& s) noexcept
{
    const bool changed = s.noteWidth != cachedWidth
                      || (float) s.tuning.referenceHz != cachedRef
                      || (float) s.tuning.fineCents != cachedCents
                      || s.minBandwidthHz != cachedMinBw;
    if (changed)
    {
        cachedWidth = s.noteWidth;
        cachedRef   = (float) s.tuning.referenceHz;
        cachedCents = (float) s.tuning.fineCents;
        cachedMinBw = s.minBandwidthHz;

        const double bw = bandwidthSemitones (s.noteWidth);
        const double qFromBw = 1.0 / (std::pow (2.0, bw / 24.0) - std::pow (2.0, -bw / 24.0));
        for (int k = 0; k < kNumNotes; ++k)
        {
            const double f = s.tuning.noteToHz (kNoteMin + k);
            double q = qFromBw;
            if (s.minBandwidthHz > 0.0f)
                q = std::min (q, f / (double) s.minBandwidthHz);
            coeffs[k].set (fs, f, std::max (0.5, q));
        }
    }

    if (changed || s.rangeLow != cachedLow || s.rangeHigh != cachedHigh)
    {
        cachedLow = s.rangeLow;
        cachedHigh = s.rangeHigh;
        useLow  = s.rangeLow > (float) kNoteMin + 0.25f;
        const double fHigh = s.tuning.noteToHz (s.rangeHigh + 0.5);
        useHigh = s.rangeHigh < (float) kNoteMax - 0.25f && fHigh < 0.45 * fs;
        // Butterworth de 4.ª ordem (2 secções) para uma fronteira nítida
        splitLowC[0].set  (fs, s.tuning.noteToHz (s.rangeLow - 0.5), 0.5411961);
        splitLowC[1].set  (fs, s.tuning.noteToHz (s.rangeLow - 0.5), 1.3065630);
        splitHighC[0].set (fs, fHigh, 0.5411961);
        splitHighC[1].set (fs, fHigh, 1.3065630);
    }
}

void ModalBank::process (float* L, float* R, int n, const TemperSettings& s) noexcept
{
    n = std::min (n, kMaxBlock);
    const float target = std::clamp (s.temper, -1.0f, 1.0f);

    // ---- totalmente neutro: devolve o sinal intacto (bit-exact)
    if (std::fabs (target) < 1.0e-5f && std::fabs (temperNow) < 1.0e-5f)
    {
        if (numActive > 0 || temperNow != 0.0f) reset();
        return;
    }

    updateCoefficients (s);

    // ---- alvo de cada ressoador
    const double nyq = 0.45 * fs;
    int inRange = 0, selected = 0;
    const float tauA = (float) std::max (1.0, s.attackMs * 0.001 * fs);
    const float tauR = (float) std::max (1.0, s.releaseMs * 0.001 * fs);
    const float decA = std::exp (-(float) n / tauA);
    const float decR = std::exp (-(float) n / tauR);

    float g0[kNumNotes], g1[kNumNotes];
    numActive = 0;
    for (int k = 0; k < kNumNotes; ++k)
    {
        const int note = kNoteMin + k;
        const bool valid = (float) note >= s.rangeLow - 0.01f && (float) note <= s.rangeHigh + 0.01f
                        && s.tuning.noteToHz (note) < nyq;
        if (valid) ++inRange;
        const bool on = valid && s.notes.test (note);
        if (on) ++selected;

        const float tgt = on ? 1.0f : 0.0f;
        g0[k] = gain[k];
        g1[k] = tgt + (gain[k] - tgt) * (tgt > gain[k] ? decA : decR);
        if (! on && g1[k] < 1.0e-4f && g0[k] < 1.0e-4f)
        {
            g1[k] = 0.0f;
            if (active[k]) { state[0][k].reset(); state[1][k].reset(); active[k] = false; }
        }
        else
        {
            active[k] = true;
            ++numActive;
        }
        gain[k] = g1[k];
    }

    // ---- compensação de nível para Temper positivo
    const float bwEq = 1.5708f * bandwidthSemitones (s.noteWidth);   // largura de ruído equivalente
    float p = inRange > 0 ? ((float) selected / (float) inRange) * bwEq : 1.0f;
    p = std::clamp (p, 0.004f, 1.0f);
    const float comp = std::clamp (1.0f / std::sqrt (p), 1.0f, 6.0f);

    // ---- curva Temper -> ganhos (início e fim do bloco, interpolação linear)
    auto gains = [] (float t, float c, float& mid, float& res)
    {
        const float a = std::fabs (t);
        if (t >= 0.0f) { mid = std::pow (1.0f - a, 1.5f); res = c * std::pow (a, 0.8f); }
        else           { mid = 1.0f;                       res = -std::pow (a, 0.7f); }
    };
    float mid0, res0, mid1, res1;
    gains (temperNow, compNow, mid0, res0);
    const float tDec = std::exp (-(float) n / (float) (0.025 * fs));
    temperNow = target + (temperNow - target) * tDec;
    if (std::fabs (temperNow - target) < 1.0e-5f) temperNow = target;
    compNow = comp + (compNow - comp) * tDec;
    gains (temperNow, compNow, mid1, res1);

    // ---- banco de ressoadores (laço por nota, estado em registos)
    float* io[2] = { L, R };
    for (int ch = 0; ch < 2; ++ch)
    {
        std::copy (io[ch], io[ch] + n, bufIn[ch]);
        std::fill (bufRes[ch], bufRes[ch] + n, 0.0f);
    }

    const float invN = 1.0f / (float) n;

    // notas activas processadas aos pares: 4 recursões independentes por
    // iteração (nota A L/R, nota B L/R) -> o compilador vectoriza/intercala
    int list[kNumNotes];
    int count = 0;
    for (int k = 0; k < kNumNotes; ++k)
        if (active[k]) list[count++] = k;

    const float* xL = bufIn[0];
    const float* xR = bufIn[1];
    float* rL = bufRes[0];
    float* rR = bufRes[1];

    for (int idx = 0; idx < count; idx += 2)
    {
        const int ka = list[idx];
        const int kb = idx + 1 < count ? list[idx + 1] : -1;
        alignas (16) float a1[4], a2[4], a3[4], kk[4], s1[4], s2[4], g[4], dg[4];
        for (int lane = 0; lane < 4; ++lane)
        {
            const int k  = lane < 2 ? ka : kb;
            const int ch = lane & 1;
            if (k < 0) { a1[lane] = a2[lane] = a3[lane] = kk[lane] = s1[lane] = s2[lane] = g[lane] = dg[lane] = 0.0f; continue; }
            const auto& c = coeffs[k];
            a1[lane] = c.a1; a2[lane] = c.a2; a3[lane] = c.a3; kk[lane] = c.k;
            s1[lane] = state[ch][k].ic1; s2[lane] = state[ch][k].ic2;
            g[lane]  = g0[k];
            dg[lane] = (g1[k] - g0[k]) * invN;
        }

        for (int i = 0; i < n; ++i)
        {
            const float xin[4] = { xL[i], xR[i], xL[i], xR[i] };
            float out[4];
            for (int lane = 0; lane < 4; ++lane)
            {
                const float v3 = xin[lane] - s2[lane];
                const float v1 = a1[lane] * s1[lane] + a2[lane] * v3;
                const float v2 = s2[lane] + a2[lane] * s1[lane] + a3[lane] * v3;
                s1[lane] = 2.0f * v1 - s1[lane];
                s2[lane] = 2.0f * v2 - s2[lane];
                g[lane] += dg[lane];
                out[lane] = g[lane] * kk[lane] * v1;
            }
            rL[i] += out[0] + out[2];
            rR[i] += out[1] + out[3];
        }

        for (int lane = 0; lane < 4; ++lane)
        {
            const int k  = lane < 2 ? ka : kb;
            if (k < 0) continue;
            const int ch = lane & 1;
            state[ch][k].ic1 = flushDenormal (s1[lane]);
            state[ch][k].ic2 = flushDenormal (s2[lane]);
        }
    }

    // ---- recombinação
    for (int ch = 0; ch < 2; ++ch)
    {
        float* y = io[ch];
        for (int i = 0; i < n; ++i)
        {
            const float f  = (float) (i + 1) * invN;
            const float mg = mid0 + (mid1 - mid0) * f;
            const float rg = res0 + (res1 - res0) * f;
            const float x  = bufIn[ch][i];
            const float lo = useLow  ? splitLow[ch][1].lowpass (splitLowC[1], splitLow[ch][0].lowpass (splitLowC[0], x))      : 0.0f;
            const float hi = useHigh ? splitHigh[ch][1].highpass (splitHighC[1], splitHigh[ch][0].highpass (splitHighC[0], x)) : 0.0f;
            const float m  = x - lo - hi;
            y[i] = lo + hi + mg * m + rg * bufRes[ch][i];
        }
    }
}
} // namespace roomx::dsp
