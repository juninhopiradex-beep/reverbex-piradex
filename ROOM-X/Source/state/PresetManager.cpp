#include "PresetManager.h"
#include "Parameters.h"

namespace roomx::state
{
static const juce::Identifier kStateName { "presetName" };

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s, SequenceModel& seq)
    : apvts (s), sequence (seq)
{
    factory = makeFactoryPresets();
    refresh();
    loadedSnapshot = capture ("Init", "");
    abState[0] = loadedSnapshot.createCopy();
    abState[1] = loadedSnapshot.createCopy();
}

juce::File PresetManager::userFolder()
{
   #if JUCE_MAC
    auto base = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
   #else
    auto base = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
   #endif
    return base.getChildFile ("Piradex").getChildFile ("ROOM-X").getChildFile ("User Presets");
}

void PresetManager::refresh()
{
    const auto keepName = currentPresetName;
    list.clear();
    for (int i = 0; i < factory.size(); ++i)
    {
        Entry e;
        e.name = factory[i].getProperty (pids::name).toString();
        e.bank = factory[i].getProperty (pids::bank).toString();
        e.info = factory[i].getProperty (pids::info).toString();
        e.factory = true;
        e.factoryIndex = i;
        list.add (e);
    }

    auto folder = userFolder();
    if (folder.isDirectory())
    {
        auto files = folder.findChildFiles (juce::File::findFiles, true, "*.roomx");
        files.sort();
        for (auto& f : files)
        {
            Entry e;
            e.name = f.getFileNameWithoutExtension();
            const auto rel = f.getParentDirectory().getRelativePathFrom (folder);
            e.bank = rel == "." || rel.isEmpty() ? juce::String ("User") : "User/" + rel;
            e.factory = false;
            e.file = f;
            list.add (e);
        }
    }

    current = -1;
    for (int i = 0; i < list.size(); ++i)
        if (list[i].name == keepName) { current = i; break; }
}

juce::String PresetManager::currentInfo() const
{
    return juce::isPositiveAndBelow (current, list.size()) ? list[current].info : juce::String();
}

juce::ValueTree PresetManager::capture (const juce::String& name, const juce::String& bank) const
{
    juce::ValueTree p (pids::preset);
    p.setProperty (pids::name, name, nullptr);
    p.setProperty (pids::bank, bank, nullptr);
    p.setProperty (pids::version, kPresetSchemaVersion, nullptr);

    for (auto* param : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            if (params::isInstanceOnly (rp->paramID)) continue;
            juce::ValueTree v (pids::param);
            v.setProperty (pids::id, rp->paramID, nullptr);
            v.setProperty (pids::value, rp->convertFrom0to1 (rp->getValue()), nullptr);
            p.appendChild (v, nullptr);
        }

    p.appendChild (sequence.tree().createCopy(), nullptr);
    return p;
}

void PresetManager::apply (const juce::ValueTree& preset, bool respectLocks)
{
    if (! preset.hasType (pids::preset)) return;

    const bool mixLocked   = respectLocks && apvts.getRawParameterValue (params::mixLock)->load() > 0.5f;
    const bool notesLocked = respectLocks && apvts.getRawParameterValue (params::notesLock)->load() > 0.5f;

    auto isNoteParam = [] (const juce::String& id)
    {
        return id == params::notes || id == params::noteMode || id == params::seqBars || id == params::seqGrid
            || id == params::seqTranspose || id == params::seqOffset || id == params::midiOctave;
    };

    if (auto* um = apvts.undoManager) um->beginNewTransaction ("Load preset");

    for (auto* param : apvts.processor.getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param);
        if (rp == nullptr) continue;
        const auto& id = rp->paramID;
        if (params::isInstanceOnly (id)) continue;
        if (mixLocked && (id == params::mix || id == params::killDry)) continue;
        if (notesLocked && isNoteParam (id)) continue;

        float value = rp->convertFrom0to1 (rp->getDefaultValue());
        auto v = preset.getChildWithProperty (pids::id, id);
        if (v.isValid()) value = (float) v.getProperty (pids::value);

        const float norm = rp->convertTo0to1 (value);
        if (std::abs (norm - rp->getValue()) > 1.0e-7f)
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (norm);
            rp->endChangeGesture();
        }
    }

    if (! notesLocked)
    {
        auto seq = preset.getChildWithName (ids::sequence);
        sequence.replaceWith (seq.isValid() ? seq : SequenceModel::toTree (music::SequenceData::makeDefault()));
    }
}

void PresetManager::setLoaded (const juce::String& name, int index)
{
    currentPresetName = name;
    current = index;
    apvts.state.setProperty (kStateName, name, nullptr);
    loadedSnapshot = capture (name, "");
}

