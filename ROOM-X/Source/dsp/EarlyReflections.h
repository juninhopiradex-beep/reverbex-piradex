// =============================================================================
//  ROOM-X — reflexões iniciais (rede multi-tap esparsa e decorrelacionada)
//
//  Cada espaço gera, a partir da sua "seed", 32 taps por canal (tempo,
//  ganho, sinal, canal de origem). Size escala os tempos (com suavização,
//  leitura Hermite); Density desliga taps de forma determinística.
// =============================================================================
#pragma once

#include "DspCommon.h"
#include "SpaceLibrary.h"

namespace roomx::dsp
{
class EarlyReflections
{
public:
    static constexpr int kMaxTaps = 32;

    void prepare (double sampleRate, double maxSizeScale);
    void reset() noexcept;
    void configure (const SpaceDef& s) noexcept;     // sem alocação

    // density 0..1; sizeScale por amostra é passado em sizeBuf
    void process (const float* inL, const float* inR, float* outL, float* outR,
                  const float* sizeBuf, int n, float density) noexcept;

private:
    struct Tap { float delay; float gain; int src; int order; };

    double fs = 48000.0;
    DelayLine line[2];
    Tap taps[2][kMaxTaps] {};
    int numTaps = 16;
    float norm = 1.0f;
    OnePole damp[2];
    AllpassDiffuser diff[2][2];
    float diffusion = 0.5f;
    float lastDensity = -1.0f;
    float tapWeight[2][kMaxTaps] {};
    float gain = 1.0f;

    void updateDensity (float density) noexcept;
};
} // namespace roomx::dsp
