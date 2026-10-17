// =============================================================================
//  ROOM-X — painéis inferiores (Manual, MIDI) e janelas flutuantes
//  (navegador de escalas, afinação, informação/licença)
// =============================================================================
#pragma once

#include "Context.h"

namespace roomx::ui
{
// Nome da harmonia que corresponde exactamente a uma máscara (ex.: "A min", "D Dorian")
juce::String describeMask (uint16_t mask, bool flats);
// Etiqueta / nome longo de uma harmonia no idioma actual (acordes ficam em cifra)
juce::String harmonyLabel (const music::Harmony& h, bool flats);
juce::String harmonyLongName (music::Category c, int index);

// ---------------------------------------------------------------------------
class NotePads : public juce::Component
{
public:
    explicit NotePads (Context& c);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    Context& ctx;
    ParamRef notes;
    bool paintOn = true;
    int last = -1;
    juce::Rectangle<float> padFor (int pc) const;
    int padAt (juce::Point<float>) const;
};

// ---------------------------------------------------------------------------
class ScaleBrowser : public juce::Component, private juce::ScrollBar::Listener
{
public:
    ScaleBrowser (Context& c, std::function<void (const music::Harmony&)> onPick, music::Harmony initial);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    Context& ctx;
    std::function<void (const music::Harmony&)> pick;
    music::Harmony h;
    int hoverType = -1, hoverRoot = -1, hoverCat = -1;
    juce::Rectangle<float> catArea, rootArea, typeArea;
    juce::ScrollBar scroll { true };
    double firstRow = 0.0;
    static constexpr float kRowH = 24.0f;
    juce::Rectangle<float> catRect (int i) const;
    juce::Rectangle<float> rootRect (int i) const;
    juce::Rectangle<float> typeRect (int i) const;
    int visibleRows() const;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override { firstRow = newRangeStart; repaint(); }
};

// ---------------------------------------------------------------------------
class ManualPanel : public juce::Component
{
public:
    explicit ManualPanel (Context& c);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    Context& ctx;
    ParamRef notes;
    NotePads pads;
    juce::TextButton choose, clear, all, invert;
    IconButton down { Icon::Minus, "Transpor -1 semitom", "Transpose -1 semitone" }, up { Icon::Plus, "Transpor +1 semitom", "Transpose +1 semitone" };
    LanguageBinder lang;
    music::Harmony lastPicked { 0, music::Category::Scale, 0 };
    void transpose (int semis);
};

// ---------------------------------------------------------------------------
class MidiPanel : public juce::Component
{
public:
    explicit MidiPanel (Context& c);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();

private:
    Context& ctx;
    ParamToggle octave;
    Knob release;
    LanguageBinder lang;
    int lastActivity = 0;
    float led = 0.0f;
    juce::Rectangle<float> keysArea, ledArea;
};

// ---------------------------------------------------------------------------
class TuningPanel : public juce::Component
{
public:
    explicit TuningPanel (Context& c);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    Context& ctx;
    Knob ref, cents;
    juce::TextButton reset, sharps { "C# D# F#" }, flats { "Db Eb Gb" };
    LanguageBinder lang;
};

// ---------------------------------------------------------------------------
class InfoPanel : public juce::Component
{
public:
    InfoPanel (Context& c, std::function<void()> onLogout);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    Context& ctx;
    std::function<void()> logout;
    juce::TextButton logoutBtn;
    juce::ToggleButton analyzer;
    LanguageSwitch language { &ctx.accent };
    LanguageBinder lang;
    juce::Rectangle<int> languageRow;
};
} // namespace roomx::ui
