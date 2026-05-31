#include "PluginEditor.h"

ReverbAudioProcessorEditor::ReverbAudioProcessorEditor (ReverbAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (480, 320);
    setupKnob (roomSizeKnob, "roomSize", "Room Size");
    setupKnob (dampingKnob,  "damping",  "Damping");
    setupKnob (widthKnob,    "width",    "Width");
    setupKnob (mixKnob,      "mix",      "Mix");
}

ReverbAudioProcessorEditor::~ReverbAudioProcessorEditor() {}

void ReverbAudioProcessorEditor::setupKnob (KnobSection& section,
                                             const juce::String& paramID,
                                             const juce::String& labelText)
{
    section.slider = std::make_unique<juce::Slider>();
    section.slider->setSliderStyle (juce::Slider::RotaryVerticalDrag);
    section.slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 20);
    section.slider->setColour (juce::Slider::rotarySliderFillColourId,    colAccent);
    section.slider->setColour (juce::Slider::rotarySliderOutlineColourId, colPanel);
    section.slider->setColour (juce::Slider::thumbColourId,               colAccentLight);
    section.slider->setColour (juce::Slider::textBoxTextColourId,         colText);
    section.slider->setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
    section.slider->setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    addAndMakeVisible (*section.slider);

    section.label = std::make_unique<juce::Label>();
    section.label->setText (labelText, juce::dontSendNotification);
    section.label->setJustificationType (juce::Justification::centred);
    section.label->setColour (juce::Label::textColourId, colTextMuted);
    section.label->setFont (juce::Font (13.0f, juce::Font::plain));
    addAndMakeVisible (*section.label);

    section.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, paramID, *section.slider);
}

void ReverbAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (colBackground);

    g.setColour (colPanel);
    g.fillRoundedRectangle (0.0f, 0.0f, (float)getWidth(), 60.0f, 0.0f);

    g.setColour (colAccent);
    g.fillRect (0, 0, getWidth(), 3);

    g.setColour (colAccentLight);
    g.setFont (juce::Font (26.0f, juce::Font::bold));
    g.drawText ("REVERBEX", 0, 8, getWidth(), 36,
                juce::Justification::centred, false);

    g.setColour (colTextMuted);
    g.setFont (juce::Font (12.0f));
    g.drawText ("by Piradex", 0, 38, getWidth(), 18,
                juce::Justification::centred, false);

    g.setColour (colPanel.brighter (0.1f));
    g.fillRect (20, 59, getWidth() - 40, 1);

    g.setColour (colTextMuted.withAlpha (0.5f));
    g.setFont (juce::Font (10.0f));
    g.drawText ("v1.0  |  Piradex", 0, getHeight() - 18, getWidth(), 14,
