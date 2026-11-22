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

    Params params;
    StepParams stepParams {};

    double sampleRate = 48000.0;
    double physicsRate = referencePhysicsRateHz;
    double stepPhase = 0.0;      // counts up by physicsRate per sample; steps at sampleRate
    double phase = 0.0;
    double phaseIncrement = 0.0;
};

} // namespace slink
