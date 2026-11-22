#include "slink/MeshBuilders.h"
#include "slink/Physics.h"
#include "slink/State.h"

#include <cassert>
#include <cmath>
#include <numbers>

namespace slink
{

int settleRestShapeImpl (Topology& t, double damp, int maxSteps)
{
    // Damped dynamics in double precision with the same spring model as step()
    // (k = m = 1: the equilibrium doesn't depend on them), no filters or walls.
    // Converges when the largest net force on any movable lump is ~0.
    const auto n = (std::size_t) t.numLumps;
    std::vector<double> x (t.restX.begin(), t.restX.end()), y (t.restY.begin(), t.restY.end());
    std::vector<double> vx (n, 0.0), vy (n, 0.0), fx (n), fy (n);
    std::vector<double> maskX (n), maskY (n);

    for (std::size_t i = 0; i < n; ++i)
    {
        const auto c = t.defaultConstraints[i];
        maskX[i] = (c == Constraint::fixX || c == Constraint::fixXY) ? 0.0 : 1.0;
        maskY[i] = (c == Constraint::fixY || c == Constraint::fixXY) ? 0.0 : 1.0;
    }

    constexpr double forceTolerance = 1.0e-12;
    int steps = 0;

    for (; steps < maxSteps; ++steps)
    {
        std::fill (fx.begin(), fx.end(), 0.0);
        std::fill (fy.begin(), fy.end(), 0.0);

        for (std::size_t i = 0; i < t.linA.size(); ++i)
        {
            const auto a = t.linA[i], b = t.linB[i];
            const double dx = x[b] - x[a], dy = y[b] - y[a];
            fx[a] += dx; fy[a] += dy; fx[b] -= dx; fy[b] -= dy;
        }
        for (std::size_t i = 0; i < t.nlA.size(); ++i)
        {
            const auto a = t.nlA[i], b = t.nlB[i];
            const double dx = x[b] - x[a], dy = y[b] - y[a];
            const double ff = (double) t.nlRest[i] / std::max (std::sqrt (dx * dx + dy * dy), 1.0e-15) - 1.0;
            fx[b] += ff * dx; fy[b] += ff * dy; fx[a] -= ff * dx; fy[a] -= ff * dy;
        }

        double maxForce = 0.0;
        for (std::size_t i = 0; i < n; ++i)
            maxForce = std::max ({ maxForce, std::abs (fx[i] * maskX[i]), std::abs (fy[i] * maskY[i]) });
        if (maxForce < forceTolerance)
            break;

        for (std::size_t i = 0; i < n; ++i)
        {
            vx[i] = damp * vx[i] + 0.5 * fx[i] * maskX[i];
            vy[i] = damp * vy[i] + 0.5 * fy[i] * maskY[i];
            x[i] += 0.5 * vx[i];
            y[i] += 0.5 * vy[i];
        }
    }

    for (std::size_t i = 0; i < n; ++i)
    {
        t.restX[i] = (float) x[i];
        t.restY[i] = (float) y[i];
    }

    // Rest offset from the neighbour average, so smoothing is zero at rest.
    for (std::size_t i = 0; i < n; ++i)
    {
        double ax = 0.0, ay = 0.0;
        for (auto j = t.nbrStart[i]; j < t.nbrStart[i + 1]; ++j)
        {
            ax += x[t.nbrIndex[j]];
            ay += y[t.nbrIndex[j]];
        }
        const double inv = t.nbrInvCount[i];
        t.smoothOffsetX[i] = inv > 0.0 ? (float) (ax * inv - x[i]) : 0.0f;
        t.smoothOffsetY[i] = inv > 0.0 ? (float) (ay * inv - y[i]) : 0.0f;
    }

    return steps;
}

void settleRestShape (Topology& topology)
{
    settleRestShapeImpl (topology, 0.9, 200000);
}

Topology makeSpiderCross (int spokes, int rings, RestShape shape)
{
    assert (spokes >= 3 && rings >= 2);

    TopologyBuilder b;
    const auto S = (std::uint32_t) spokes;
    const auto R = (std::uint32_t) rings;
    auto lump = [S] (std::uint32_t ring, std::uint32_t spoke) { return ring * S + spoke; };

    // Lumps, ring by ring from the centre; spoke 0 points right.
    const double angleStep = 2.0 * std::numbers::pi / spokes;
    const double ringSpacing = 1.0 / (2.0 * rings);

    for (std::uint32_t ring = 0; ring < R; ++ring)
        for (std::uint32_t spoke = 0; spoke < S; ++spoke)
        {
            const double radius = ringSpacing * (ring + 1);
            b.addLump (radius * std::cos (angleStep * spoke) + 0.5,
                       radius * std::sin (angleStep * spoke) + 0.5);
        }

    // Ring springs (natural length), each ring closed on itself.
    for (std::uint32_t ring = 0; ring < R; ++ring)
        for (std::uint32_t spoke = 0; spoke < S; ++spoke)
            b.addSpring (lump (ring, spoke), lump (ring, (spoke + 1) % S));

    // Spokes (zero length), ring to next ring out.
    for (std::uint32_t ring = 0; ring + 1 < R; ++ring)
        for (std::uint32_t spoke = 0; spoke < S; ++spoke)
            b.addZeroLengthSpring (lump (ring, spoke), lump (ring + 1, spoke));

    // "/" diagonals (zero length): spoke s to spoke s + 1 on the next ring out.
    for (std::uint32_t ring = 0; ring + 1 < R; ++ring)
        for (std::uint32_t spoke = 0; spoke < S; ++spoke)
            b.addZeroLengthSpring (lump (ring, spoke), lump (ring + 1, (spoke + 1) % S));

    // "\" diagonals (zero length): spoke s to spoke s - 1 on the next ring out.
    for (std::uint32_t ring = 0; ring + 1 < R; ++ring)
        for (std::uint32_t spoke = 0; spoke < S; ++spoke)
            b.addZeroLengthSpring (lump (ring, spoke), lump (ring + 1, (spoke + S - 1) % S));

    // Pin the inner and outer rings.
    for (std::uint32_t spoke = 0; spoke < S; ++spoke)
    {
        b.setConstraint (lump (0, spoke), Constraint::fixXY);
        b.setConstraint (lump (R - 1, spoke), Constraint::fixXY);
    }

    // Scan path round the middle ring.
    for (std::uint32_t spoke = 0; spoke < S; ++spoke)
        b.addToScanPath (lump (R / 2, spoke));

    auto topology = b.finalise();

    if (shape == RestShape::settled)
        settleRestShape (topology);

    return topology;
}

} // namespace slink
