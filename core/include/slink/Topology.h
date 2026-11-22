#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace slink
{

/** How a lump is held in place. */
enum class Constraint : std::uint8_t
{
    free,    // moves freely
    fixX,    // no acceleration in x
    fixY,    // no acceleration in y
    fixXY    // pinned: does not move at all
};

/** The fixed structure of a mesh: which lumps exist, how springs connect them
    and where the scan path runs. Built once by a mesh builder, then read-only,
    so several States (voices) can share one Topology.

    Everything is stored as flat arrays indexed by lump or spring number
    (structure-of-arrays), which keeps the hot loops in step() simple and
    cache-friendly. */
struct Topology
{
    std::uint32_t numLumps = 0;

    // Initial (rest) position of every lump; used to initialise and reset State.
    std::vector<float> restX, restY;

    // Zero-rest-length springs: the force is linear, F = k * (b - a), so no sqrt.
    std::vector<std::uint32_t> linA, linB;

    // Springs with a natural rest length: F = k * (rest / length - 1) * (b - a).
    std::vector<std::uint32_t> nlA, nlB;
    std::vector<float> nlRest;

    // Neighbours of each lump in CSR form: lump i's neighbours are
    // nbrIndex[nbrStart[i] .. nbrStart[i + 1]). One entry per attached spring.
    std::vector<std::uint32_t> nbrStart, nbrIndex;
    std::vector<float> nbrInvCount;   // 1 / number of neighbours (0 if none)

    // Where each lump sits relative to its neighbours' average when at rest.
    // The smoothing filter subtracts this, so it smooths vibration (displacement
    // from rest) and does nothing to a mesh at rest. Zero for a built (unsettled)
    // shape, which reproduces the reference model's absolute-position smoothing.
    std::vector<float> smoothOffsetX, smoothOffsetY;

    // Constraints applied when a State is initialised or reset.
    std::vector<Constraint> defaultConstraints;

    // Lumps read by the scan path, in order around the path.
    std::vector<std::uint32_t> scanPath;

    std::size_t numSprings() const noexcept { return linA.size() + nlA.size(); }
};

/** Assembles a Topology. Mesh builders add lumps and springs in any order;
    finalise() sorts springs into the linear / non-linear arrays and builds the
    neighbour lists. */
class TopologyBuilder
{
public:
    /** Positions are taken in double so rest lengths are computed precisely. */
    std::uint32_t addLump (double x, double y, Constraint c = Constraint::free);

    /** A spring whose rest length is the current distance between a and b. */
    void addSpring (std::uint32_t a, std::uint32_t b);

    /** A spring with zero rest length (always in tension). */
    void addZeroLengthSpring (std::uint32_t a, std::uint32_t b);

    void setConstraint (std::uint32_t lump, Constraint c);
    void addToScanPath (std::uint32_t lump);

    Topology finalise();

private:
    struct SpringDef { std::uint32_t a, b; bool zeroLength; };

    Topology topology;
    std::vector<double> posX, posY;
    std::vector<SpringDef> springs;
};

} // namespace slink