bool PresetManager::load (int index)
{
    if (! juce::isPositiveAndBelow (index, list.size())) return false;
    const auto& e = list.getReference (index);
    juce::ValueTree p;
    if (e.factory)
        p = factory[e.factoryIndex];
    else if (auto xml = juce::XmlDocument::parse (e.file))
        p = juce::ValueTree::fromXml (*xml);

    if (! p.isValid()) return false;
    apply (p, true);
    setLoaded (e.name, index);
    return true;
}

void PresetManager::loadNext()
{
    if (list.isEmpty()) return;
    load (current < 0 ? 0 : (current + 1) % list.size());
}

void PresetManager::loadPrevious()
{
    if (list.isEmpty()) return;
    load (current <= 0 ? list.size() - 1 : current - 1);
}

bool PresetManager::loadFile (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr) return false;
    auto p = juce::ValueTree::fromXml (*xml);
    if (! p.hasType (pids::preset)) return false;
    apply (p, true);
    refresh();
    int idx = -1;
    for (int i = 0; i < list.size(); ++i)
        if (list[i].file == f) idx = i;
    setLoaded (f.getFileNameWithoutExtension(), idx);
    return true;
}

bool PresetManager::importFile (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr || ! juce::ValueTree::fromXml (*xml).hasType (pids::preset)) return false;
    auto dest = userFolder().getChildFile (f.getFileName());
    dest.getParentDirectory().createDirectory();
    if (dest.exists()) dest = dest.getNonexistentSibling();
    if (! f.copyFileTo (dest)) return false;
    return loadFile (dest);
}

juce::File PresetManager::saveUser (const juce::String& rawName)
{
    auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty()) name = "My Preset";
    auto folder = userFolder();
    folder.createDirectory();
    auto f = folder.getChildFile (name + ".roomx");
    auto p = capture (name, "User");
    if (auto xml = p.createXml())
        if (xml->writeTo (f))
        {
            refresh();
            int idx = -1;
            for (int i = 0; i < list.size(); ++i)
                if (list[i].file == f) idx = i;
            setLoaded (name, idx);
            return f;
        }
    return {};
}

bool PresetManager::saveCurrent()
{
    if (juce::isPositiveAndBelow (current, list.size()) && ! list[current].factory)
        return saveUser (list[current].name).existsAsFile();
    return false;
}

bool PresetManager::same (const juce::ValueTree& a, const juce::ValueTree& b) const
{
    for (const auto& pa : a)
    {
        if (! pa.hasType (pids::param)) continue;
        auto pb = b.getChildWithProperty (pids::id, pa.getProperty (pids::id));
        if (! pb.isValid()) return false;
        const double va = pa.getProperty (pids::value), vb = pb.getProperty (pids::value);
        if (std::abs (va - vb) > 1.0e-3 * std::max (1.0, std::abs (va))) return false;
    }
    const auto sa = a.getChildWithName (ids::sequence), sb = b.getChildWithName (ids::sequence);
    if (sa.isValid() != sb.isValid()) return false;
    if (sa.isValid())
    {
        // ignora a linha seleccionada (estado de navegação)
        auto ca = sa.createCopy(), cb = sb.createCopy();
        ca.removeProperty (ids::selected, nullptr);
        cb.removeProperty (ids::selected, nullptr);
        if (! ca.isEquivalentTo (cb)) return false;
    }
    return true;
}

bool PresetManager::isDirty()
{
    return ! same (capture (currentPresetName, ""), loadedSnapshot);
}

void PresetManager::selectSlot (int slot)
{
    slot = slot != 0 ? 1 : 0;
    if (slot == abSlot) return;
    abState[abSlot] = capture (currentPresetName, "");
    abSlot = slot;
    if (abState[abSlot].isValid())
        apply (abState[abSlot], false);
}

void PresetManager::copyAtoB()
{
    if (abSlot == 0) abState[0] = capture (currentPresetName, "");
    abState[1] = abState[0].createCopy();
    if (abSlot == 1) apply (abState[1], false);
}

void PresetManager::copyBtoA()
{
    if (abSlot == 1) abState[1] = capture (currentPresetName, "");
    abState[0] = abState[1].createCopy();
    if (abSlot == 0) apply (abState[0], false);
}

void PresetManager::restoreNameFromState()
{
    currentPresetName = apvts.state.getProperty (kStateName, "Init").toString();
    current = -1;
    for (int i = 0; i < list.size(); ++i)
        if (list[i].name == currentPresetName) { current = i; break; }
    loadedSnapshot = capture (currentPresetName, "");
    abState[0] = loadedSnapshot.createCopy();
    abState[1] = loadedSnapshot.createCopy();
    abSlot = 0;
}
} // namespace roomx::state
