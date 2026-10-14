#include "SequenceModel.h"

namespace roomx::state
{
using namespace roomx::music;

juce::ValueTree SequenceModel::toTree (const SequenceData& d)
{
    juce::ValueTree t (ids::sequence);
    t.setProperty (ids::selected, d.selectedRow, nullptr);
    for (int i = 0; i < kSeqRows; ++i)
    {
        juce::ValueTree r (ids::row);
        r.setProperty (ids::root, d.rows[i].root, nullptr);
        r.setProperty (ids::cat,  (int) d.rows[i].category, nullptr);
        r.setProperty (ids::type, d.rows[i].typeIndex, nullptr);
        t.appendChild (r, nullptr);
    }
    for (int i = 0; i < d.numRegions; ++i)
    {
        juce::ValueTree r (ids::region);
        r.setProperty (ids::rowIdx, d.regions[i].row, nullptr);
        r.setProperty (ids::start,  d.regions[i].start, nullptr);
        r.setProperty (ids::length, d.regions[i].length, nullptr);
        t.appendChild (r, nullptr);
    }
    return t;
}

SequenceData SequenceModel::fromTree (const juce::ValueTree& t)
{
    SequenceData d = SequenceData::makeDefault();
    if (! t.isValid()) return d;

    d.selectedRow = juce::jlimit (0, kSeqRows - 1, (int) t.getProperty (ids::selected, 0));
    int ri = 0;
    d.numRegions = 0;
    for (const auto& c : t)
    {
        if (c.hasType (ids::row) && ri < kSeqRows)
        {
            const int cat = juce::jlimit (0, kNumCategories - 1, (int) c.getProperty (ids::cat, 1));
            d.rows[ri].category  = (Category) cat;
            d.rows[ri].root      = juce::jlimit (0, 11, (int) c.getProperty (ids::root, 0));
            d.rows[ri].typeIndex = juce::jlimit (0, numTypes ((Category) cat) - 1, (int) c.getProperty (ids::type, 0));
            ++ri;
        }
        else if (c.hasType (ids::region) && d.numRegions < kSeqMaxRegions)
        {
            auto& r = d.regions[d.numRegions++];
            r.row    = juce::jlimit (0, kSeqRows - 1, (int) c.getProperty (ids::rowIdx, 0));
            r.start  = juce::jmax (0.0, (double) c.getProperty (ids::start, 0.0));
            r.length = juce::jmax (1.0 / 16.0, (double) c.getProperty (ids::length, 1.0));
        }
    }
    return d;
}

juce::ValueTree SequenceModel::tree()
{
    auto t = apvts.state.getChildWithName (ids::sequence);
    if (! t.isValid())
    {
        t = toTree (SequenceData::makeDefault());
        apvts.state.appendChild (t, nullptr);
    }
    return t;
}

juce::ValueTree SequenceModel::rowTree (int i)
{
    int k = 0;
    for (auto c : tree())
        if (c.hasType (ids::row) && k++ == i) return c;
    return {};
}

Harmony SequenceModel::getRow (int i)
{
    const auto d = fromTree (tree());
    return d.rows[juce::jlimit (0, kSeqRows - 1, i)];
}

void SequenceModel::setRow (int i, const Harmony& h)
{
    auto r = rowTree (i);
    if (! r.isValid()) return;
    undo()->beginNewTransaction ("Sequence row");
    r.setProperty (ids::root, h.root, undo());
    r.setProperty (ids::cat,  (int) h.category, undo());
    r.setProperty (ids::type, h.typeIndex, undo());
}

int SequenceModel::getNumRegions()
{
    int n = 0;
    for (const auto& c : tree()) if (c.hasType (ids::region)) ++n;
    return n;
}

static juce::ValueTree nthRegion (juce::ValueTree t, int i)
{
    int k = 0;
    for (auto c : t)
        if (c.hasType (ids::region) && k++ == i) return c;
    return {};
}

SeqRegion SequenceModel::getRegion (int i)
{
    auto c = nthRegion (tree(), i);
    SeqRegion r;
    if (c.isValid())
    {
        r.row = (int) c.getProperty (ids::rowIdx, 0);
        r.start = (double) c.getProperty (ids::start, 0.0);
        r.length = (double) c.getProperty (ids::length, 1.0);
    }
    return r;
}

int SequenceModel::addRegion (const SeqRegion& r)
{
    if (getNumRegions() >= kSeqMaxRegions) return -1;
    juce::ValueTree c (ids::region);
    c.setProperty (ids::rowIdx, r.row, nullptr);
    c.setProperty (ids::start, r.start, nullptr);
    c.setProperty (ids::length, r.length, nullptr);
    tree().appendChild (c, undo());
    return getNumRegions() - 1;
}

void SequenceModel::setRegion (int i, const SeqRegion& r)
{
    auto c = nthRegion (tree(), i);
    if (! c.isValid()) return;
    c.setProperty (ids::rowIdx, juce::jlimit (0, kSeqRows - 1, r.row), undo());
    c.setProperty (ids::start, juce::jmax (0.0, r.start), undo());
    c.setProperty (ids::length, juce::jmax (1.0 / 16.0, r.length), undo());
}

void SequenceModel::removeRegions (juce::Array<int> indices)
{
    indices.sort();
    auto t = tree();
    for (int k = indices.size(); --k >= 0;)
    {
        auto c = nthRegion (t, indices[k]);
        if (c.isValid()) t.removeChild (c, undo());
    }
}

void SequenceModel::clearRegions()
{
    auto t = tree();
    for (int i = t.getNumChildren(); --i >= 0;)
        if (t.getChild (i).hasType (ids::region)) t.removeChild (i, undo());
}

int SequenceModel::getSelectedRow()
{
    return juce::jlimit (0, kSeqRows - 1, (int) tree().getProperty (ids::selected, 0));
}

void SequenceModel::setSelectedRow (int r)
{
    tree().setProperty (ids::selected, juce::jlimit (0, kSeqRows - 1, r), nullptr);
}

SequenceData SequenceModel::compile()
{
    return fromTree (tree());
}

void SequenceModel::replaceWith (const juce::ValueTree& seq)
{
    auto t = tree();
    auto fresh = toTree (fromTree (seq));    // valida/limpa
    // substitui os filhos (undoable) e a linha seleccionada
    for (int i = t.getNumChildren(); --i >= 0;) t.removeChild (i, undo());
    for (const auto& c : fresh) t.appendChild (c.createCopy(), undo());
    t.setProperty (ids::selected, fresh.getProperty (ids::selected), undo());
}
} // namespace roomx::state
