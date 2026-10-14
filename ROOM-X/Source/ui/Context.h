// =============================================================================
//  ROOM-X — contexto partilhado pelos componentes da UI
// =============================================================================
#pragma once

#include "Widgets.h"
#include "../music/MusicTheory.h"

class RoomXProcessor;

namespace roomx::ui
{
class WetAnalyzer;

// Acesso simples a um parâmetro em unidades reais, com gestos para o host/undo
struct ParamRef
{
    juce::RangedAudioParameter* p = nullptr;

    ParamRef() = default;
    ParamRef (APVTS& s, const char* id) : p (s.getParameter (id)) { jassert (p != nullptr); }

    float get() const               { return p->convertFrom0to1 (p->getValue()); }
    float defaultValue() const      { return p->convertFrom0to1 (p->getDefaultValue()); }
    float clamp (float v) const     { return p->getNormalisableRange().snapToLegalValue (v); }
    void  begin()                   { p->beginChangeGesture(); }
    void  set (float v)             { p->setValueNotifyingHost (p->convertTo0to1 (clamp (v))); }
    void  end()                     { p->endChangeGesture(); }
    void  setOnce (float v)         { begin(); set (v); end(); }
    juce::String text() const       { return p->getCurrentValueAsText(); }
};

struct Context
{
    RoomXProcessor& proc;
    APVTS& apvts;
    Accent& accent;
    WetAnalyzer& analyzer;

    bool useFlats() const;
    music::Tuning tuning() const;
    uint16_t displayedPitchClasses() const;     // notas activas (Manual / Sequence / MIDI)
    int  noteMode() const;
    juce::UndoManager& undo() const;
};

// Pinta um painel base com cantos suaves
inline void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner = 10.0f)
{
    g.setColour (col::panel);
    g.fillRoundedRectangle (r, corner);
    g.setColour (col::line);
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
}
} // namespace roomx::ui
