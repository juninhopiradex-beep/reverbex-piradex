// =============================================================================
//  ROOM-X — parâmetros (IDs imutáveis depois do lançamento!)
// =============================================================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace roomx::params
{
// ---- IDs (NUNCA mudar depois de publicado: as sessões dos DAWs dependem deles)
inline constexpr const char* input        = "input";
inline constexpr const char* output       = "output";
inline constexpr const char* mix          = "mix";
inline constexpr const char* killDry      = "killDry";
inline constexpr const char* preDelay     = "preDelay";
inline constexpr const char* preDelaySync = "preDelaySync";
inline constexpr const char* preDelayDiv  = "preDelayDiv";
inline constexpr const char* decay        = "decay";
inline constexpr const char* size         = "size";
inline constexpr const char* position     = "position";
inline constexpr const char* density      = "density";
inline constexpr const char* offset       = "offset";
inline constexpr const char* temper       = "temper";
inline constexpr const char* noteWidth    = "noteWidth";
inline constexpr const char* target       = "target";
inline constexpr const char* rangeLow     = "rangeLow";
inline constexpr const char* rangeHigh    = "rangeHigh";
inline constexpr const char* lowCut       = "lowCut";
inline constexpr const char* lowCutSlope  = "lowCutSlope";
inline constexpr const char* highCut      = "highCut";
inline constexpr const char* highCutSlope = "highCutSlope";
inline constexpr const char* space        = "space";
inline constexpr const char* dualMono     = "dualMono";
inline constexpr const char* bypass       = "bypass";
inline constexpr const char* eco          = "eco";
inline constexpr const char* noteMode     = "noteMode";
inline constexpr const char* notes        = "notes";
inline constexpr const char* midiOctave   = "midiOctave";
inline constexpr const char* midiRelease  = "midiRelease";
inline constexpr const char* seqBars      = "seqBars";
inline constexpr const char* seqGrid      = "seqGrid";
inline constexpr const char* seqTranspose = "seqTranspose";
inline constexpr const char* seqArmed     = "seqArmed";
inline constexpr const char* seqOffset    = "seqOffset";
inline constexpr const char* tuneRef      = "tuneRef";
inline constexpr const char* tuneCents    = "tuneCents";
inline constexpr const char* mixLock      = "mixLock";
inline constexpr const char* notesLock    = "notesLock";

enum class NoteMode : int { Manual = 0, Sequence = 1, Midi = 2 };

// Divisões rítmicas do pre-delay sincronizado (em semínimas)
struct NoteDivision { const char* name; double beats; };
const juce::Array<NoteDivision>& preDelayDivisions();
// Grelha do sequenciador (em semínimas)
const juce::Array<NoteDivision>& gridDivisions();

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

// Parâmetros que pertencem à instância e não aos presets
bool isInstanceOnly (const juce::String& id);

// Textos de apresentação partilhados com a UI
juce::String positionToText (float v);
juce::String beatsToBarBeat (double beats);
juce::String formatHz (float hz);
juce::String formatMs (float ms);
} // namespace roomx::params
