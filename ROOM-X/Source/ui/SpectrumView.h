// =============================================================================
//  ROOM-X — análise do sinal wet (thread da UI) e vista de espectro
//
//  WetAnalyzer: lê o FIFO do processador, FFT de 8192 pontos (Hann) a ~30 Hz,
//  curva log 20 Hz–20 kHz suavizada e energia por classe de altura (para o
//  NoteScape). Nada disto corre na thread de áudio.
//
//  SpectrumView: espectro wet + Temper Range (pegas com nome da nota) +
//  low-cut / high-cut (pegas na base, arrasto central desloca a banda,
//  duplo clique repõe, Shift = ajuste fino).
// =============================================================================
#pragma once

#include <juce_dsp/juce_dsp.h>
#include "Context.h"

namespace roomx::ui
{
class WetAnalyzer
{
public:
    static constexpr int kOrder = 13, kSize = 1 << kOrder;
    static constexpr int kPoints = 320;          // pontos da curva log

    WetAnalyzer();
    // Chamar a partir de um Timer da UI; devolve true se houve dados novos
    bool update (RoomXProcessor& p, float rangeLowNote, float rangeHighNote, const music::Tuning& t);

    const float* curve() const noexcept   { return display; }      // dB por ponto
    const float* pitchEnergy() const noexcept { return pcEnergy; }  // 0..1 por classe
    float level() const noexcept          { return overall; }       // 0..1
    static float pointToHz (int i) noexcept { return 20.0f * std::pow (1000.0f, (float) i / (float) (kPoints - 1)); }
    bool enabled = true;
    bool eco = false;

private:
    juce::dsp::FFT fft { kOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) kSize, juce::dsp::WindowingFunction<float>::hann, true };
    std::vector<float> ring = std::vector<float> ((size_t) kSize, 0.0f);
    std::vector<float> work = std::vector<float> ((size_t) kSize * 2, 0.0f);
    int ringPos = 0, newSamples = 0, skip = 0;
    double rate = 48000.0;
    float display[kPoints] {};
    float pcEnergy[12] {};
    float overall = 0.0f;
};

// ---------------------------------------------------------------------------
class SpectrumView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit SpectrumView (Context& c);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    Context& ctx;
    ParamRef rangeLow, rangeHigh, lowCut, highCut, lowSlope, highSlope, temper, noteWidth;

    enum class Drag { None, RangeLow, RangeHigh, RangeBoth, LowCut, HighCut, CutBoth };
    Drag drag = Drag::None, hoverPart = Drag::None;
    juce::Point<float> dragStart;
    float startA = 0, startB = 0;
    float lastX = 0;

    juce::Rectangle<float> plot, lowBtn, highBtn;

    float xForHz (float hz) const;
    float hzForX (float x) const;
    float xForNote (float note) const;
    float noteForX (float x) const;
    float yForDb (float db) const;
    Drag partAt (juce::Point<float> p) const;
    void drawFilterButton (juce::Graphics&, juce::Rectangle<float>, bool high);
    juce::String describe (Drag d) const;
};
} // namespace roomx::ui
