#include "PluginEditor.h"

namespace slink::plugin
{

SlinkEditor::SlinkEditor (SlinkProcessor& p)
    : AudioProcessorEditor (p),
      slinkProcessor (p),
      meshView (p.getTopology(), p.getSnapshot()),
      generic (p),
      keyboard (p.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard)
{
    addAndMakeVisible (meshView);

    // Display only (not a plugin parameter): motion is small next to the mesh.
    exaggeration.setSliderStyle (juce::Slider::LinearHorizontal);
    exaggeration.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 20);
    exaggeration.setRange (1.0, 20.0, 0.1);
    exaggeration.setSkewFactorFromMidPoint (5.0);
    exaggeration.setTextValueSuffix ("x");
    exaggeration.onValueChange = [this] { meshView.setExaggeration ((float) exaggeration.getValue()); };
    exaggeration.setValue (4.0);
    addAndMakeVisible (exaggeration);

    exaggerationLabel.setText ("View exaggeration", juce::dontSendNotification);
    exaggerationLabel.setFont (juce::FontOptions (13.0f));
    addAndMakeVisible (exaggerationLabel);

    // Touch: drag on the mesh to grab it (not a plugin parameter, like the exaggeration).
    auto& proc = slinkProcessor;   // captured directly: the view may call onRelease while the editor is being destroyed
    meshView.onGrab    = [&proc] (std::uint32_t lump, float radius) { proc.grab (lump, radius); };
    meshView.onDrag    = [&proc] (float dx, float dy) { proc.drag (dx, dy); };
    meshView.onRelease = [&proc] { proc.release(); };

    grabSize.setSliderStyle (juce::Slider::LinearHorizontal);
    grabSize.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 20);
    grabSize.setRange (0.005, 0.2, 0.001);
    grabSize.setSkewFactorFromMidPoint (0.05);
    grabSize.onValueChange = [this] { meshView.setGrabRadius ((float) grabSize.getValue()); };
    grabSize.setValue (0.05);
    grabSize.setTooltip ("Radius of the patch a drag grabs (the mesh is 1 across)");
    addAndMakeVisible (grabSize);

    grabSizeLabel.setText ("Grab size", juce::dontSendNotification);
    grabSizeLabel.setFont (juce::FontOptions (13.0f));
    addAndMakeVisible (grabSizeLabel);

    resetButton.onClick = [this] { slinkProcessor.requestReset(); };
    addAndMakeVisible (resetButton);

    addAndMakeVisible (generic);

    status.setJustificationType (juce::Justification::centredLeft);
    status.setFont (juce::FontOptions (13.0f));
    addAndMakeVisible (status);

    keyboard.setAvailableRange (24, 108);
    keyboard.setLowestVisibleKey (36);
    keyboard.setKeyWidth (16.0f);
    addAndMakeVisible (keyboard);

    setResizable (true, true);
    setResizeLimits (760, 460, 2400, 1600);
    setSize (980, 600);

    startTimerHz (10);
}

SlinkEditor::~SlinkEditor() = default;

void SlinkEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void SlinkEditor::resized()
{
    auto r = getLocalBounds();
    keyboard.setBounds (r.removeFromBottom (80));
    status.setBounds (r.removeFromBottom (26).reduced (8, 2));

    // Mesh view on the left (square as space allows), parameters on the right.
    auto left = r.removeFromLeft (std::max (r.getWidth() - 420, r.getWidth() / 2));
    auto grabRow = left.removeFromBottom (28).reduced (8, 3);
    auto viewRow = left.removeFromBottom (28).reduced (8, 3);
    resetButton.setBounds (grabRow.removeFromRight (100));
    grabRow.removeFromRight (8);
    grabSizeLabel.setBounds (grabRow.removeFromLeft (120));
    grabSize.setBounds (grabRow);
    viewRow.removeFromRight (108);
    exaggerationLabel.setBounds (viewRow.removeFromLeft (120));
    exaggeration.setBounds (viewRow);
    meshView.setBounds (left);
    generic.setBounds (r);
}

void SlinkEditor::timerCallback()
{
    const auto rate = slinkProcessor.getCurrentPhysicsRate();
    const auto requested = slinkProcessor.getRequestedSpring();
    const auto effective = slinkProcessor.getEffectiveSpring();

    juce::String text = "Physics: " + juce::String ((int) std::round (rate)) + " steps/s";
    if (effective < requested * 0.999f)
    {
        text << "   |   Stability guard: spring limited to " << juce::String (effective, effective < 10 ? 3 : 1);
        status.setColour (juce::Label::textColourId, juce::Colours::orange);
    }
    else
    {
        status.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    }
    status.setText (text, juce::dontSendNotification);
}

} // namespace slink::plugin
