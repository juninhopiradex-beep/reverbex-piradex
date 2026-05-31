#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout
ReverbAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "roomSize", "Room Size",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "damping", "Damping",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "width", "Width",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "mix", "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.33f));

    return { params.begin(), params.end() };
}

ReverbAudioProcessor::ReverbAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    apvts.addParameterListener ("roomSize", this);
    apvts.addParameterListener ("damping",  this);
    apvts.addParameterListener ("width",    this);
    apvts.addParameterListener ("mix",      this);
}

ReverbAudioProcessor::~ReverbAudioProcessor()
{
    apvts.removeParameterListener ("roomSize", this);
    apvts.removeParameterListener ("damping",  this);
    apvts.removeParameterListener ("width",    this);
    apvts.removeParameterListener ("mix",      this);
}

void ReverbAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = (juce::uint32) getTotalNumOutputChannels();

    reverb.prepare (spec);
    updateReverbParams();
}

void ReverbAudioProcessor::releaseResources()
{
    reverb.reset();
}

void ReverbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);
    reverb.process (context);
}

void ReverbAudioProcessor::updateReverbParams()
{
    reverbParams.roomSize   = apvts.getRawParameterValue ("roomSize")->load();
    reverbParams.damping    = apvts.getRawParameterValue ("damping") ->load();
    reverbParams.width      = apvts.getRawParameterValue ("width")   ->load();
    reverbParams.wetLevel   = apvts.getRawParameterValue ("mix")     ->load();
    reverbParams.dryLevel   = 1.0f - reverbParams.wetLevel;
    reverbParams.freezeMode = 0.0f;

    reverb.setParameters (reverbParams);
}

void ReverbAudioProcessor::parameterChanged (const juce::String&, float)
{
    updateReverbParams();
}

void ReverbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void ReverbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessorEditor* ReverbAudioProcessor::createEditor()
{
    return new ReverbAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReverbAudioProcessor();
}
