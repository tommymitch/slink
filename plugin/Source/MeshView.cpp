#include "MeshView.h"

namespace slink::plugin
{

MeshView::MeshView (const Topology& t, MeshSnapshot& s)
    : topology (t),
      snapshot (s),
      vblank (this, [this] { if (snapshot.fetch()) repaint(); }),
      isScan (t.numLumps, false)
{
    for (auto i : topology.scanPath)
        isScan[i] = true;

    if (topology.numLumps > 0)
    {
        minX = *std::min_element (topology.restX.begin(), topology.restX.end());
        maxX = *std::max_element (topology.restX.begin(), topology.restX.end());
        minY = *std::min_element (topology.restY.begin(), topology.restY.end());
        maxY = *std::max_element (topology.restY.begin(), topology.restY.end());
    }

    setOpaque (true);
}

MeshView::~MeshView()
{
    endGrab();   // never leave the mesh held if the editor closes mid-drag
}

void MeshView::updateTransform()
{
    const auto bounds = getLocalBounds().toFloat().reduced (12.0f);
    const float w = std::max (maxX - minX, 1.0e-6f), h = std::max (maxY - minY, 1.0e-6f);
    scale = std::min (bounds.getWidth() / w, bounds.getHeight() / h);
    offsetX = bounds.getCentreX() - scale * 0.5f * (minX + maxX);
    offsetY = bounds.getCentreY() + scale * 0.5f * (minY + maxY);
}

juce::Point<float> MeshView::toScreen (float x, float y) const noexcept
{
    return { offsetX + scale * x, offsetY - scale * y };   // y up
}

juce::Point<float> MeshView::displayed (std::uint32_t i) const noexcept
{
    const auto& rx = topology.restX;
    const auto& ry = topology.restY;
    if (! snapshot.hasData())
        return toScreen (rx[i], ry[i]);

    const auto& frame = snapshot.latest();
    const float e = exaggeration;
    return toScreen (rx[i] + e * (frame.x[i] - rx[i]), ry[i] + e * (frame.y[i] - ry[i]));
}

std::optional<std::uint32_t> MeshView::nearestLump (juce::Point<float> screen) const noexcept
{
    const float reach = std::max (16.0f, grabRadius * scale);
    float best = reach * reach;
    std::optional<std::uint32_t> found;

    for (std::uint32_t i = 0; i < topology.numLumps; ++i)
    {
        const float d = displayed (i).getDistanceSquaredFrom (screen);
        if (d < best)
        {
            best = d;
            found = i;
        }
    }
    return found;
}

void MeshView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff15171a));

    juce::Path springs, scanRing;
    auto addSprings = [&] (const std::vector<std::uint32_t>& a, const std::vector<std::uint32_t>& b)
    {
        for (size_t k = 0; k < a.size(); ++k)
        {
            const auto p = displayed (a[k]), q = displayed (b[k]);
            auto& path = (isScan[a[k]] && isScan[b[k]]) ? scanRing : springs;
            path.startNewSubPath (p);
            path.lineTo (q);
        }
    };
    addSprings (topology.linA, topology.linB);
    addSprings (topology.nlA, topology.nlB);

    g.setColour (juce::Colour (0xff5d6b7a).withAlpha (0.7f));
    g.strokePath (springs, juce::PathStrokeType (0.8f));

    g.setColour (juce::Colour (0xffffa53d));
    g.strokePath (scanRing, juce::PathStrokeType (2.0f));

    // Pinned lumps.
    g.setColour (juce::Colour (0xff9aa7b4));
    for (std::uint32_t i = 0; i < topology.numLumps; ++i)
        if (topology.defaultConstraints[i] == Constraint::fixXY)
        {
            const auto p = displayed (i);
            g.fillEllipse (p.x - 1.5f, p.y - 1.5f, 3.0f, 3.0f);
        }

    // Grab patch: under the mouse while dragging, or where a click would grab.
    const float r = std::max (grabRadius * scale, 4.0f);
    std::optional<juce::Point<float>> ring;
    if (grabbedLump)
        ring = displayed (*grabbedLump);
    else if (hover)
        if (const auto lump = nearestLump (*hover))
            ring = displayed (*lump);

    if (ring)
    {
        const auto colour = juce::Colour (0xff7fd1ff);
        g.setColour (colour.withAlpha (grabbedLump ? 0.25f : 0.1f));
        g.fillEllipse (ring->x - r, ring->y - r, 2 * r, 2 * r);
        g.setColour (colour.withAlpha (grabbedLump ? 0.9f : 0.5f));
        g.drawEllipse (ring->x - r, ring->y - r, 2 * r, 2 * r, 1.2f);
    }
}

void MeshView::mouseMove (const juce::MouseEvent& e)
{
    hover = e.position;
    repaint();
}

void MeshView::mouseExit (const juce::MouseEvent&)
{
    hover.reset();
    repaint();
}

void MeshView::mouseDown (const juce::MouseEvent& e)
{
    endGrab();
    grabbedLump = nearestLump (e.position);
    if (! grabbedLump)
        return;

    grabMouseStart = e.position;
    if (onGrab)
        onGrab (*grabbedLump, grabRadius);
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    repaint();
}

void MeshView::mouseDrag (const juce::MouseEvent& e)
{
    hover = e.position;
    if (! grabbedLump)
        return;

    // Screen pixels -> mesh units, undoing the display exaggeration (y up).
    const float k = 1.0f / (scale * std::max (exaggeration, 1.0e-3f));
    if (onDrag)
        onDrag ((e.position.x - grabMouseStart.x) * k, (grabMouseStart.y - e.position.y) * k);
}

void MeshView::mouseUp (const juce::MouseEvent&)
{
    endGrab();
}

void MeshView::endGrab()
{
    if (! grabbedLump)
        return;

    grabbedLump.reset();
    if (onRelease)
        onRelease();
    setMouseCursor (juce::MouseCursor::NormalCursor);
    repaint();
}

} // namespace slink::plugin
