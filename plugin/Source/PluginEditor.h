#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "MeshView.h"
#include "PluginProcessor.h"

namespace slink::plugin
{

/** Bare-bones listening editor: the mesh view (with a display-only exaggeration
    control), JUCE's generic parameter editor, a status line (physics rate in use,
    and whether the stability guard is limiting the spring) and an on-screen keyboard. */
class SlinkEditor final : public juce::AudioProcessorEditor,
                          private juce::Timer
{
public:
    explicit SlinkEditor (SlinkProcessor&);
    ~SlinkEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    SlinkProcessor& slinkProcessor;
    MeshView meshView;
    juce::Slider exaggeration;
    juce::Label exaggerationLabel;
    juce::TextButton resetButton { "Reset mesh" };
    juce::GenericAudioProcessorEditor generic;
    juce::Label status;
    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlinkEditor)
};

} // namespace slink::plugin
