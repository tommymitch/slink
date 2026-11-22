#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "MeshSnapshot.h"
#include "slink/Topology.h"

namespace slink::plugin
{

/** Draws the mesh from the processor's position snapshot, once per display
    refresh. Plain JUCE graphics: a stand-in until the GPU renderer (backlog).

    Displacements are tiny compared with the mesh, so they can be exaggerated
    for display (rest + exaggeration * (position - rest)). */
class MeshView final : public juce::Component
{
public:
    MeshView (const Topology& topology, MeshSnapshot& snapshot);

    void setExaggeration (float amount) noexcept { exaggeration = amount; repaint(); }

    void paint (juce::Graphics&) override;
    void resized() override { updateTransform(); }

private:
    void updateTransform();
    juce::Point<float> toScreen (float x, float y) const noexcept;

    const Topology& topology;
    MeshSnapshot& snapshot;
    juce::VBlankAttachment vblank;

    std::vector<bool> isScan;
    float exaggeration = 1.0f;

    // Mesh-to-screen transform.
    float scale = 1.0f, offsetX = 0.0f, offsetY = 0.0f;
    float minX = 0.0f, minY = 0.0f, maxX = 1.0f, maxY = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeshView)
};

} // namespace slink::plugin
