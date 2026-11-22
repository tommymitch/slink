#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace slink::plugin
{

SlinkProcessor::SlinkProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "SlinkParameters", createParameterLayout()),
      params (state)
{
}

bool SlinkProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void SlinkProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    lastPhysicsRate = params.physicsRateHz (sampleRate);
    voice.prepare (sampleRate, lastPhysicsRate);
    applyParameters (true);

    gain.reset (sampleRate, 0.02);
    gain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.gain->load()));

    numHeld = 0;
    keyboardState.reset();
}

void SlinkProcessor::applyParameters (bool force) noexcept
{
    const auto rate = params.physicsRateHz (sampleRate);
    if (force || ! juce::exactlyEqual (rate, lastPhysicsRate))
    {
        voice.setPhysicsRate (rate);   // re-resolves step parameters for the new rate
        lastPhysicsRate = rate;
        force = true;
    }

    const auto p = params.coreParams();
    using juce::exactlyEqual;
    if (force || ! exactlyEqual (p.springConstant, lastParams.springConstant) || ! exactlyEqual (p.mass, lastParams.mass)
              || ! exactlyEqual (p.friction, lastParams.friction) || ! exactlyEqual (p.smoothing, lastParams.smoothing)
              || ! exactlyEqual (p.homing, lastParams.homing) || ! exactlyEqual (p.gravityY, lastParams.gravityY)
              || p.springsOn != lastParams.springsOn || p.integrator != lastParams.integrator)
    {
        voice.setParams (p);
        lastParams = p;
    }

    voice.velocitySensitivity = params.velocity->load();

    voice.setFreeEdges (params.freeEdges->load() >= 0.5f);
    voice.excitation = (slink::Excitation) std::clamp ((int) params.excitation->load(), 0, 2);
    voice.harmonicX  = (int) params.harmonicX->load();
    voice.harmonicY  = (int) params.harmonicY->load();
    voice.strikeX    = params.strikeX->load();
    voice.strikeY    = params.strikeY->load();
    voice.setScanMode ((slink::ScanMode) std::clamp ((int) params.scanMode->load(), 0, 3));

    requestedSpring.store (p.springConstant);
    effectiveSpring.store (voice.getStepParams().springConstant);
    currentPhysicsRate.store (voice.getPhysicsRate());
}

void SlinkProcessor::handleMidi (const juce::MidiMessage& m) noexcept
{
    auto removeHeld = [this] (int note)
    {
        for (int i = 0; i < numHeld; ++i)
            if (heldNotes[(size_t) i] == note)
            {
                for (int j = i; j < numHeld - 1; ++j)
                    heldNotes[(size_t) j] = heldNotes[(size_t) j + 1];
                --numHeld;
                return;
            }
    };

    if (m.isNoteOn())
    {
        const int note = m.getNoteNumber();
        removeHeld (note);
        if (numHeld < (int) heldNotes.size())
            heldNotes[(size_t) numHeld++] = note;
        voice.noteOn (note, m.getFloatVelocity());
    }
    else if (m.isNoteOff())
    {
        const int note = m.getNoteNumber();
        const bool wasCurrent = numHeld > 0 && heldNotes[(size_t) numHeld - 1] == note;
        removeHeld (note);
        // Last-note priority: fall back to the previous held note's pitch without a
        // new hit. With nothing held the mesh just rings on (no envelope yet).
        if (wasCurrent && numHeld > 0)
            voice.setPitch (heldNotes[(size_t) numHeld - 1]);
    }
    else if (m.isAllNotesOff())
    {
        numHeld = 0;
    }
    else if (m.isAllSoundOff() || m.isResetAllControllers())
    {
        numHeld = 0;
        voice.reset();
    }
}

void SlinkProcessor::render (float* output, int numSamples) noexcept
{
    if (numSamples > 0)
        voice.render (output, numSamples);
}

void SlinkProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    // On-screen keyboard (standalone / editor) merges into the MIDI stream.
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    applyParameters (false);

    if (resetRequested.exchange (false))
        voice.reset();

    // Sample-accurate MIDI: render up to each event, then apply it.
    float* out = buffer.getWritePointer (0);
    int pos = 0;
    for (const auto meta : midi)
    {
        const int eventPos = std::clamp (meta.samplePosition, 0, numSamples);
        render (out + pos, eventPos - pos);
        pos = std::max (pos, eventPos);
        handleMidi (meta.getMessage());
    }
    render (out + pos, numSamples - pos);

    gain.setTargetValue (juce::Decibels::decibelsToGain (params.gain->load()));
    gain.applyGain (out, numSamples);

    for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);

    // Latest positions for the mesh view (~6 KB copy per block).
    const auto& meshState = voice.getState();
    snapshot.publish (meshState.x.data(), meshState.y.data());
}

juce::AudioProcessorEditor* SlinkProcessor::createEditor()
{
    return new SlinkEditor (*this);
}

void SlinkProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SlinkProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

} // namespace slink::plugin

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new slink::plugin::SlinkProcessor();
}
