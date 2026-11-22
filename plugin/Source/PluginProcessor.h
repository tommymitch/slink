#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include "MeshSnapshot.h"
#include "Parameters.h"
#include "slink/MeshBuilders.h"
#include "slink/Voice.h"

#include <array>

namespace slink::plugin
{

/** Monophonic Slink synth: one SlinkCore Voice on the audio thread.

    Everything the audio thread touches (the Voice, the held-note stack) is
    owned and used only by the audio thread; the GUI talks to it through
    parameters and the keyboard state, both thread-safe. */
class SlinkProcessor final : public juce::AudioProcessor
{
public:
    SlinkProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }

    /** For the editor: the spring constant actually in use, and the requested one
        (they differ when the stability guard is limiting it). */
    float getEffectiveSpring() const noexcept { return effectiveSpring.load(); }
    float getRequestedSpring() const noexcept { return requestedSpring.load(); }
    double getCurrentPhysicsRate() const noexcept { return currentPhysicsRate.load(); }

    /** For the editor's mesh view. The topology never changes after construction. */
    const slink::Topology& getTopology() const noexcept { return voice.getTopology(); }
    MeshSnapshot& getSnapshot() noexcept { return snapshot; }

    /** From the editor: puts the mesh back at rest on the next audio block. */
    void requestReset() noexcept { resetRequested.store (true); }

private:
    void applyParameters (bool force) noexcept;
    void handleMidi (const juce::MidiMessage& message) noexcept;
    void render (float* output, int numSamples) noexcept;

    juce::AudioProcessorValueTreeState state;
    ParameterRefs params;
    juce::MidiKeyboardState keyboardState;

    slink::Voice voice { slink::makeSpiderCross() };
    MeshSnapshot snapshot { voice.getTopology().numLumps };   // after voice
    slink::Params lastParams;
    double lastPhysicsRate = 0.0;
    double sampleRate = 48000.0;

    juce::SmoothedValue<float> gain;

    // Held notes, most recent last (last-note priority).
    std::array<int, 128> heldNotes {};
    int numHeld = 0;

    std::atomic<float> effectiveSpring { 0.8f }, requestedSpring { 0.8f };
    std::atomic<double> currentPhysicsRate { 60.0 };
    std::atomic<bool> resetRequested { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlinkProcessor)
};

} // namespace slink::plugin
