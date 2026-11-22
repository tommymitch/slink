#pragma once

#include "State.h"

namespace slink
{

/** What the scan path reads from each lump (or ring spring) into the wavetable. */
enum class ScanMode
{
    displacement,   // distance from home (unsigned): the reference model's default
    speed,          // lump speed (unsigned)
    springForce,    // stretch of the spring to the next lump on the path (unsigned)
    yPosition       // y offset from home (signed)
};

/** Turns the shape of the scan path into a wavetable and reads it as audio.

    After each physics step, capture() measures every scan-path lump's
    displacement from home into a new table. read() plays the table at the
    oscillator phase, crossfading from the previous table to the new one so
    sub-audio physics rates don't click.

    (Not yet band-limited: see "Anti-aliased scan reading" in ToDo.md.) */
class ScanReader
{
public:
    /** Allocates the tables. */
    void prepare (const Topology& topology);

    /** How far the old -> new crossfade moves per output sample (1 = instant).
        Normally physicsRate / sampleRate, so a crossfade lasts one physics frame. */
    void setCrossfadeIncrement (float increment) noexcept { crossfadeIncrement = increment; }

    void reset() noexcept;

    void setMode (ScanMode m) noexcept { mode = m; }
    ScanMode getMode() const noexcept { return mode; }

    /** Captures the current mesh shape as the new table. Allocation-free. */
    void capture (const Topology& topology, const State& state) noexcept;

    /** One output sample at oscillator phase [0, 1). */
    float read (double phase) noexcept;

private:
    std::vector<float> oldTable, newTable;
    std::vector<float> restLength;   // rest distance from each path lump to the next
    ScanMode mode = ScanMode::displacement;
    float crossfade = 0.0f;
    float crossfadeIncrement = 1.0f;
};

} // namespace slink
