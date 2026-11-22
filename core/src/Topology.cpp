#include "slink/Topology.h"

#include <cassert>
#include <cmath>

namespace slink
{

std::uint32_t TopologyBuilder::addLump (double x, double y, Constraint c)
{
    posX.push_back (x);
    posY.push_back (y);
    topology.restX.push_back ((float) x);
    topology.restY.push_back ((float) y);
    topology.defaultConstraints.push_back (c);
    return topology.numLumps++;
}

void TopologyBuilder::addSpring (std::uint32_t a, std::uint32_t b)
{
    assert (a < topology.numLumps && b < topology.numLumps && a != b);
    springs.push_back ({ a, b, false });
}

void TopologyBuilder::addZeroLengthSpring (std::uint32_t a, std::uint32_t b)
{
    assert (a < topology.numLumps && b < topology.numLumps && a != b);
    springs.push_back ({ a, b, true });
}

void TopologyBuilder::setConstraint (std::uint32_t lump, Constraint c)
{
    assert (lump < topology.numLumps);
    topology.defaultConstraints[lump] = c;
}

void TopologyBuilder::addToScanPath (std::uint32_t lump)
{
    assert (lump < topology.numLumps);
    topology.scanPath.push_back (lump);
}

Topology TopologyBuilder::finalise()
{
    auto& t = topology;

    for (const auto& s : springs)
    {
        if (s.zeroLength)
        {
            t.linA.push_back (s.a);
            t.linB.push_back (s.b);
        }
        else
        {
            t.nlA.push_back (s.a);
            t.nlB.push_back (s.b);
            t.nlRest.push_back ((float) std::hypot (posX[s.b] - posX[s.a], posY[s.b] - posY[s.a]));
        }
    }

    // Neighbour lists (CSR), one entry per attached spring, in spring order.
    std::vector<std::uint32_t> counts (t.numLumps, 0);
    for (const auto& s : springs)
    {
        ++counts[s.a];
        ++counts[s.b];
    }

    t.nbrStart.assign (t.numLumps + 1, 0);
    for (std::uint32_t i = 0; i < t.numLumps; ++i)
        t.nbrStart[i + 1] = t.nbrStart[i] + counts[i];

    t.nbrIndex.assign (t.nbrStart.back(), 0);
    std::vector<std::uint32_t> fill (t.nbrStart.begin(), t.nbrStart.end() - 1);
    for (const auto& s : springs)
    {
        t.nbrIndex[fill[s.a]++] = s.b;
        t.nbrIndex[fill[s.b]++] = s.a;
    }

    t.nbrInvCount.resize (t.numLumps);
    for (std::uint32_t i = 0; i < t.numLumps; ++i)
        t.nbrInvCount[i] = counts[i] > 0 ? 1.0f / (float) counts[i] : 0.0f;

    t.smoothOffsetX.assign (t.numLumps, 0.0f);
    t.smoothOffsetY.assign (t.numLumps, 0.0f);

    Topology result = std::move (t);
    *this = TopologyBuilder();
    return result;
}

} // namespace slink
