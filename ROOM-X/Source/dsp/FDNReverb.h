// =============================================================================
//  ROOM-X — cauda tardia: FDN de 16 (ou 8 em modo Eco) linhas
//
//  Matriz de realimentação = 4 (ou 3) andares de rotações de Givens em
//  "butterfly". É ortogonal para qualquer ângulo θ, logo estável:
//    θ = 45°  -> mistura total (Hadamard rodado)  = cauda densa e lisa
//    θ -> 0°  -> linhas quase independentes (pentes) = modos esparsos/metálicos
//  Density controla θ. Absorção por linha: duas prateleiras de 1.ª ordem
//  monótonas (graves / agudos) com ganhos derivados do RT60 desejado.
// =============================================================================
#pragma once

#include "DspCommon.h"
#include "SpaceLibrary.h"

namespace roomx::dsp
{
class FDNReverb
{
public:
    static constexpr int kMaxLines = 16;

    void prepare (double sampleRate, double maxSizeScale);
    void reset() noexcept;
    void configure (const SpaceDef& s) noexcept;
    void setNumLines (int n) noexcept;          // 8 ou 16 (chamar com o sinal em silêncio)
    int  getNumLines() const noexcept { return numLines; }

    // rt60 já inclui Decay e Size; density 0..1
    void setDecay (float rt60Seconds, float sizeScale) noexcept;

    void process (const float* inL, const float* inR, float* outL, float* outR,
                  const float* sizeBuf, int n, float density) noexcept;

private:
    double fs = 48000.0;
    int numLines = kMaxLines;
    const SpaceDef* def = nullptr;

    DelayLine lines[kMaxLines];
    float baseLen[kMaxLines] {};          // amostras a Size 100 %
    float gLow[kMaxLines] {}, gMid[kMaxLines] {}, hRatio[kMaxLines] {};
    OnePole lfShelf[kMaxLines], hfShelf[kMaxLines];
    float apState[kMaxLines] {};          // passa-tudo dispersivo de 1.ª ordem por linha
    float dispersion = 0.0f;

    // modulação: oscilador em quadratura por linha
    float oscC[kMaxLines] {}, oscS[kMaxLines] {}, oscRotC[kMaxLines] {}, oscRotS[kMaxLines] {};
    float modDepth = 0.0f;

    AllpassDiffuser diffusers[2][4];
    float inputAp[2][8] {};               // cadeia dispersiva de entrada (molas)
    float inSign[kMaxLines] {}, outSignL[kMaxLines] {}, outSignR[kMaxLines] {};
    float outGain = 1.0f;
    float width = 1.0f;
    float lastTheta = -1.0f, cosT = 0.7071f, sinT = 0.7071f;
    int   oscRenorm = 0;

    void mix (float* v) const noexcept;
};
} // namespace roomx::dsp
