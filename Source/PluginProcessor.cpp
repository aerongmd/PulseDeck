#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParamTable.h"

juce::AudioProcessorValueTreeState::ParameterLayout PulseDeckProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    StringArray names;
    for (int i = 0; i < pd::kNumSounds; ++i)
        names.add (pd::soundName (i));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "sound", 1 }, "Sound", names, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "pitch", 1 }, "Pitch",
                                                       NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "attack", 1 }, "Attack",
                                                       NormalisableRange<float> (0.001f, 2.0f, 0.0001f, 0.3f), 0.003f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "release", 1 }, "Release",
                                                       NormalisableRange<float> (0.01f, 4.0f, 0.001f, 0.4f), 0.35f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "master", 1 }, "Master",
                                                       NormalisableRange<float> (0.0f, 1.5f, 0.001f), 0.8f));

    for (const auto& d : fxDefs())
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { d.id, 1 }, d.name,
                                                           NormalisableRange<float> (0.0f, 1.0f, 0.001f), d.def));
    return layout;
}

PulseDeckProcessor::PulseDeckProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

void PulseDeckProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

bool PulseDeckProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

pd::Params PulseDeckProcessor::readParams() const
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    pd::Params p;
    p.sound = juce::jlimit (0, pd::kNumSounds - 1, (int) get ("sound"));
    p.pitch = get ("pitch");
    p.attack = get ("attack");
    p.release = get ("release");
    p.master = get ("master");
    p.drive = get ("drive");
    p.bitcrush = get ("bitcrush");
    p.downsample = get ("downsample");
    p.lowpass = get ("lowpass");
    p.highpass = get ("highpass");
    p.resonance = get ("resonance");
    p.chorusDepth = get ("chorusDepth");
    p.chorusRate = get ("chorusRate");
    p.delayMix = get ("delayMix");
    p.delayTime = get ("delayTime");
    p.delayFeedback = get ("delayFeedback");
    p.reverbMix = get ("reverbMix");
    p.reverbSize = get ("reverbSize");
    p.width = get ("width");
    p.tremolo = get ("tremolo");
    return p;
}

void PulseDeckProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    // merge notes played on the on-screen keyboard into the host MIDI stream
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    engine.setParams (readParams());

    float* left = buffer.getWritePointer (0);
    float* right = buffer.getWritePointer (1); // isBusesLayoutSupported() guarantees stereo output

    int pos = 0;
    for (const auto meta : midi)
    {
        const int t = juce::jlimit (pos, numSamples, meta.samplePosition);
        if (t > pos)
        {
            engine.render (left + pos, right + pos, t - pos);
            pos = t;
        }

        const auto m = meta.getMessage();
        if (m.isNoteOn())                              engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff())                        engine.noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    }

    if (pos < numSamples)
        engine.render (left + pos, right + pos, numSamples - pos);
}

juce::AudioProcessorEditor* PulseDeckProcessor::createEditor()
{
    return new PulseDeckEditor (*this);
}

void PulseDeckProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PulseDeckProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PulseDeckProcessor();
}
