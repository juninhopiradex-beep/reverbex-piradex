// =============================================================================
//  ROOM-X — painel do sequenciador harmónico
//
//  8 linhas (harmonias) × 1–64 compassos. Clique/arrasto em zona vazia cria
//  uma região; arrastar move (Alt = duplicar); bordas redimensionam;
//  clique direito = menu; Delete apaga; Ctrl+roda = zoom; navegador em baixo.
//  Regiões nunca se sobrepõem no tempo (só uma harmonia de cada vez).
// =============================================================================
#pragma once

#include "Context.h"
#include "../state/SequenceModel.h"

namespace roomx::ui
{
class SequencePanel : public juce::Component
{
public:
    SequencePanel (Context& c, state::SequenceModel& model);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void tick();

private:
    Context& ctx;
    state::SequenceModel& seq;
    ParamRef bars, grid, transposeP, offset, armed;

    IconButton barsDown { Icon::Minus, "Menos compassos", "Fewer bars" }, barsUp { Icon::Plus, "Mais compassos", "More bars" };
    IconButton trDown { Icon::Minus, "Transpor -1", "Transpose -1" }, trUp { Icon::Plus, "Transpor +1", "Transpose +1" };
    IconButton retrig { Icon::Retrigger, "Retrigger: recomeçar a sequência agora", "Retrigger: restart the sequence now" };
    IconButton arm { Icon::Play, "Seguir o transporte do DAW (armado)", "Follow the DAW transport (armed)" };
    juce::TextButton gridBtn;
    std::unique_ptr<APVTS::ButtonAttachment> armAtt;

    juce::Rectangle<float> labels, gridArea, ruler, nav, side, offsetBox;
    double viewStart = 0.0, viewLen = 16.0;
    std::vector<music::SeqRegion> regs;
    int selected = -1;

    enum class Mode { None, Create, Move, ResizeL, ResizeR, Nav, NavL, NavR, Offset };
    Mode mode = Mode::None;
    music::SeqRegion dragOrig;
    double dragAnchorBeat = 0.0, navStartView = 0.0, navStartLen = 0.0;
    float offsetStart = 0.0f;
    bool duplicating = false;

    double totalBeats() const;
    double gridBeats() const;
    double snap (double beats) const;
    float  xForBeat (double b) const;
    double beatForX (float x) const;
    int    rowForY (float y) const;
    juce::Rectangle<float> regionRect (const music::SeqRegion&) const;
    juce::Rectangle<float> rowRect (int r) const;
    int    regionAt (juce::Point<float>) const;

    void reload();
    void writeAll (std::vector<music::SeqRegion> list, int keepIndex);
    void clampView();
    void showRowBrowser (int row);
    void showRegionMenu (int index);
};
} // namespace roomx::ui
