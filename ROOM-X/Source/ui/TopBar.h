// =============================================================================
//  ROOM-X — barra de topo (marca, undo/redo, presets, A/B, info, Eco)
//  e selector de espaço (29 espaços em 5 categorias, RT60 efectivo)
// =============================================================================
#pragma once

#include "Context.h"
#include "../state/PresetManager.h"

namespace roomx::ui
{
void drawRoomXLogo (juce::Graphics&, juce::Rectangle<float> area, juce::Colour accent, float height, bool withByline);

class TopBar : public juce::Component
{
public:
    TopBar (Context& c, state::PresetManager& pm);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void tick();

    std::function<void()> onInfo, onTuning;
    IconButton tuning { Icon::Fork, U ("Afinação global (A4 / cents / sustenidos-bemóis)") };

private:
    Context& ctx;
    state::PresetManager& presets;
    IconButton undo { Icon::Undo, U ("Desfazer (Ctrl/Cmd+Z)") }, redo { Icon::Redo, U ("Refazer (Ctrl/Cmd+Shift+Z)") };
    IconButton prev { Icon::Prev, "Preset anterior" }, next { Icon::Next, U ("Próximo preset") };
    IconButton save { Icon::Save, U ("Guardar / importar presets") };
    IconButton info { Icon::Info, U ("Informação, licença e definições") };
    IconButton eco  { Icon::Leaf, U ("Modo Eco: menos CPU (cauda de 8 linhas, analisador mais lento)") };
    juce::TextButton slotA { "A" }, slotB { "B" };
    IconButton copyAB { Icon::Copy, U ("Copiar A para B (ou B para A)") };
    std::unique_ptr<APVTS::ButtonAttachment> ecoAtt;
    juce::Rectangle<float> presetBox, logoArea;
    bool dirty = false;
    juce::String shownName;

    void showPresetMenu();
    void showSaveMenu();
    void saveAs();
};

// ---------------------------------------------------------------------------
class SpaceSelector : public juce::Component
{
public:
    explicit SpaceSelector (Context& c);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { hot = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hot = false; repaint(); }

private:
    Context& ctx;
    ParamRef space, decay, size;
    IconButton prev { Icon::Prev, "Espaço anterior" }, next { Icon::Next, U ("Próximo espaço") };
    bool hot = false;
    juce::Rectangle<float> box;
};
} // namespace roomx::ui
