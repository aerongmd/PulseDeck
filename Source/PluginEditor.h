#pragma once
#include "PluginProcessor.h"

class PulseDeckEditor : public juce::AudioProcessorEditor
{
public:
    explicit PulseDeckEditor (PulseDeckProcessor&);
    ~PulseDeckEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void addKnob (const char* paramId, const juce::String& text);
    void stepSound (int delta);

    static juce::File presetDir();
    void refreshPresets (const juce::String& select = {});
    void savePresetDialog();

    PulseDeckProcessor& proc;

    juce::ComboBox soundBox, presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "Save preset" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> soundAttachment;
    std::vector<std::unique_ptr<Knob>> knobs;
    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PulseDeckEditor)
};
