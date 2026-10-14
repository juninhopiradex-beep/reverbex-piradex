// =============================================================================
//  ROOM-X — camada modal musical ("tempering")
//
//  Banco paralelo de 121 ressoadores TPT-SVF (um por semitom, D#0..D#10)
//  aplicado a um caminho da reverb (reflexões OU cauda). Para cada amostra:
//
//      y = low + high + midGain * mid + resGain * Σ_k g_k · BP_k(x)
//
//  low/high = partes do espectro fora do Temper Range (intocadas);
//  mid      = x - low - high  (reconstrução perfeita quando Temper = 0).
//  Temper > 0: atenua o resto e reforça as notas seleccionadas.
//  Temper < 0: subtrai as notas seleccionadas (entalhes musicais).
//  Só os ressoadores activos (ganho alvo ou actual > 0) são calculados.
// =============================================================================
#pragma once

#include "DspCommon.h"
#include "../music/MusicTheory.h"

namespace roomx::dsp
{
struct TemperSettings
{
    float temper      = 0.0f;     // -1..+1
    float noteWidth   = 0.5f;     // 0.01..0.99
    float rangeLow    = (float) music::kNoteMin;   // em notas (fraccionário)
    float rangeHigh   = (float) music::kNoteMax;
    music::NoteSet notes;         // notas absolutas seleccionadas
    music::Tuning  tuning;
    float attackMs    = 15.0f;
    float releaseMs   = 60.0f;
    float minBandwidthHz = 0.0f;  // limita a duração do "ring" (caminho Early)
    bool  eco         = false;
};

class ModalBank
{
public:
    static constexpr int kMaxBlock = 64;

    void prepare (double sampleRate);
    void reset() noexcept;

    // processa in-place um sub-bloco estéreo (n <= kMaxBlock)
    void process (float* L, float* R, int n, const TemperSettings& s) noexcept;

    int  activeCount() const noexcept { return numActive; }
    bool isIdle() const noexcept      { return numActive == 0 && std::fabs (temperNow) < 1.0e-5f; }

    static float bandwidthSemitones (float noteWidth) noexcept;

private:
    double fs = 48000.0;
    SvfCoeffs coeffs[music::kNumNotes];
    SvfState  state[2][music::kNumNotes];
    float gain[music::kNumNotes] {};        // ganho actual (por nota)
    bool  active[music::kNumNotes] {};
    int   numActive = 0;

    // cache para recalcular coeficientes só quando necessário
    float  cachedWidth = -1.0f, cachedRef = -1.0f, cachedCents = 1.0e9f, cachedMinBw = -1.0f;
    float  cachedLow = -1.0f, cachedHigh = -1.0f;

    SvfCoeffs splitLowC[2], splitHighC[2];
    SvfState  splitLow[2][2], splitHigh[2][2];
    bool useLow = false, useHigh = false;

    float temperNow = 0.0f;     // suavizado
    float compNow   = 1.0f;

    float bufIn[2][kMaxBlock] {};
    float bufRes[2][kMaxBlock] {};

    void updateCoefficients (const TemperSettings& s) noexcept;
};
} // namespace roomx::dsp
