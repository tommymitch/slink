#pragma once

#include "Topology.h"

namespace slink
{

/** Everything about a mesh that changes as it moves. One State per voice;
    it shares its Topology with any other voices on the same mesh.

    All arrays have one entry per lump. They are allocated by initState() and
    never resized afterwards, so stepping is allocation-free. */
struct State
{
    std::vector<float> x, y;          // position
    std::vector<float> vx, vy;        // velocity
    std::vector<float> fx, fy;        // force accumulators (scratch for step())
    std::vector<float> tmpX, tmpY;    // scratch for the smoothing filter

    // "Home" position each lump is measured from (scan displacement) and pulled
    // towards (homing). Starts at the rest position; reset when constrained.
    std::vector<float> homeX, homeY;

    std::vector<Constraint> constraint;

    // Per-lump multipliers derived from `constraint`, so step() can multiply
    // instead of branching:
    std::vector<float> moveMask;      // 0 = pinned (fixXY), 1 = integrates
    std::vector<float> accelMaskX;    // 0 = fixX
    std::vector<float> accelMaskY;    // 0 = fixY
    std::vector<float> filterMask;    // 0 = any constraint (homing/smoothing skip it)
};

/** Allocates the State for a Topology and resets it. */
void initState (const Topology& topology, State& state);

/** Puts every lump back at rest with zero velocity and the default
    constraints. Does not allocate. */
void resetState (const Topology& topology, State& state);

/** Changes one lump's constraint. As in the reference model, constraining a
    lump makes its current position its new home. */
void setConstraint (State& state, std::uint32_t lump, Constraint c);

/** Frees every lump. */
void releaseAllConstraints (State& state);

} // namespace slink
