#pragma once

#include "Topology.h"

namespace slink
{

/** Which shape a mesh starts in, and is measured from. */
enum class RestShape
{
    /** The shape the mesh actually rests in: settled to force balance at build
        time, so a mesh with no excitation stays perfectly still, and the scan
        reads displacement from true rest. */
    settled,

    /** The geometric shape as constructed. Meshes whose zero-length springs
        are in tension (like the spider-cross) are not at rest in this shape and
        drift towards equilibrium; this matches the reference model. */
    built
};

/** Moves every free lump of a Topology to its equilibrium (force-balance)
    position and makes that the rest shape. Spring rest lengths are left as
    built, so springs that are compressed or stretched at equilibrium stay so.

    The equilibrium depends only on the layout (all springs share one k and
    there is no gravity), so this is done once, at build time. */
void settleRestShape (Topology& topology);

/** A circular spider web: `rings` concentric rings of `spokes` lumps, with
    ring springs (natural length), spoke springs and both diagonals
    (zero length). The inner and outer rings are pinned, and the scan path
    runs round the middle ring.

    With RestShape::built the layout matches the reference model's
    SpiderCrossMesh exactly. Settled, the free rings sit about 20% closer to
    the centre (the zero-length diagonals pull them in). */
Topology makeSpiderCross (int spokes = 50, int rings = 15, RestShape shape = RestShape::settled);

} // namespace slink
