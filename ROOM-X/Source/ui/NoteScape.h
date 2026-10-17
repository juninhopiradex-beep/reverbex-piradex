// =============================================================================
//  ROOM-X — NoteScape: visualizador original de energia por classe de altura
//
//  "Rosa radial segmentada": 12 sectores (C no topo, sentido horário), cada
//  um com segmentos em arco que acendem conforme a energia do wet nessa
//  classe de altura, uma linha de contorno suave e marcadores prateados nas
//  notas seleccionadas. Temper > 0 faz crescer os sectores seleccionados;
//  Temper < 0 abre "buracos" (sectores ocos). Clicar numa nota (modo Manual)
//  liga/desliga-a. No centro: TemperDial (Temper + Note Width).
// =============================================================================
#pragma once

#include "Context.h"

namespace roomx::ui
{
class TemperDial : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit TemperDial (Context& c);
    juce::String getTooltip() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    bool hitTest (int x, int y) override;

    bool isDragging() const { return zone != Zone::None; }

private:
    Context& ctx;
    ParamRef temper, width;
    enum class Zone { None, Temper, Width };
    Zone zone = Zone::None, hover = Zone::None;
    float startValue = 0.0f;
    Zone zoneAt (juce::Point<float> p) const;
};

class NoteScape : public juce::Component
{
public:
    explicit NoteScape (Context& c);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    void tick();     // ~30 Hz: anima a energia exibida

    TemperDial dial;

private:
    Context& ctx;
    ParamRef temper, width, notes, mode;
    float shown[12] {};          // energia exibida (suavizada)
    float ghost = 0.0f;          // realce temporário enquanto se arrasta Temper
    int hoverNote = -1;
    bool paintValue = true;      // arrasto a pintar notas: liga ou desliga
    int lastPainted = -1;

    juce::Point<float> centre;
    float radius = 100.0f;

    int noteAt (juce::Point<float> p) const;
    void toggleNote (int pc, bool on);
};
} // namespace roomx::ui
