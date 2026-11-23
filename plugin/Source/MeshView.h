#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "MeshSnapshot.h"
#include "slink/Topology.h"

#include <functional>
#include <optional>

namespace slink::plugin
{

/** Draws the mesh from the processor's position snapshot, once per display
    refresh, and turns mouse drags into grab / drag / release requests.
    Plain JUCE graphics: a stand-in until the GPU renderer (backlog).

    Displacements are tiny compared with the mesh, so they can be exaggerated
    for display (rest + exaggeration * (position - rest)). Drags are divided by
    the exaggeration, so a grabbed lump stays under the mouse. */
class MeshView final : public juce::Component
{
public:
    MeshView (const Topology& topology, MeshSnapshot& snapshot);
    ~MeshView() override;

    void setExaggeration (float amount) noexcept { exaggeration = amount; repaint(); }

    /** Grab patch radius in mesh units (the mesh is 1 across). */
    void setGrabRadius (float radius) noexcept { grabRadius = radius; repaint(); }
    float getGrabRadius() const noexcept { return grabRadius; }

    std::function<void (std::uint32_t lump, float radius)> onGrab;
    std::function<void (float dx, float dy)> onDrag;   // mesh units from the grab point
    std::function<void()> onRelease;

    void paint (juce::Graphics&) override;
    void resized() override { updateTransform(); }

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void updateTransform();
    juce::Point<float> toScreen (float x, float y) const noexcept;
    juce::Point<float> displayed (std::uint32_t lump) const noexcept;
    std::optional<std::uint32_t> nearestLump (juce::Point<float> screen) const noexcept;
    void endGrab();

    const Topology& topology;
    MeshSnapshot& snapshot;
    juce::VBlankAttachment vblank;

    std::vector<bool> isScan;
    float exaggeration = 1.0f;
    float grabRadius = 0.05f;

    std::optional<juce::Point<float>> hover;    // mouse position while over the view
    std::optional<std::uint32_t> grabbedLump;   // set between mouseDown and mouseUp
    juce::Point<float> grabMouseStart;

    // Mesh-to-screen transform.
    float scale = 1.0f, offsetX = 0.0f, offsetY = 0.0f;
    float minX = 0.0f, minY = 0.0f, maxX = 1.0f, maxY = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeshView)
};

} // namespace slink::plugin
