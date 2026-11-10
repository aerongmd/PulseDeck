#include "PluginEditor.h"
#include "ParamTable.h"

PulseDeckEditor::PulseDeckEditor (PulseDeckProcessor& p)
    : AudioProcessorEditor (&p),
      proc (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    // sound selector
    for (int i = 0; i < pd::kNumSounds; ++i)
        soundBox.addItem (pd::soundName (i), i + 1);
    soundAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "sound", soundBox);
    addAndMakeVisible (soundBox);

    prevButton.onClick = [this] { stepSound (-1); };
    nextButton.onClick = [this] { stepSound (+1); };
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);

    // user presets (stored as XML files in the user's application data folder)
    presetBox.onChange = [this]
    {
        const auto file = presetDir().getChildFile (presetBox.getText() + ".pdpreset");
        if (auto xml = juce::XmlDocument::parse (file))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            if (state.hasType (proc.apvts.state.getType()))
                proc.apvts.replaceState (state);
        }
    };
    addAndMakeVisible (presetBox);
    saveButton.onClick = [this] { savePresetDialog(); };
    addAndMakeVisible (saveButton);
    refreshPresets();

    // knobs: 4 voice controls + 15 effects
    addKnob ("pitch", "Pitch");
    addKnob ("attack", "Attack");
    addKnob ("release", "Release");
    addKnob ("master", "Master");
    for (const auto& d : fxDefs())
        addKnob (d.id, d.name);

    keyboard.setAvailableRange (36, 96);
    addAndMakeVisible (keyboard);

    setSize (900, 440);
}

void PulseDeckEditor::addKnob (const char* paramId, const juce::String& text)
{
    auto k = std::make_unique<Knob>();
    k->label.setText (text, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k->slider);
    addAndMakeVisible (k->label);
    addAndMakeVisible (k->slider);
    knobs.push_back (std::move (k));
}

void PulseDeckEditor::stepSound (int delta)
{
    const int count = soundBox.getNumItems();
    const int next = (soundBox.getSelectedItemIndex() + delta + count) % count;
    soundBox.setSelectedItemIndex (next, juce::sendNotificationSync);
}

juce::File PulseDeckEditor::presetDir()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("PulseDeck").getChildFile ("Presets");
    dir.createDirectory();
    return dir;
}

void PulseDeckEditor::refreshPresets (const juce::String& select)
{
    presetBox.clear (juce::dontSendNotification);
    int id = 1, selectedId = 0;
    for (const auto& f : presetDir().findChildFiles (juce::File::findFiles, false, "*.pdpreset"))
    {
        presetBox.addItem (f.getFileNameWithoutExtension(), id);
        if (f.getFileNameWithoutExtension() == select) selectedId = id;
        ++id;
    }
    if (selectedId != 0) presetBox.setSelectedId (selectedId, juce::dontSendNotification);
    else presetBox.setTextWhenNothingSelected ("User presets");
}

void PulseDeckEditor::savePresetDialog()
{
    auto* window = new juce::AlertWindow ("Save preset", "Preset name:", juce::MessageBoxIconType::NoIcon);
    window->addTextEditor ("name", "", "");
    window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PulseDeckEditor> safe (this);
    window->enterModalState (true,
        juce::ModalCallbackFunction::create ([safe, window] (int result)
        {
            if (result != 1 || safe == nullptr) return;
            const auto name = juce::File::createLegalFileName (window->getTextEditorContents ("name").trim());
            if (name.isEmpty()) return;

            if (auto xml = safe->proc.apvts.copyState().createXml())
                xml->writeTo (presetDir().getChildFile (name + ".pdpreset"));
            safe->refreshPresets (name);
        }),
        true);
}

void PulseDeckEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void PulseDeckEditor::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto top = area.removeFromTop (30);
    prevButton.setBounds (top.removeFromLeft (30));
    soundBox.setBounds (top.removeFromLeft (200));
    nextButton.setBounds (top.removeFromLeft (30));
    top.removeFromLeft (30);
    presetBox.setBounds (top.removeFromLeft (220));
    top.removeFromLeft (8);
    saveButton.setBounds (top.removeFromLeft (110));

    auto keys = area.removeFromBottom (110);
    keyboard.setKeyWidth (keys.getWidth() / 36.0f);
    keyboard.setBounds (keys);

    area.removeFromTop (10);
    const int columns = 10;
    const int rows = ((int) knobs.size() + columns - 1) / columns;
    const int cellW = area.getWidth() / columns, cellH = area.getHeight() / rows;
    for (int i = 0; i < (int) knobs.size(); ++i)
    {
        juce::Rectangle<int> cell (area.getX() + (i % columns) * cellW,
                                   area.getY() + (i / columns) * cellH, cellW, cellH);
        cell.reduce (4, 2);
        knobs[(size_t) i]->label.setBounds (cell.removeFromTop (18));
        knobs[(size_t) i]->slider.setBounds (cell);
    }
}
