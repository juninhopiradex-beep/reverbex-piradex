#include "EarlyReflections.h"

namespace roomx::dsp
{
void EarlyReflections::prepare (double sampleRate, double maxSizeScale)
{
    fs = sampleRate;
    const int maxSamples = (int) std::ceil ((0.45 * maxSizeScale + 0.01) * fs) + 8;
    for (auto& l : line) l.allocate (maxSamples);
    for (auto& ch : diff)
        for (auto& d : ch) d.allocate ((int) (0.02 * fs) + 4);
    configure (space (0));
}

void EarlyReflections::reset() noexcept
{
    for (auto& l : line) l.clear();
    for (auto& ch : diff)
        for (auto& d : ch) d.clear();
    for (auto& d : damp) d.reset();
}

void EarlyReflections::configure (const SpaceDef& s) noexcept
{
    numTaps = std::clamp (s.erTaps, 4, kMaxTaps);
    gain = s.erGain;
    diffusion = std::clamp (s.erDiffusion, 0.0f, 0.75f);

    for (int ch = 0; ch < 2; ++ch)
    {
        Rng rng (s.seed * 7919u + (unsigned) ch * 104729u + 17u);
        for (int i = 0; i < numTaps; ++i)
        {
            const float u   = std::clamp (((float) i + 0.5f + 0.8f * rng.bipolar() * 0.5f) / (float) numTaps, 0.0f, 1.0f);
            const float pos = std::pow (u, std::max (0.2f, s.erShape));
            const float tMs = s.erStartMs + pos * std::max (1.0f, s.erLengthMs - s.erStartMs);

            float g;
            if (s.erSlope >= 0.0f)
                g = std::pow (std::max (0.0f, 1.0f - 0.92f * pos), s.erSlope);
            else
                g = std::pow (0.06f + pos, -s.erSlope);           // reflexões que crescem ("swell")
            g *= 0.55f + 0.45f * rng.uniform();
            if (rng.uniform() < 0.45f) g = -g;

            int src = ch;
            if (rng.uniform() < 0.5f * (1.0f - s.erWidth)) src = 1 - ch;

            taps[ch][i].delay = (float) (tMs * 0.001 * fs);
            taps[ch][i].gain  = g;
            taps[ch][i].src   = src;
            taps[ch][i].order = i;
        }

        // ordem determinística de desligar com Density baixa (espalhada no tempo)
        for (int i = numTaps - 1; i > 0; --i)
        {
            const int j = (int) (rng.next() % (uint32_t) (i + 1));
            std::swap (taps[ch][i].order, taps[ch][j].order);
        }

        damp[ch].setCutoff (fs, s.erDampHz);
        diff[ch][0].setDelay ((int) (0.0037 * fs + ch * 0.00041 * fs));
        diff[ch][1].setDelay ((int) (0.0089 * fs + ch * 0.00077 * fs));
    }
    lastDensity = -1.0f;
}

void EarlyReflections::updateDensity (float density) noexcept
{
    if (std::fabs (density - lastDensity) < 1.0e-4f) return;
    lastDensity = density;
    const float active = 2.0f + std::clamp (density, 0.0f, 1.0f) * (float) (numTaps - 2);

    for (int ch = 0; ch < 2; ++ch)
    {
        float energy = 0.0f;
        for (int i = 0; i < numTaps; ++i)
        {
            const float w = std::clamp (active - (float) taps[ch][i].order, 0.0f, 1.0f);
            tapWeight[ch][i] = w;
            energy += w * taps[ch][i].gain * taps[ch][i].gain;
        }
        // normalização de energia comum aos dois canais (usa o canal 0)
        if (ch == 0) norm = gain / std::sqrt (std::max (energy, 1.0e-4f)) * 0.9f;
    }
}

void EarlyReflections::process (const float* inL, const float* inR, float* outL, float* outR,
                                const float* sizeBuf, int n, float density) noexcept
{
    updateDensity (density);
    const float maxD = (float) line[0].capacity() - 4.0f;
    const float g = diffusion * (0.35f + 0.65f * std::clamp (density, 0.0f, 1.0f));

    for (int s = 0; s < n; ++s)
    {
        line[0].push (inL[s]);
        line[1].push (inR[s]);
        const float sc = sizeBuf[s];

        float acc[2] = { 0.0f, 0.0f };
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int i = 0; i < numTaps; ++i)
            {
                const float w = tapWeight[ch][i];
                if (w <= 0.0f) continue;
                const auto& t = taps[ch][i];
                const float d = std::clamp (t.delay * sc, 2.0f, maxD);
                acc[ch] += w * t.gain * line[t.src].readFrac (d);
            }
        }

        for (int ch = 0; ch < 2; ++ch)
        {
            float y = damp[ch].process (acc[ch] * norm);
            y = diff[ch][0].process (y, g);
            y = diff[ch][1].process (y, g * 0.8f);
            (ch == 0 ? outL : outR)[s] = y;
        }
    }
}
} // namespace roomx::dsp
