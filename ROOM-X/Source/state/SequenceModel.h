// =============================================================================
//  ROOM-X — modelo editável da sequência (ValueTree dentro do estado APVTS,
//  com undo/redo) e conversão para os dados POD do motor de áudio.
// =============================================================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../music/SequenceEngine.h"

namespace roomx::state
{
namespace ids
{
    inline const juce::Identifier sequence { "SEQUENCE" };
    inline const juce::Identifier row      { "ROW" };
    inline const juce::Identifier region   { "REGION" };
    inline const juce::Identifier root     { "root" };
    inline const juce::Identifier cat      { "cat" };
    inline const juce::Identifier type     { "type" };
    inline const juce::Identifier rowIdx   { "row" };
    inline const juce::Identifier start    { "start" };
    inline const juce::Identifier length   { "len" };
    inline const juce::Identifier selected { "selectedRow" };
}

class SequenceModel
{
public:
    explicit SequenceModel (juce::AudioProcessorValueTreeState& s) : apvts (s) {}

    juce::ValueTree tree();                     // cria com valores por defeito se faltar

    music::Harmony getRow (int i);
    void setRow (int i, const music::Harmony& h);

    int  getNumRegions();
    music::SeqRegion getRegion (int i);
    int  addRegion (const music::SeqRegion& r);
    void setRegion (int i, const music::SeqRegion& r);
    void removeRegions (juce::Array<int> indices);
    void clearRegions();

    int  getSelectedRow();
    void setSelectedRow (int r);

    music::SequenceData compile();
    void replaceWith (const juce::ValueTree& seq);   // de um preset (undoable)

    static juce::ValueTree toTree (const music::SequenceData& d);
    static music::SequenceData fromTree (const juce::ValueTree& t);

    juce::UndoManager* undo() const { return apvts.undoManager; }

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::ValueTree rowTree (int i);
};
} // namespace roomx::state
