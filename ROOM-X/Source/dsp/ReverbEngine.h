// =============================================================================
//  ROOM-X — motor completo do sinal "wet"
//
//    entrada -> pre-delay -> reflexões iniciais ─┬─> Offset -> banco modal (Early) ─┐
//                         └> atraso de início -> FDN tardio -> Offset -> banco modal (Late) ─┴> filtros wet -> saída
//
//  Dual Mono = dois núcleos independentes (L e R). Mudanças estruturais
//  (espaço, Dual Mono, Eco) fazem um fade curto do wet (sem cliques).
//  Latência: 0 amostras.
// =============================================================================
#pragma once

#include "EarlyReflections.h"
#include "FDNReverb.h"
#include "ModalBank.h"
#include "Processors.h"

namespace roomx::dsp
{
enum class Target : int { Early = 0, All = 1, Late = 2 };

struct EngineParams
{
    float preDelayMs = 0.0f;
    float decay      = 1.0f;      // multiplicador 0.01..30
    float size       = 1.0f;      // 0.5..2
    float position   = 0.0f;      // -99 (Early) .. +99 (Late)
    float density    = 1.0f;      // 0.01..1
    float offsetHz   = 0.0f;
    int   spaceIndex = 0;
    bool  dualMono   = false;
    bool  eco        = false;
    float lowCutHz   = 20.0f;
    int   lowCut36   = 0;
    float highCutHz  = 20000.0f;
    int   highCut36  = 0;
    Target target    = Target::All;
    TemperSettings temper;
};

class ReverbCore
{
public:
    static constexpr int kBlock = ModalBank::kMaxBlock;
    static constexpr double kMaxSize = 2.0;

    void prepare (double sampleRate);
    void reset() noexcept;
    void configure (const SpaceDef& s, bool eco) noexcept;

    void process (const float* inL, const float* inR, float* outL, float* outR, int n,
                  const EngineParams& p, const float* shiftCos, const float* shiftSin, bool shiftOn) noexcept;

private:
    double fs = 48000.0;
    const SpaceDef* def = nullptr;
    CrossfadeDelay preDelay;
    DelayLine onset[2];
    EarlyReflections er;
    FDNReverb fdn;
    FrequencyShifter shift[2][2];
    ModalBank bankEarly, bankLate;
    WetFilters filters;
    Smoother sizeSm, earlyGainSm, lateGainSm;
    float temperEarly = 0.0f, temperLate = 0.0f;

    float bIn[2][kBlock] {}, bEr[2][kBlock] {}, bLate[2][kBlock] {}, bOn[2][kBlock] {}, bSize[kBlock] {};
};

class ReverbEngine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset() noexcept;

    // processa n amostras (qualquer tamanho); in/out podem ser iguais
    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const EngineParams& p) noexcept;

    static float effectiveRt60 (int spaceIndex, float decay, float size) noexcept;
    double getSampleRate() const noexcept { return fs; }

private:
    double fs = 48000.0;
    ReverbCore cores[2];
    int  curSpace = -1;
    bool curDual = false, curEco = false;
    float structFade = 1.0f;      // 0..1
    int   fadeDir = 0;            // -1 a descer, +1 a subir
    float fadeStep = 0.01f;
    double shiftPhase = 0.0;
    float shiftCos[ReverbCore::kBlock] {}, shiftSin[ReverbCore::kBlock] {};
    float tmp[4][ReverbCore::kBlock] {};
    bool  firstBlock = true;

    void applyStructure (const EngineParams& p) noexcept;
};
} // namespace roomx::dsp
