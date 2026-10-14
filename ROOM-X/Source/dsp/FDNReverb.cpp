#include "FDNReverb.h"

namespace roomx::dsp
{
namespace
{
// ordem de atribuição dos comprimentos: as 8 primeiras linhas (modo Eco)
// cobrem toda a gama de comprimentos
constexpr int kLenOrder[16] = { 0, 8, 2, 10, 4, 12, 6, 14, 1, 9, 3, 11, 5, 13, 7, 15 };
// padrões de sinal de saída ortogonais (decorrelação L/R)
constexpr float kOutL[16] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
constexpr float kOutR[16] = { 1,-1, 1,-1,-1, 1,-1, 1, 1,-1, 1,-1,-1, 1,-1, 1 };
constexpr float kIn[16]   = { 1, 1,-1, 1, 1,-1,-1, 1,-1, 1, 1,-1, 1,-1, 1, 1 };
}

void FDNReverb::prepare (double sampleRate, double maxSizeScale)
{
    fs = sampleRate;
    const int maxLine = (int) std::ceil ((0.165 * maxSizeScale + 0.012) * fs) + 8;
    for (auto& l : lines) l.allocate (maxLine);
    for (auto& ch : diffusers)
        for (auto& d : ch) d.allocate ((int) (0.012 * fs) + 4);
    configure (space (0));
}

void FDNReverb::reset() noexcept
{
    for (auto& l : lines) l.clear();
    for (auto& ch : diffusers)
        for (auto& d : ch) d.clear();
    for (int i = 0; i < kMaxLines; ++i)
    {
        lfShelf[i].reset(); hfShelf[i].reset();
        apState[i] = 0.0f;
    }
    for (auto& ch : inputAp)
        for (auto& a : ch) a = 0.0f;
}

void FDNReverb::setNumLines (int n) noexcept
{
    numLines = n <= 8 ? 8 : 16;
    lastTheta = -1.0f;
}

void FDNReverb::configure (const SpaceDef& s) noexcept
{
    def = &s;
    Rng rng (s.seed * 31337u + 5u);

    float sorted[kMaxLines];
    for (int i = 0; i < kMaxLines; ++i)
    {
        const float u  = (float) i / (float) (kMaxLines - 1);
        const float ms = s.lateMinMs + (s.lateMaxMs - s.lateMinMs) * std::pow (u, 1.15f);
        sorted[i] = ms * (1.0f + 0.045f * rng.bipolar());
    }
    std::sort (sorted, sorted + kMaxLines);

    for (int i = 0; i < kMaxLines; ++i)
    {
        // comprimentos em amostras, evitando valores iguais entre linhas
        baseLen[i] = std::max (8.0f, (float) (sorted[kLenOrder[i]] * 0.001 * fs) + (float) i * 0.37f);
        inSign[i]   = kIn[i];
        outSignL[i] = kOutL[i];
        outSignR[i] = kOutR[i];

        const float rate = s.modRateHz * (0.65f + 0.7f * rng.uniform());
        const float w = (float) (kTwoPi * rate / fs);
        oscRotC[i] = std::cos (w);
        oscRotS[i] = std::sin (w);
        const float ph = (float) kTwoPi * rng.uniform();
        oscC[i] = std::cos (ph);
        oscS[i] = std::sin (ph);

        lfShelf[i].setCutoff (fs, s.lfHz);
        hfShelf[i].setCutoff (fs, s.dampHz);
    }

    modDepth   = (float) (s.modDepthMs * 0.001 * fs);
    dispersion = std::clamp (s.dispersion, 0.0f, 0.95f);
    width      = s.width;

    const float diffMs[4] = { 1.31f, 2.87f, 4.73f, 7.07f };
    for (int ch = 0; ch < 2; ++ch)
        for (int k = 0; k < 4; ++k)
            diffusers[ch][k].setDelay ((int) ((diffMs[k] + 0.29f * (float) ch * (float) (k + 1)) * 0.001 * fs));

    lastTheta = -1.0f;
    setDecay (s.rt60, 1.0f);
}

void FDNReverb::setDecay (float rt60, float sizeScale) noexcept
{
    if (def == nullptr) return;
    rt60 = std::clamp (rt60, 0.03f, 600.0f);
    const float lowRt  = rt60 * std::max (0.3f, def->lfRatio);
    const float highRt = rt60 * std::clamp (def->hfRatio, 0.05f, 1.0f);
    const double k = -3.0 / fs;

    for (int i = 0; i < numLines; ++i)
    {
        const double len = (double) baseLen[i] * sizeScale;
        const float gm = (float) std::pow (10.0, k * len / rt60);
        const float gl = (float) std::pow (10.0, k * len / lowRt);
        const float gh = (float) std::pow (10.0, k * len / highRt);
        gMid[i]   = std::min (gm, 0.99995f);
        gLow[i]   = std::min (gl, 0.99995f);
        hRatio[i] = std::clamp (gh / std::max (gm, 1.0e-6f), 0.0f, 1.0f);
    }
    outGain = 1.0f / std::sqrt ((float) numLines) * def->lateGain;
}

void FDNReverb::mix (float* v) const noexcept
{
    // andares de rotações de Givens (ortogonais para qualquer θ)
    for (int span = 1; span < numLines; span <<= 1)
        for (int i = 0; i < numLines; ++i)
            if ((i & span) == 0)
            {
                const float a = v[i], b = v[i + span];
                v[i]        =  cosT * a + sinT * b;
                v[i + span] = -sinT * a + cosT * b;
            }
}

void FDNReverb::process (const float* inL, const float* inR, float* outL, float* outR,
                         const float* sizeBuf, int n, float density) noexcept
{
    density = std::clamp (density, 0.0f, 1.0f);
    const float theta = (float) (kPi * 0.25) * (0.06f + 0.94f * std::pow (density, 0.6f));
    if (std::fabs (theta - lastTheta) > 1.0e-5f)
    {
        lastTheta = theta;
        cosT = std::cos (theta);
        sinT = std::sin (theta);
    }

    const float diffG = (def != nullptr ? std::clamp (def->diffusion, 0.0f, 0.8f) : 0.6f) * (0.25f + 0.75f * density);
    const float apA   = 0.62f * dispersion;
    const float lineAp = 0.55f * dispersion;
    const float inGain = 1.0f / std::sqrt ((float) numLines * 0.5f);
    const float maxD  = (float) lines[0].capacity() - 4.0f;

    float v[kMaxLines];

    for (int s = 0; s < n; ++s)
    {
        // ---- entrada: difusão (+ dispersão para molas)
        float x[2] = { inL[s], inR[s] };
        for (int ch = 0; ch < 2; ++ch)
        {
            float y = x[ch];
            for (int k = 0; k < 4; ++k)
                y = diffusers[ch][k].process (y, diffG * (k < 2 ? 1.0f : 0.85f));
            if (apA > 0.0f)
                for (int k = 0; k < 8; ++k)
                {
                    const float o = apA * y + inputAp[ch][k];
                    inputAp[ch][k] = flushDenormal (y - apA * o);
                    y = o;
                }
            x[ch] = y * inGain;
        }

        // ---- leitura das linhas + absorção
        const float sc = sizeBuf[s];
        float yl = 0.0f, yr = 0.0f;
        for (int i = 0; i < numLines; ++i)
        {
            // oscilador em quadratura
            const float c = oscC[i] * oscRotC[i] - oscS[i] * oscRotS[i];
            const float sn = oscS[i] * oscRotC[i] + oscC[i] * oscRotS[i];
            oscC[i] = c; oscS[i] = sn;

            const float d = std::clamp (baseLen[i] * sc + modDepth * (1.0f + sn), 3.0f, maxD);
            float val = lines[i].readFrac (d);

            // prateleira de graves (monótona)
            const float lp1 = lfShelf[i].process (val);
            val = gMid[i] * val + (gLow[i] - gMid[i]) * lp1;
            // prateleira de agudos (monótona, ganho <= 1)
            const float lp2 = hfShelf[i].process (val);
            val = lp2 + hRatio[i] * (val - lp2);
            // passa-tudo dispersivo (|H| = 1)
            if (lineAp > 0.0f)
            {
                const float o = lineAp * val + apState[i];
                apState[i] = flushDenormal (val - lineAp * o);
                val = o;
            }
            v[i] = val;
            yl += outSignL[i] * val;
            yr += outSignR[i] * val;
        }

        mix (v);

        for (int i = 0; i < numLines; ++i)
            lines[i].push (flushDenormal (v[i] + inSign[i] * x[i & 1]));

        yl *= outGain; yr *= outGain;
        const float mid = 0.5f * (yl + yr), side = 0.5f * (yl - yr) * width;
        outL[s] = mid + side;
        outR[s] = mid - side;
    }

    if (++oscRenorm >= 64)
    {
        oscRenorm = 0;
        for (int i = 0; i < kMaxLines; ++i)
        {
            const float m = 1.0f / std::sqrt (oscC[i] * oscC[i] + oscS[i] * oscS[i]);
            oscC[i] *= m; oscS[i] *= m;
        }
    }
}
} // namespace roomx::dsp
