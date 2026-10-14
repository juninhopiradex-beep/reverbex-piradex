// =============================================================================
//  ROOM-X — blocos DSP básicos (sem JUCE, sem alocação no processamento)
// =============================================================================
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace roomx::dsp
{
constexpr double kPi    = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;

inline float flushDenormal (float x) noexcept { return std::fabs (x) < 1.0e-20f ? 0.0f : x; }
inline float dbToGain (float db) noexcept     { return db <= -100.0f ? 0.0f : std::pow (10.0f, db * 0.05f); }

// Gerador pseudo-aleatório determinístico (xorshift32) — mesmo resultado em
// todas as máquinas e renders offline.
struct Rng
{
    uint32_t s;
    explicit Rng (uint32_t seed) : s (seed ? seed : 0x9E3779B9u) {}
    uint32_t next() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uniform() noexcept { return (float) (next() & 0xFFFFFF) / 16777216.0f; }          // [0,1)
    float bipolar() noexcept { return uniform() * 2.0f - 1.0f; }
};

// Suavização exponencial de um parâmetro (1 pólo)
class Smoother
{
public:
    void prepare (double sampleRate, double timeMs) noexcept
    {
        const double n = std::max (1.0, timeMs * 0.001 * sampleRate);
        coeff = (float) std::exp (-1.0 / n);
    }
    void reset (float v) noexcept   { current = target = v; }
    void setTarget (float v) noexcept { target = v; }
    float next() noexcept           { current = target + (current - target) * coeff; return current; }
    // avança N amostras de uma vez (para valores actualizados por bloco)
    float advance (int n) noexcept  { current = target + (current - target) * std::pow (coeff, (float) n); return current; }
    float get() const noexcept      { return current; }
    float getTarget() const noexcept { return target; }
    bool  isSettled() const noexcept { return std::fabs (current - target) < 1.0e-6f; }
private:
    float coeff = 0.99f, current = 0.0f, target = 0.0f;
};

// Filtro de estado variável TPT (topologia de Zavalishin/Simper):
// estável sob modulação, bom com Q alto. Devolve LP, BP (ganho 1 no centro) e HP.
struct SvfCoeffs
{
    float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0;

    void set (double sampleRate, double freqHz, double q) noexcept
    {
        freqHz = std::clamp (freqHz, 5.0, 0.49 * sampleRate);
        g  = (float) std::tan (kPi * freqHz / sampleRate);
        k  = (float) (1.0 / std::max (0.025, q));
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
};

struct SvfState
{
    float ic1 = 0, ic2 = 0;
    void reset() noexcept { ic1 = ic2 = 0; }

    inline void tick (const SvfCoeffs& c, float x, float& lp, float& bp, float& hp) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = c.a1 * ic1 + c.a2 * v3;
        const float v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = x - c.k * v1 - v2;
    }

    // banda passante normalizada (pico = 1)
    inline float bandpass (const SvfCoeffs& c, float x) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = c.a1 * ic1 + c.a2 * v3;
        const float v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return c.k * v1;
    }
    inline float lowpass (const SvfCoeffs& c, float x) noexcept
    {
        float lp, bp, hp; tick (c, x, lp, bp, hp); return lp;
    }
    inline float highpass (const SvfCoeffs& c, float x) noexcept
    {
        float lp, bp, hp; tick (c, x, lp, bp, hp); return hp;
    }
};

// Passa-baixo de 1 pólo
struct OnePole
{
    float a = 0.0f, z = 0.0f;
    void setCutoff (double sampleRate, double hz) noexcept
    {
        hz = std::clamp (hz, 1.0, 0.49 * sampleRate);
        a = (float) std::exp (-kTwoPi * hz / sampleRate);
    }
    inline float process (float x) noexcept { z = x + (z - x) * a; return z; }
    void reset() noexcept { z = 0.0f; }
};

// Linha de atraso com leitura fraccionária (Hermite de 4 pontos).
// Tamanho em potência de 2 para wrap por máscara.
class DelayLine
{
public:
    void allocate (int maxSamples)
    {
        int n = 1;
        while (n < maxSamples + 4) n <<= 1;
        buffer.assign ((size_t) n, 0.0f);
        mask = n - 1;
        write = 0;
    }
    void clear() noexcept { std::fill (buffer.begin(), buffer.end(), 0.0f); }
    int  capacity() const noexcept { return mask - 3; }

    inline void push (float x) noexcept
    {
        buffer[(size_t) write] = x;
        write = (write + 1) & mask;
    }
    // atraso inteiro (>= 1 depois do push)
    inline float readInt (int d) const noexcept
    {
        return buffer[(size_t) ((write - d) & mask)];
    }
    // atraso fraccionário >= 1
    inline float readFrac (float d) const noexcept
    {
        const int   i  = (int) d;
        const float f  = d - (float) i;
        const float xm1 = buffer[(size_t) ((write - i + 1) & mask)];
        const float x0  = buffer[(size_t) ((write - i)     & mask)];
        const float x1  = buffer[(size_t) ((write - i - 1) & mask)];
        const float x2  = buffer[(size_t) ((write - i - 2) & mask)];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

private:
    std::vector<float> buffer;
    int mask = 0, write = 0;
};

// Passa-tudo de Schroeder com atraso fixo (difusão)
class AllpassDiffuser
{
public:
    void allocate (int maxSamples) { line.allocate (maxSamples); }
    void clear() noexcept { line.clear(); }
    void setDelay (int samples) noexcept { delay = std::clamp (samples, 1, line.capacity()); }
    inline float process (float x, float g) noexcept
    {
        const float d = line.readInt (delay);
        const float v = x + g * d;
        line.push (flushDenormal (v));
        return d - g * v;
    }
private:
    DelayLine line;
    int delay = 1;
};
} // namespace roomx::dsp
