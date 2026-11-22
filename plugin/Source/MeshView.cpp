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

void MeshView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff15171a));

    const auto& rx = topology.restX;
    const auto& ry = topology.restY;
    const bool live = snapshot.hasData();
    const auto& frame = snapshot.latest();
    const float e = exaggeration;

    auto pos = [&] (std::uint32_t i)
    {
        if (! live)
            return toScreen (rx[i], ry[i]);
        return toScreen (rx[i] + e * (frame.x[i] - rx[i]), ry[i] + e * (frame.y[i] - ry[i]));
    };

    juce::Path springs, scanRing;
    auto addSprings = [&] (const std::vector<std::uint32_t>& a, const std::vector<std::uint32_t>& b)
    {
        for (size_t k = 0; k < a.size(); ++k)
        {
            const auto p = pos (a[k]), q = pos (b[k]);
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
            const auto p = pos (i);
            g.fillEllipse (p.x - 1.5f, p.y - 1.5f, 3.0f, 3.0f);
        }
}

} // namespace slink::plugin
