#include "slink/State.h"

#include <algorithm>
#include <cassert>

namespace slink
{

void initState (const Topology& topology, State& s)
{
    const auto n = (std::size_t) topology.numLumps;

    for (auto* v : { &s.x, &s.y, &s.vx, &s.vy, &s.fx, &s.fy, &s.tmpX, &s.tmpY, &s.homeX, &s.homeY,
                     &s.moveMask, &s.accelMaskX, &s.accelMaskY, &s.filterMask })
        v->assign (n, 0.0f);

    s.constraint.assign (n, Constraint::free);

    resetState (topology, s);
}

void resetState (const Topology& topology, State& s)
{
    assert (s.x.size() == topology.numLumps);

    std::copy (topology.restX.begin(), topology.restX.end(), s.x.begin());
    std::copy (topology.restY.begin(), topology.restY.end(), s.y.begin());
    std::copy (topology.restX.begin(), topology.restX.end(), s.homeX.begin());
    std::copy (topology.restY.begin(), topology.restY.end(), s.homeY.begin());
    std::fill (s.vx.begin(), s.vx.end(), 0.0f);
    std::fill (s.vy.begin(), s.vy.end(), 0.0f);

    for (std::uint32_t i = 0; i < topology.numLumps; ++i)
        setConstraint (s, i, topology.defaultConstraints[i]);
}

void setConstraint (State& s, std::uint32_t i, Constraint c)
{
    s.constraint[i] = c;
    s.moveMask[i]   = c == Constraint::fixXY ? 0.0f : 1.0f;
    s.accelMaskX[i] = c == Constraint::fixX  ? 0.0f : 1.0f;
    s.accelMaskY[i] = c == Constraint::fixY  ? 0.0f : 1.0f;
    s.filterMask[i] = c == Constraint::free  ? 1.0f : 0.0f;

    if (c != Constraint::free)
    {
        s.homeX[i] = s.x[i];
        s.homeY[i] = s.y[i];
    }
}

void releaseAllConstraints (State& s)
{
    for (std::uint32_t i = 0; i < (std::uint32_t) s.x.size(); ++i)
        setConstraint (s, i, Constraint::free);
}

} // namespace slink
