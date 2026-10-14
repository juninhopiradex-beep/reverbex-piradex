#include "ReverbEngine.h"

namespace roomx::dsp
{
// =============================================================================
//  ReverbCore
// =============================================================================
void ReverbCore::prepare (double sampleRate)
{
    fs = sampleRate;
    preDelay.prepare (fs, 2.05);
    for (auto& o : onset) o.allocate ((int) ((0.30 * kMaxSize + 0.01) * fs) + 8);
    er.prepare (fs, kMaxSize);
    fdn.prepare (fs, kMaxSize);
    for (auto& path : shift)
        for (auto& s : path) s.prepare (fs);
    bankEarly.prepare (fs);
    bankLate.prepare (fs);
    filters.prepare (fs);
    sizeSm.prepare (fs, 220.0);
    earlyGainSm.prepare (fs, 40.0);
    lateGainSm.prepare (fs, 40.0);
    sizeSm.reset (1.0f);
    earlyGainSm.reset (1.0f);
    lateGainSm.reset (1.0f);
    configure (space (0), false);
}

void ReverbCore::reset() noexcept
{
    preDelay.reset();
    for (auto& o : onset) o.clear();
    er.reset();
    fdn.reset();
    for (auto& path : shift)
        for (auto& s : path) s.reset();
    bankEarly.reset();
    bankLate.reset();
    filters.reset();
}

void ReverbCore::configure (const SpaceDef& s, bool eco) noexcept
{
    def = &s;
    er.configure (s);
    fdn.setNumLines (eco ? 8 : 16);
    fdn.configure (s);
}

void ReverbCore::process (const float* inL, const float* inR, float* outL, float* outR, int n,
                          const EngineParams& p, const float* shiftCos, const float* shiftSin, bool shiftOn) noexcept
{
    n = std::min (n, kBlock);
    const SpaceDef& s = def != nullptr ? *def : space (0);

    // ---- suavizações por amostra
    sizeSm.setTarget (std::clamp (p.size, 0.5f, (float) kMaxSize));
    for (int i = 0; i < n; ++i) bSize[i] = sizeSm.next();

    const float pos = std::clamp (p.position, -99.0f, 99.0f) / 99.0f;
    earlyGainSm.setTarget (pos > 0.0f ? 1.0f - 0.88f * pos : 1.0f + 0.30f * (-pos));
    lateGainSm.setTarget  (pos < 0.0f ? 1.0f - 0.88f * (-pos) : 1.0f + 0.25f * pos);

    // ---- pre-delay
    preDelay.setDelaySamples ((int) std::lround (std::clamp (p.preDelayMs, 0.0f, 2000.0f) * 0.001 * fs));
    for (int i = 0; i < n; ++i)
    {
        float l = inL[i], r = inR[i];
        preDelay.process (l, r);
        bIn[0][i] = l; bIn[1][i] = r;
    }

    // ---- reflexões iniciais
    const float density = std::clamp (p.density, 0.01f, 1.0f);
    er.process (bIn[0], bIn[1], bEr[0], bEr[1], bSize, n, density);

    // ---- início da cauda + FDN
    const float onsetBase = s.lateOnsetMs * 0.001f * (float) fs;
    const float maxOn = (float) onset[0].capacity() - 4.0f;
    for (int i = 0; i < n; ++i)
    {
        onset[0].push (bIn[0][i]);
        onset[1].push (bIn[1][i]);
        const float d = std::clamp (onsetBase * bSize[i], 2.0f, maxOn);
        bOn[0][i] = 0.85f * onset[0].readFrac (d) + 0.30f * bEr[0][i];
        bOn[1][i] = 0.85f * onset[1].readFrac (d) + 0.30f * bEr[1][i];
    }
    const float sizeNow = bSize[n - 1];
    fdn.setDecay (s.rt60 * std::clamp (p.decay, 0.01f, 30.0f) * sizeNow, sizeNow);
    fdn.process (bOn[0], bOn[1], bLate[0], bLate[1], bSize, n, density);

    // ---- Offset (desvio de frequência de todos os modos)
    if (shiftOn)
    {
        for (int i = 0; i < n; ++i)
        {
            bEr[0][i]   = shift[0][0].process (bEr[0][i],   shiftCos[i], shiftSin[i]);
            bEr[1][i]   = shift[0][1].process (bEr[1][i],   shiftCos[i], shiftSin[i]);
            bLate[0][i] = shift[1][0].process (bLate[0][i], shiftCos[i], shiftSin[i]);
            bLate[1][i] = shift[1][1].process (bLate[1][i], shiftCos[i], shiftSin[i]);
        }
    }

    // ---- "tempering" musical por caminho (Target)
    TemperSettings ts = p.temper;
    const bool earlyOn = p.target != Target::Late;
    const bool lateOn  = p.target != Target::Early;

    ts.temper = earlyOn ? p.temper.temper : 0.0f;
    ts.minBandwidthHz = 3.0f;                     // ressoadores do Early não tocam para sempre
    bankEarly.process (bEr[0], bEr[1], n, ts);

    ts.temper = lateOn ? p.temper.temper : 0.0f;
    ts.minBandwidthHz = 0.0f;
    bankLate.process (bLate[0], bLate[1], n, ts);

    // ---- soma, filtros wet
    filters.set (p.lowCutHz, p.lowCut36, p.highCutHz, p.highCut36);
    for (int i = 0; i < n; ++i)
    {
        const float ge = earlyGainSm.next(), gl = lateGainSm.next();
        float l = ge * bEr[0][i] + gl * bLate[0][i];
        float r = ge * bEr[1][i] + gl * bLate[1][i];
        filters.process (l, r);
        outL[i] = l;
        outR[i] = r;
    }
}

// =============================================================================
//  ReverbEngine
// =============================================================================
float ReverbEngine::effectiveRt60 (int spaceIndex, float decay, float size) noexcept
{
    return std::clamp (space (spaceIndex).rt60 * std::clamp (decay, 0.01f, 30.0f) * std::clamp (size, 0.5f, 2.0f), 0.03f, 600.0f);
}

void ReverbEngine::prepare (double sampleRate, int)
{
    fs = sampleRate;
    for (auto& c : cores) c.prepare (fs);
    fadeStep = (float) (1.0 / (0.015 * fs));
    firstBlock = true;
    curSpace = -1;
    shiftPhase = 0.0;
}

void ReverbEngine::reset() noexcept
{
    for (auto& c : cores) c.reset();
    shiftPhase = 0.0;
}

void ReverbEngine::applyStructure (const EngineParams& p) noexcept
{
    curSpace = std::clamp (p.spaceIndex, 0, kNumSpaces - 1);
    curDual  = p.dualMono;
    curEco   = p.eco;
    for (auto& c : cores)
    {
        c.configure (space (curSpace), curEco);
        c.reset();
    }
}

void ReverbEngine::process (const float* inL, const float* inR, float* outL, float* outR, int n, const EngineParams& p) noexcept
{
    const int wantSpace = std::clamp (p.spaceIndex, 0, kNumSpaces - 1);
    if (firstBlock)
    {
        applyStructure (p);
        firstBlock = false;
        structFade = 1.0f;
        fadeDir = 0;
    }

    const bool structureDiffers = wantSpace != curSpace || p.dualMono != curDual || p.eco != curEco;
    if (structureDiffers && fadeDir == 0)
        fadeDir = -1;

    const double dphi = kTwoPi * (double) std::clamp (p.offsetHz, -500.0f, 500.0f) / fs;
    const bool shiftOn = std::fabs (p.offsetHz) > 0.05f;

    int done = 0;
    while (done < n)
    {
        const int m = std::min (ReverbCore::kBlock, n - done);

        if (shiftOn)
        {
            for (int i = 0; i < m; ++i)
            {
                shiftPhase += dphi;
                if (shiftPhase > kTwoPi) shiftPhase -= kTwoPi;
                if (shiftPhase < 0.0)    shiftPhase += kTwoPi;
                shiftCos[i] = (float) std::cos (shiftPhase);
                shiftSin[i] = (float) std::sin (shiftPhase);
            }
        }

        const float* iL = inL + done;
        const float* iR = inR + done;
        float* oL = outL + done;
        float* oR = outR + done;

        if (! curDual)
        {
            cores[0].process (iL, iR, tmp[0], tmp[1], m, p, shiftCos, shiftSin, shiftOn);
        }
        else
        {
            // dois espaços mono independentes
            cores[0].process (iL, iL, tmp[0], tmp[2], m, p, shiftCos, shiftSin, shiftOn);
            cores[1].process (iR, iR, tmp[3], tmp[1], m, p, shiftCos, shiftSin, shiftOn);
            for (int i = 0; i < m; ++i)
            {
                tmp[0][i] = 0.5f * (tmp[0][i] + tmp[2][i]);
                tmp[1][i] = 0.5f * (tmp[3][i] + tmp[1][i]);
            }
        }

        // segurança: NaN/Inf -> reinicia o motor em silêncio
        float check = 0.0f;
        for (int i = 0; i < m; ++i) check += tmp[0][i] * 0.0f + tmp[1][i] * 0.0f;
        if (! std::isfinite (check) || ! std::isfinite (tmp[0][m - 1]) || ! std::isfinite (tmp[1][m - 1]))
        {
            for (auto& c : cores) c.reset();
            std::fill (tmp[0], tmp[0] + m, 0.0f);
            std::fill (tmp[1], tmp[1] + m, 0.0f);
        }

        for (int i = 0; i < m; ++i)
        {
            if (fadeDir < 0)
            {
                structFade -= fadeStep;
                if (structFade <= 0.0f)
                {
                    structFade = 0.0f;
                    applyStructure (p);
                    fadeDir = 1;
                }
            }
            else if (fadeDir > 0)
            {
                structFade += fadeStep;
                if (structFade >= 1.0f) { structFade = 1.0f; fadeDir = 0; }
            }
            oL[i] = tmp[0][i] * structFade;
            oR[i] = tmp[1][i] * structFade;
        }
        done += m;
    }
}
} // namespace roomx::dsp
