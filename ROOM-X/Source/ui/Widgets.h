// =============================================================================
//  ROOM-X — componentes base da UI (knobs, faders com medidores, ícones,
//  selector segmentado, botões ligados a parâmetros)
// =============================================================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

namespace roomx::ui
{
using APVTS = juce::AudioProcessorValueTreeState;

// ---------------------------------------------------------------------------
// Textos que dependem do idioma: bind() aplica já e volta a aplicar a cada troca
class LanguageBinder : private piradex::i18n::Listener
{
public:
    LanguageBinder()  { piradex::i18n::addListener (this); }
    ~LanguageBinder() override { piradex::i18n::removeListener (this); }
    void bind (std::function<void()> f) { f(); fns.push_back (std::move (f)); }
    std::function<void()> onChange;          // ex.: repaint() do dono
private:
    std::vector<std::function<void()>> fns;
    void languageChanged() override { for (auto& f : fns) f(); if (onChange) onChange(); }
};

// ---------------------------------------------------------------------------
class Knob : public juce::Component
{
public:
    Knob (APVTS& s, const juce::String& paramId, const juce::String& title, bool bipolar = false);
    void resized() override;
    void paint (juce::Graphics&) override;
    void setValueText (std::function<juce::String (double)> f) { valueText = std::move (f); repaint(); }

    juce::Slider slider;
    juce::Label  valueLabel;
    juce::String title;
    int titleHeight = 20, valueHeight = 22;

private:
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
    std::function<juce::String (double)> valueText;
    LanguageBinder lang;
};

// ---------------------------------------------------------------------------
class Fader : public juce::Component
{
public:
    Fader (APVTS& s, const juce::String& paramId, const juce::String& title);
    void resized() override;
    void paint (juce::Graphics&) override;
    void setLevels (float l, float r);   // picos lineares (chamar a ~30 Hz)
    void setTitle (const juce::String& t) { title = t; repaint(); }

    juce::Slider slider;

private:
    juce::String title;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
    float level[2] {}, hold[2] {};
    int holdTicks[2] {}, clip[2] {};
    juce::Rectangle<float> meterArea;
};

// ---------------------------------------------------------------------------
enum class Icon { Undo, Redo, Prev, Next, Save, Info, Leaf, Lock, Unlock, Fork, Retrigger, Play, Minus, Plus, Menu, Power, Copy, Folder };

class IconButton : public juce::Button
{
public:
    IconButton (Icon i, const juce::String& tooltip = {});
    IconButton (Icon i, const char* pt, const char* en) : IconButton (i) { setTip (pt, en); }
    void setTip (const char* pt, const char* en) { tipPt = pt; tipEn = en; }
    juce::String getTooltip() override { return tipPt != nullptr ? tr (tipPt, tipEn) : juce::Button::getTooltip(); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
    void setIcon (Icon i) { icon = i; repaint(); }
    static void drawIcon (juce::Graphics&, Icon, juce::Rectangle<float>, juce::Colour, float stroke = 1.6f);

    bool showDot = false;                  // ponto laranja (ex.: afinação alterada)
    bool drawBackground = true;
    juce::Colour onColour;                 // cor quando "toggled" (transparente = acento)
    const Accent* accent = nullptr;

private:
    Icon icon;
    const char* tipPt = nullptr;
    const char* tipEn = nullptr;
};

// ---------------------------------------------------------------------------
// Botão de texto ligado a um parâmetro booleano
class ParamToggle : public juce::TextButton
{
public:
    ParamToggle (APVTS& s, const juce::String& paramId, const juce::String& text);
private:
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

// ---------------------------------------------------------------------------
// Selector segmentado ligado a um parâmetro de escolha (Target, Note Mode...)
class SegmentedChoice : public juce::Component
{
public:
    SegmentedChoice (APVTS& s, const juce::String& paramId, juce::StringArray labels, bool vertical = false);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    int  getIndex() const { return index; }
    void setLabels (juce::StringArray l) { labels = std::move (l); repaint(); }
    std::function<juce::Colour (int)> colourForIndex;   // cor própria por segmento (Target)
    std::function<void (int)> onChange;
    juce::String caption;

private:
    juce::StringArray labels;
    bool vertical;
    int index = -1, hover = -1;
    juce::RangedAudioParameter* param;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::Rectangle<float> segment (int i) const;
};

// ---------------------------------------------------------------------------
// Selector de idioma "PT | EN" (troca imediata em todas as janelas abertas)
class LanguageSwitch : public juce::Component, public juce::SettableTooltipClient, private piradex::i18n::Listener
{
public:
    explicit LanguageSwitch (const Accent* a = nullptr);
    ~LanguageSwitch() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    const Accent* accent;
    int hover = -1;
    juce::Rectangle<float> half (int i) const;
    void languageChanged() override { repaint(); }
};
} // namespace roomx::ui
