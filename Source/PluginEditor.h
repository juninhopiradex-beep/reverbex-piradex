#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class ReverbAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ReverbAudioProcessorEditor (ReverbAudioProcessor&);
    ~ReverbAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ReverbAudioProcessor& audioProcessor;

    const juce::Colour colBackground  { 0xFF0D0D1A };
    const juce::Colour colPanel       { 0xFF12122A };
    const juce::Colour colAccent      { 0xFF9B59B6 };
    const juce::Colour colAccentLight { 0xFFBB88DD };
    const juce::Colour colText        { 0xFFE0E0F0 };
    const juce::Colour colTextMuted   { 0xFF9988BB };

    struct KnobSection
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label>  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    KnobSection roomSizeKnob;
    KnobSection dampingKnob;
    KnobSection widthKnob;
    KnobSection mixKnob;

    void setupKnob (KnobSection& section,
                    const juce::String& paramID,
                    const juce::String& labelText);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverbAudioProcessorEditor)
};
