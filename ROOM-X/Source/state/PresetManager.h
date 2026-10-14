// =============================================================================
//  ROOM-X — presets (factory + tutorial + utilizador), A/B e estado "sujo"
//
//  Formato: XML  <ROOMX_PRESET name bank version>  <PARAM id value/>...  <SEQUENCE>
//  Extensão .roomx. Os factory presets vivem no binário (FactoryPresets.cpp).
//  Pasta do utilizador:
//    macOS   ~/Music/Piradex/ROOM-X/User Presets
//    Windows Documentos\Piradex\ROOM-X\User Presets
// =============================================================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "SequenceModel.h"

namespace roomx::state
{
namespace pids
{
    inline const juce::Identifier preset  { "ROOMX_PRESET" };
    inline const juce::Identifier param   { "PARAM" };
    inline const juce::Identifier id      { "id" };
    inline const juce::Identifier value   { "value" };
    inline const juce::Identifier name    { "name" };
    inline const juce::Identifier bank    { "bank" };
    inline const juce::Identifier version { "version" };
    inline const juce::Identifier info    { "info" };
}

constexpr int kPresetSchemaVersion = 1;

// Factory presets (gerados em código)
juce::Array<juce::ValueTree> makeFactoryPresets();

class PresetManager
{
public:
    struct Entry
    {
        juce::String name, bank, info;
        bool factory = true;
        int factoryIndex = -1;
        juce::File file;
    };

    PresetManager (juce::AudioProcessorValueTreeState& s, SequenceModel& seq);

    // ---- catálogo
    void refresh();                                    // relê a pasta do utilizador
    const juce::Array<Entry>& entries() const { return list; }
    int  currentIndex() const { return current; }
    juce::String currentName() const { return currentPresetName; }
    juce::String currentInfo() const;

    bool load (int index);
    void loadNext();
    void loadPrevious();
    bool loadFile (const juce::File& f);
    bool importFile (const juce::File& f);              // copia para a pasta do utilizador
    juce::File saveUser (const juce::String& name);     // devolve o ficheiro gravado
    bool saveCurrent();                                 // grava por cima (só presets do utilizador)

    bool isDirty();                                     // comparação com o preset carregado

    // ---- A/B
    int  activeSlot() const { return abSlot; }
    void selectSlot (int slot);
    void copyAtoB();
    void copyBtoA();

    // ---- estado
    juce::ValueTree capture (const juce::String& name, const juce::String& bank) const;
    void apply (const juce::ValueTree& preset, bool respectLocks);
    void restoreNameFromState();                        // depois de setStateInformation

    static juce::File userFolder();

private:
    juce::AudioProcessorValueTreeState& apvts;
    SequenceModel& sequence;
    juce::Array<juce::ValueTree> factory;
    juce::Array<Entry> list;
    int current = -1;
    juce::String currentPresetName { "Init" };
    juce::ValueTree loadedSnapshot;
    juce::ValueTree abState[2];
    int abSlot = 0;

    bool same (const juce::ValueTree& a, const juce::ValueTree& b) const;
    void setLoaded (const juce::String& name, int index);
};
} // namespace roomx::state
