// =============================================================================
//  ROOM-X by Piradex — editor
//  Base lógica 1180×800 (escalável 70–150 %). Base grafite neutra; o acento
//  (anel, NoteScape, arcos, espectro, pegas) segue o Target com transição
//  suave: EARLY coral · ALL teal · LATE azul eléctrico.
// =============================================================================
#pragma once

#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"
#include "ui/Context.h"
#include "ui/SpectrumView.h"
#include "ui/NoteScape.h"
#include "ui/Panels.h"
#include "ui/SequencePanel.h"
#include "ui/TopBar.h"
#include "ui/LoginOverlay.h"

class RoomXEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RoomXEditor (RoomXProcessor&);
    ~RoomXEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;

    static constexpr int kBaseW = 1180, kBaseH = 800;
    roomx::ui::Context& uiContext() noexcept { return ctx; }   // testes / capturas

private:
    RoomXProcessor& proc;
    roomx::ui::Accent accent;
    roomx::ui::RoomXLookAndFeel lnf { accent };
    roomx::ui::WetAnalyzer analyzer;
    roomx::ui::Context ctx { proc, proc.apvts, accent, analyzer };
    juce::TooltipWindow tooltips { this, 650 };

    struct Content : public juce::Component
    {
        explicit Content (RoomXEditor& e) : owner (e) {}
        void paint (juce::Graphics&) override;
        RoomXEditor& owner;
    } content { *this };

    // ---- topo
    roomx::ui::TopBar topBar { ctx, *proc.presets };
    roomx::ui::SpaceSelector spaceSel { ctx };

    // ---- esquerda
    roomx::ui::Fader inputFader { proc.apvts, roomx::params::input, "INPUT" };
    roomx::ui::Knob preDelay { proc.apvts, roomx::params::preDelay, "PRE-DELAY" };
    roomx::ui::Knob preDiv   { proc.apvts, roomx::params::preDelayDiv, "PRE-DELAY" };
    roomx::ui::Knob mix      { proc.apvts, roomx::params::mix, "MIX" };
    roomx::ui::Knob decay    { proc.apvts, roomx::params::decay, "DECAY" };
    roomx::ui::Knob size     { proc.apvts, roomx::params::size, "SIZE" };
    roomx::ui::ParamToggle preSync { proc.apvts, roomx::params::preDelaySync, "Sync" };
    roomx::ui::ParamToggle killDry { proc.apvts, roomx::params::killDry, "Kill Dry" };
    roomx::ui::IconButton mixLock { roomx::ui::Icon::Unlock, "Bloquear Mistura: manter a Mistura ao trocar de preset", "Mix Lock: keep the Mix when changing presets" };

    // ---- centro
    roomx::ui::NoteScape noteScape { ctx };

    // ---- direita
    roomx::ui::SegmentedChoice target { proc.apvts, roomx::params::target, { "Early", "All", "Late" }, true };
    roomx::ui::Knob position { proc.apvts, roomx::params::position, "POSITION", true };
    roomx::ui::Knob density  { proc.apvts, roomx::params::density, "DENSITY" };
    roomx::ui::Knob offset   { proc.apvts, roomx::params::offset, "OFFSET", true };
    roomx::ui::Fader outputFader { proc.apvts, roomx::params::output, "OUTPUT" };
    roomx::ui::ParamToggle dualMono { proc.apvts, roomx::params::dualMono, "Dual Mono" };

    // ---- espectro + selecção de notas
    roomx::ui::SpectrumView spectrum { ctx };
    roomx::ui::SegmentedChoice noteMode { proc.apvts, roomx::params::noteMode, { "Manual", "Sequence", "MIDI" } };
    roomx::ui::IconButton notesLock { roomx::ui::Icon::Unlock, "Bloquear Notas: manter as notas/sequência ao trocar de preset", "Note Lock: keep the notes/sequence when changing presets" };
    roomx::ui::ManualPanel manualPanel { ctx };
    roomx::ui::SequencePanel sequencePanel { ctx, proc.sequence };
    roomx::ui::MidiPanel midiPanel { ctx };

    // ---- rodapé
    roomx::ui::ParamToggle bypass { proc.apvts, roomx::params::bypass, "Bypass" };

    roomx::ui::LoginOverlay login { accent };
    roomx::ui::LanguageBinder lang;      // idioma PT/EN (troca imediata)

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> mixLockAtt, notesLockAtt;
    juce::Rectangle<int> leftPanel, rightPanel, lowerPanel, footer, tabsRow;
    int tickCount = 0;
    bool wasLicensed = false;

    void layoutContent();
    void timerCallback() override;
    void updateVisibility();
    void showCallout (std::unique_ptr<juce::Component> c, juce::Component& anchor);
    void logout();
    void applyLanguage();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoomXEditor)
};
