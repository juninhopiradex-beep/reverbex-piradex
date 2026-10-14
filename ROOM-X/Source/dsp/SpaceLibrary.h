// =============================================================================
//  ROOM-X — biblioteca de espaços originais (29 espaços, 5 categorias)
//
//  Cada espaço é uma receita para o motor híbrido (reflexões iniciais +
//  FDN tardio). Nada aqui vem de IRs ou dados de terceiros: os valores são
//  escolhas de design próprias e os padrões de reflexão são gerados de forma
//  determinística a partir de uma "seed" por espaço.
// =============================================================================
#pragma once

namespace roomx::dsp
{
enum class SpaceCategory { Rooms, Halls, Mechanical, Unusual, SpecialFX };

struct SpaceDef
{
    const char*   name;
    SpaceCategory category;
    const char*   description;

    // ---- Tempo de reverberação base (s, a Size 100 % e Decay 1x)
    float rt60;
    float hfRatio;        // RT60 agudos / graves  (< 1 = escuro)
    float dampHz;         // frequência de transição dos agudos
    float lfRatio;        // RT60 graves extra (> 1 = mais corpo)
    float lfHz;

    // ---- Reflexões iniciais
    float erLengthMs;     // janela das reflexões
    float erStartMs;      // primeira reflexão
    int   erTaps;         // por canal (máx. 32)
    float erGain;
    float erShape;        // expoente da distribuição temporal (<1 = concentrado no início)
    float erSlope;        // decaimento de ganho ao longo da janela (negativo = cresce: "swell")
    float erDampHz;
    float erDiffusion;    // 0..0.75
    float erWidth;        // 0..1 decorrelação L/R

    // ---- Cauda tardia (FDN de 16 linhas)
    float lateMinMs;      // linha mais curta (a Size 100 %)
    float lateMaxMs;      // linha mais longa
    float lateGain;
    float diffusion;      // difusores de entrada 0..0.8
    float modDepthMs;
    float modRateHz;
    float dispersion;     // 0..1 cadeia de passa-tudo dispersiva (molas / metal)
    float lateOnsetMs;    // atraso extra da cauda em relação às reflexões
    float width;          // 0..1

    unsigned seed;
};

constexpr int kNumSpaces = 29;
const SpaceDef& space (int index) noexcept;
const char*     categoryName (SpaceCategory c) noexcept;
} // namespace roomx::dsp
