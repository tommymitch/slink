#pragma once

#include "Physics.h"
#include "ScanReader.h"

namespace slink
{

/** How a note-on excites the mesh. */
enum class Excitation
{
    noise,    // random kick to every scan-path lump (the reference model's default)
    sine,     // spatial harmonic over the whole mesh: smooth, pitched start
    strike    // raised-cosine bump at a point (strikeX, strikeY)
};

/** One playable instance of a mesh: physics state, scan reader, oscillator and
    output filter. This is what a plugin's processBlock (or the benchmark) runs.

    prepare() allocates; everything else is real-time safe. */
class Voice
{
public:
    explicit Voice (Topology topology);

    /** Allocates and resets. physicsRateHz is clamped to the sample rate. */
    void prepare (double sampleRate, double physicsRateHz);

    void setParams (const Params& params);
    const Params& getParams() const noexcept { return params; }
    const StepParams& getStepParams() const noexcept { return stepParams; }

    /** Changes the physics rate without resetting the mesh. Real-time safe. */
    void setPhysicsRate (double physicsRateHz) noexcept;
    double getPhysicsRate() const noexcept { return physicsRate; }

    /** Puts the mesh back at rest. */
    void reset() noexcept;

    /** Sets the pitch and hits the scan path. velocity is 0..1. */
    void noteOn (int noteNumber, float velocity) noexcept;

    /** Changes the scan pitch without hitting the mesh (e.g. legato fall-back). */
    void setPitch (int noteNumber) noexcept;

    /** Unpins the mesh's pinned edges (true), or pins them again at their rest
        positions (false; the jump back excites the mesh). Kept across reset(). */
    void setFreeEdges (bool shouldBeFree) noexcept;
    bool getFreeEdges() const noexcept { return freeEdges; }

    void setScanMode (ScanMode m) noexcept { scan.setMode (m); }

    /** Touch: grabs the free lumps within `radius` of `lump`'s current position
        (the centre lump itself may be pinned; only free lumps are taken). They
        are held kinematically, with a raised-cosine falloff from the centre, and
        follow dragTo(). Replaces any earlier grab. */
    void grab (std::uint32_t lump, float radius) noexcept;

    /** Moves the grabbed patch by (dx, dy) mesh units from where it was grabbed.
        The motion is smoothed over a few milliseconds, so block-rate updates
        don't click. Positions are kept inside the unit square. */
    void dragTo (float dx, float dy) noexcept;

    /** Lets go. The patch keeps the velocity it was being moved with, so
        releasing a stretched mesh plucks it and a fast drag flings it. */
    void release() noexcept;

    bool isGrabbing() const noexcept { return numGrabbed > 0; }

    /** Renders mono audio. */
    void render (float* output, int numSamples) noexcept;

    float velocitySensitivity = 0.5f;   // as the reference model's "Velocity" default
    float volume = 1.0f;

    Excitation excitation = Excitation::noise;
    int   harmonicX = 1, harmonicY = 0;      // sine: displacement in y follows sin((2hx-1) pi x); hy = 0 means none in x
    float strikeX = 0.5f, strikeY = 0.7f;    // strike: centre, in mesh coordinates
    float strikeRadius = 0.1f;

    const Topology& getTopology() const noexcept { return topology; }
    const State& getState() const noexcept { return state; }
    State& getState() noexcept { return state; }

    /** For tests: override the crossfade increment chosen by prepare(). */
    ScanReader& getScanReader() noexcept { return scan; }

private:
    /** 2-pole Butterworth high-pass (removes the scan displacement's DC offset). */
    struct HighPass
    {
        void setup (double sampleRate, double cutoffHz) noexcept;
        void reset() noexcept { x1 = x2 = y1 = y2 = 0.0; }
        float process (float in) noexcept;

        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    };

    Topology topology;
    State state;
    ScanReader scan;
    HighPass highPass;
    Random random;

    bool freeEdges = false;
    void applyFreeEdges() noexcept;

    // Touch grab: sized for every lump at construction, so grabbing never allocates.
    std::vector<std::uint32_t> grabLump;
    std::vector<float> grabWeight, grabStartX, grabStartY;
    std::uint32_t numGrabbed = 0;
    float grabTargetX = 0, grabTargetY = 0;     // requested offset
    float grabOffsetX = 0, grabOffsetY = 0;     // smoothed offset, applied per step
    float grabPrevX = 0, grabPrevY = 0;         // offset at the previous step (release velocity)
    float grabSmoothing = 1.0f;                 // per-step one-pole coefficient
    void applyGrab() noexcept;

    Params params;
    StepParams stepParams {};

    double sampleRate = 48000.0;
    double physicsRate = referencePhysicsRateHz;
    double stepPhase = 0.0;      // counts up by physicsRate per sample; steps at sampleRate
    double phase = 0.0;
    double phaseIncrement = 0.0;
};

} // namespace slink
