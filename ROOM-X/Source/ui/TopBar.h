// =============================================================================
//  ROOM-X — barra de topo (marca, undo/redo, presets, A/B, info, Eco)
//  e selector de espaço (29 espaços em 5 categorias, RT60 efectivo)
// =============================================================================
#pragma once

#include "Context.h"
#include "../state/PresetManager.h"

namespace roomx::ui
{
juce::String bankLabel (const juce::String& bank);   // nome do banco no idioma actual
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
    IconButton tuning { Icon::Fork, "Afinação global (A4 / cents / sustenidos-bemóis)", "Global tuning (A4 / cents / sharps-flats)" };

private:
    Context& ctx;
    state::PresetManager& presets;
    IconButton undo { Icon::Undo, "Desfazer (Ctrl/Cmd+Z)", "Undo (Ctrl/Cmd+Z)" }, redo { Icon::Redo, "Refazer (Ctrl/Cmd+Shift+Z)", "Redo (Ctrl/Cmd+Shift+Z)" };
    IconButton prev { Icon::Prev, "Preset anterior", "Previous preset" }, next { Icon::Next, "Próximo preset", "Next preset" };
    IconButton save { Icon::Save, "Guardar / importar presets", "Save / import presets" };
    IconButton info { Icon::Info, "Informação, licença e definições (idioma)", "Info, license and settings (language)" };
    IconButton eco  { Icon::Leaf, "Modo Eco: menos CPU (cauda de 8 linhas, analisador mais lento)", "Eco mode: less CPU (8-line tail, slower analyzer)" };
    juce::TextButton slotA { "A" }, slotB { "B" };
    IconButton copyAB { Icon::Copy, "Copiar A para B (ou B para A)", "Copy A to B (or B to A)" };
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
    IconButton prev { Icon::Prev, "Espaço anterior", "Previous space" }, next { Icon::Next, "Próximo espaço", "Next space" };
    bool hot = false;
    juce::Rectangle<float> box;
};
} // namespace roomx::ui
