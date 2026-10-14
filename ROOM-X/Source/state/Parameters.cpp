#include "Parameters.h"
#include "../dsp/SpaceLibrary.h"
#include "../music/MusicTheory.h"

namespace roomx::params
{
using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;
using APC = juce::AudioParameterChoice;
using API = juce::AudioParameterInt;
using Attr = juce::AudioParameterFloatAttributes;

const juce::Array<NoteDivision>& preDelayDivisions()
{
    static const juce::Array<NoteDivision> d = []
    {
        juce::Array<NoteDivision> a;
        for (const auto& x : std::initializer_list<NoteDivision> {
        { "1/64", 1.0 / 16.0 }, { "1/32T", 1.0 / 12.0 }, { "1/32", 1.0 / 8.0 }, { "1/16T", 1.0 / 6.0 },
        { "1/16", 0.25 }, { "1/16D", 0.375 }, { "1/8T", 1.0 / 3.0 }, { "1/8", 0.5 }, { "1/8D", 0.75 },
        { "1/4T", 2.0 / 3.0 }, { "1/4", 1.0 }, { "1/4D", 1.5 }, { "1/2", 2.0 }, { "1/2D", 3.0 }, { "1/1", 4.0 } })
            a.add (x);
        return a;
    }();
    return d;
}

const juce::Array<NoteDivision>& gridDivisions()
{
    static const juce::Array<NoteDivision> d = []
    {
        juce::Array<NoteDivision> a;
        for (const auto& x : std::initializer_list<NoteDivision> {
        { "Bar", 4.0 }, { "1/2", 2.0 }, { "Beat", 1.0 }, { "1/8", 0.5 }, { "1/16", 0.25 },
        { "1/2T", 4.0 / 3.0 }, { "1/4T", 2.0 / 3.0 }, { "1/8T", 1.0 / 3.0 }, { "1/16T", 1.0 / 6.0 } })
            a.add (x);
        return a;
    }();
    return d;
}

juce::String positionToText (float v)
{
    const int i = juce::roundToInt (v);
    if (i < 0) return juce::String (-i) + " E";
    if (i > 0) return juce::String (i) + " L";
    return "0";
}

juce::String beatsToBarBeat (double beats)
{
    const int bar  = (int) std::floor (beats / 4.0) + 1;
    const double inBar = beats - (bar - 1) * 4.0;
    const int beat = (int) std::floor (inBar) + 1;
    const int frac = juce::roundToInt ((inBar - std::floor (inBar)) * 100.0) % 100;
    return juce::String (bar) + ":" + juce::String (beat) + "." + juce::String (frac).paddedLeft ('0', 2);
}

juce::String formatHz (float hz)
{
    if (hz >= 1000.0f) return juce::String (hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz";
    return juce::String (juce::roundToInt (hz)) + " Hz";
}

juce::String formatMs (float ms)
{
    if (ms >= 1000.0f) return juce::String (ms / 1000.0f, 2) + " s";
    return juce::String (juce::roundToInt (ms)) + " ms";
}

bool isInstanceOnly (const juce::String& id)
{
    return id == tuneRef || id == tuneCents || id == bypass || id == eco || id == mixLock || id == notesLock;
}

static juce::NormalisableRange<float> logRange (float lo, float hi)
{
    return { lo, hi,
             [] (float s, float e, float v) { return s * std::pow (e / s, v); },
             [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
             [] (float, float, float v) { return v; } };
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using PID = juce::ParameterID;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto dB = [] (float v, int) { return juce::String (v, 1) + " dB"; };
    auto pct = [] (float v, int) { return juce::String (juce::roundToInt (v)) + "%"; };

    // ---------------------------------------------------------------- Reverb
    auto reverb = std::make_unique<juce::AudioProcessorParameterGroup> ("reverb", "Reverb", "|");
    {
        juce::StringArray spaces;
        for (int i = 0; i < dsp::kNumSpaces; ++i) spaces.add (dsp::space (i).name);
        reverb->addChild (std::make_unique<APC> (PID { space, 1 }, "Space", spaces, 6));
    }
    reverb->addChild (std::make_unique<APF> (PID { decay, 1 }, "Decay", logRange (0.01f, 30.0f), 1.0f,
        Attr().withLabel ("x").withStringFromValueFunction ([] (float v, int) { return juce::String (v, v < 1.0f ? 2 : 1) + "x"; })));
    reverb->addChild (std::make_unique<APF> (PID { size, 1 }, "Size", juce::NormalisableRange<float> (50.0f, 200.0f, 1.0f), 100.0f,
        Attr().withLabel ("%").withStringFromValueFunction (pct)));
    reverb->addChild (std::make_unique<APF> (PID { position, 1 }, "Position", juce::NormalisableRange<float> (-99.0f, 99.0f, 1.0f), 0.0f,
        Attr().withStringFromValueFunction ([] (float v, int) { return positionToText (v); })));
    reverb->addChild (std::make_unique<APF> (PID { density, 1 }, "Density", juce::NormalisableRange<float> (1.0f, 100.0f, 1.0f), 100.0f,
        Attr().withLabel ("%").withStringFromValueFunction (pct)));
    reverb->addChild (std::make_unique<APF> (PID { offset, 1 }, "Offset", juce::NormalisableRange<float> (-500.0f, 500.0f, 1.0f), 0.0f,
        Attr().withLabel ("Hz").withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " Hz"; })));
    {
        juce::NormalisableRange<float> r (0.0f, 2000.0f, 0.1f);
        r.setSkewForCentre (120.0f);
        reverb->addChild (std::make_unique<APF> (PID { preDelay, 1 }, "Pre-Delay", r, 0.0f,
            Attr().withLabel ("ms").withStringFromValueFunction ([] (float v, int) { return formatMs (v); })));
    }
    reverb->addChild (std::make_unique<APB> (PID { preDelaySync, 1 }, "Pre-Delay Sync", false));
    {
        juce::StringArray divs;
        for (auto& d : preDelayDivisions()) divs.add (d.name);
        reverb->addChild (std::make_unique<APC> (PID { preDelayDiv, 1 }, "Pre-Delay Division", divs, 4));
    }
    reverb->addChild (std::make_unique<APB> (PID { dualMono, 1 }, "Dual Mono", false));
    layout.add (std::move (reverb));

    // ---------------------------------------------------------------- Musical
    auto musical = std::make_unique<juce::AudioProcessorParameterGroup> ("musical", "Musical", "|");
    musical->addChild (std::make_unique<APF> (PID { temper, 1 }, "Temper", juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f,
        Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)); })));
    musical->addChild (std::make_unique<APF> (PID { noteWidth, 1 }, "Note Width", juce::NormalisableRange<float> (1.0f, 99.0f, 0.1f), 50.0f,
        Attr().withLabel ("%").withStringFromValueFunction (pct)));
    musical->addChild (std::make_unique<APC> (PID { target, 1 }, "Target", juce::StringArray { "Early", "All", "Late" }, 1));
    auto noteText = [] (float v, int) { return juce::String (music::noteName (juce::roundToInt (v), false)); };
    musical->addChild (std::make_unique<APF> (PID { rangeLow, 1 }, "Temper Range Low",
        juce::NormalisableRange<float> ((float) music::kNoteMin, (float) music::kNoteMax, 0.01f), (float) music::kNoteMin,
        Attr().withStringFromValueFunction (noteText)));
    musical->addChild (std::make_unique<APF> (PID { rangeHigh, 1 }, "Temper Range High",
        juce::NormalisableRange<float> ((float) music::kNoteMin, (float) music::kNoteMax, 0.01f), (float) music::kNoteMax,
        Attr().withStringFromValueFunction (noteText)));
    layout.add (std::move (musical));

    // ---------------------------------------------------------------- Note selection
    auto sel = std::make_unique<juce::AudioProcessorParameterGroup> ("notesel", "Note Selection", "|");
    sel->addChild (std::make_unique<APC> (PID { noteMode, 1 }, "Note Mode", juce::StringArray { "Manual", "Sequence", "MIDI" }, 0));
    sel->addChild (std::make_unique<API> (PID { notes, 1 }, "Manual Notes", 0, 4095, 0x091));
    layout.add (std::move (sel));

    // ---------------------------------------------------------------- Sequencer
    auto seq = std::make_unique<juce::AudioProcessorParameterGroup> ("sequencer", "Sequencer", "|");
    seq->addChild (std::make_unique<API> (PID { seqBars, 1 }, "Sequence Bars", 1, 64, 4));
    {
        juce::StringArray grid;
        for (auto& d : gridDivisions()) grid.add (d.name);
        seq->addChild (std::make_unique<APC> (PID { seqGrid, 1 }, "Sequence Grid", grid, 2));
    }
    seq->addChild (std::make_unique<API> (PID { seqTranspose, 1 }, "Sequence Transpose", -12, 12, 0));
    seq->addChild (std::make_unique<APB> (PID { seqArmed, 1 }, "Sequence Armed", true));
    seq->addChild (std::make_unique<APF> (PID { seqOffset, 1 }, "Session Offset", juce::NormalisableRange<float> (0.0f, 252.0f, 0.25f), 0.0f,
        Attr().withStringFromValueFunction ([] (float v, int) { return beatsToBarBeat (v); })));
    layout.add (std::move (seq));

    // ---------------------------------------------------------------- MIDI
    auto midi = std::make_unique<juce::AudioProcessorParameterGroup> ("midi", "MIDI", "|");
    midi->addChild (std::make_unique<APB> (PID { midiOctave, 1 }, "Octave Repeat", true));
    {
        juce::NormalisableRange<float> r (5.0f, 2000.0f, 1.0f);
        r.setSkewForCentre (150.0f);
        midi->addChild (std::make_unique<APF> (PID { midiRelease, 1 }, "MIDI Release", r, 120.0f,
            Attr().withLabel ("ms").withStringFromValueFunction ([] (float v, int) { return formatMs (v); })));
    }
    layout.add (std::move (midi));

    // ---------------------------------------------------------------- Filters
    auto filt = std::make_unique<juce::AudioProcessorParameterGroup> ("filters", "Filters", "|");
    auto hzText = [] (float v, int) { return formatHz (v); };
    filt->addChild (std::make_unique<APF> (PID { lowCut, 1 }, "Low Cut", logRange (20.0f, 20000.0f), 20.0f,
        Attr().withLabel ("Hz").withStringFromValueFunction (hzText)));
    filt->addChild (std::make_unique<APC> (PID { lowCutSlope, 1 }, "Low Cut Slope", juce::StringArray { "12 dB/oct", "36 dB/oct" }, 0));
    filt->addChild (std::make_unique<APF> (PID { highCut, 1 }, "High Cut", logRange (20.0f, 20000.0f), 20000.0f,
        Attr().withLabel ("Hz").withStringFromValueFunction (hzText)));
    filt->addChild (std::make_unique<APC> (PID { highCutSlope, 1 }, "High Cut Slope", juce::StringArray { "12 dB/oct", "36 dB/oct" }, 0));
    layout.add (std::move (filt));

    // ---------------------------------------------------------------- Input / Output
    auto io = std::make_unique<juce::AudioProcessorParameterGroup> ("io", "Input/Output", "|");
    {
        juce::NormalisableRange<float> r (-60.0f, 12.0f, 0.1f);
        r.setSkewForCentre (-6.0f);
        io->addChild (std::make_unique<APF> (PID { input, 1 }, "Input", r, 0.0f, Attr().withLabel ("dB").withStringFromValueFunction (dB)));
        io->addChild (std::make_unique<APF> (PID { output, 1 }, "Output", r, 0.0f, Attr().withLabel ("dB").withStringFromValueFunction (dB)));
    }
    io->addChild (std::make_unique<APF> (PID { mix, 1 }, "Mix", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f,
        Attr().withLabel ("%").withStringFromValueFunction (pct)));
    io->addChild (std::make_unique<APB> (PID { killDry, 1 }, "Kill Dry", false));
    layout.add (std::move (io));

    // ---------------------------------------------------------------- Utility
    auto util = std::make_unique<juce::AudioProcessorParameterGroup> ("utility", "Utility", "|");
    util->addChild (std::make_unique<APB> (PID { bypass, 1 }, "Bypass", false));
    util->addChild (std::make_unique<APB> (PID { eco, 1 }, "Eco Mode", false));
    util->addChild (std::make_unique<APF> (PID { tuneRef, 1 }, "Reference A4", juce::NormalisableRange<float> (400.0f, 500.0f, 0.1f), 440.0f,
        Attr().withLabel ("Hz").withAutomatable (false).withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " Hz"; })));
    util->addChild (std::make_unique<APF> (PID { tuneCents, 1 }, "Fine Tune", juce::NormalisableRange<float> (-50.0f, 50.0f, 0.1f), 0.0f,
        Attr().withLabel ("ct").withAutomatable (false).withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " ct"; })));
    util->addChild (std::make_unique<APB> (PID { mixLock, 1 }, "Mix Lock", false, juce::AudioParameterBoolAttributes().withAutomatable (false)));
    util->addChild (std::make_unique<APB> (PID { notesLock, 1 }, "Notes Lock", false, juce::AudioParameterBoolAttributes().withAutomatable (false)));
    layout.add (std::move (util));

    return layout;
}
} // namespace roomx::params
