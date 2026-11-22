#include "slink/Voice.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace slink
{

namespace
{
    // Excitation levels relative to the noise hit (measured; see ToDo.md stage 1c).
    constexpr float sineScale   = 0.8f;
    constexpr float strikeScale = 2.0f;
}

Voice::Voice (Topology t) : topology (std::move (t))
{
    initState (topology, state);
    scan.prepare (topology);
    stepParams = resolve (params, physicsRate);
}

void Voice::prepare (double newSampleRate, double physicsRateHz)
{
    sampleRate = newSampleRate;
    physicsRate = std::clamp (physicsRateHz, 1.0, sampleRate);

    initState (topology, state);
    scan.prepare (topology);
    scan.setCrossfadeIncrement ((float) (physicsRate / sampleRate));
    highPass.setup (sampleRate, 20.0);
    stepParams = resolve (params, physicsRate);

    reset();
    phaseIncrement = 261.63 / sampleRate;
}

void Voice::setParams (const Params& p)
{
    params = p;
    stepParams = resolve (params, physicsRate);
}

void Voice::setPhysicsRate (double physicsRateHz) noexcept
{
    physicsRate = std::clamp (physicsRateHz, 1.0, sampleRate);
    scan.setCrossfadeIncrement ((float) (physicsRate / sampleRate));
    stepParams = resolve (params, physicsRate);
    stepPhase = std::min (stepPhase, sampleRate - physicsRate);   // keep the accumulator in range
}

void Voice::setPitch (int noteNumber) noexcept
{
    phaseIncrement = 440.0 * std::pow (2.0, (noteNumber - 69) / 12.0) / sampleRate;
}

void Voice::reset() noexcept
{
    resetState (topology, state);
    applyFreeEdges();
    scan.reset();
    highPass.reset();
    stepPhase = 0.0;
    phase = 0.0;
}

void Voice::noteOn (int noteNumber, float velocity) noexcept
{
    setPitch (noteNumber);

    // Same scaling as the reference model: 0.002 per MIDI velocity unit.
    const float amplitude = 0.002f * (velocity * 127.0f) * velocitySensitivity;

    switch (excitation)
    {
        case Excitation::noise:
            hitNoise (state, topology.scanPath, amplitude, random);
            break;

        case Excitation::sine:
        {
            // Odd harmonics of the unit square, so the displacement vanishes at its edges.
            const float a = amplitude * sineScale;
            const float wx = std::numbers::pi_v<float> * (float) (2 * std::max (harmonicX, 1) - 1);
            const float wy = std::numbers::pi_v<float> * (float) (2 * std::max (harmonicY, 1) - 1);
            const float ax = harmonicY > 0 ? a : 0.0f;
            for (std::uint32_t i = 0; i < topology.numLumps; ++i)
            {
                const float m = state.filterMask[i];   // free lumps only
                state.y[i] += m * a  * std::sin (wx * topology.restX[i]);
                state.x[i] += m * ax * std::sin (wy * topology.restY[i]);
            }
            break;
        }

        case Excitation::strike:
        {
            const float a = amplitude * strikeScale;
            const float r = std::max (strikeRadius, 1.0e-3f);
            for (std::uint32_t i = 0; i < topology.numLumps; ++i)
            {
                const float d = std::hypot (state.x[i] - strikeX, state.y[i] - strikeY);
                if (d < r)
                    state.y[i] += state.filterMask[i] * a * 0.5f * (1.0f + std::cos (std::numbers::pi_v<float> * d / r));
            }
            break;
        }
    }
}

void Voice::setFreeEdges (bool shouldBeFree) noexcept
{
    if (shouldBeFree == freeEdges)
        return;

    freeEdges = shouldBeFree;

    if (! freeEdges)
    {
        // Put the edge lumps back where they belong before pinning them there.
        for (std::uint32_t i = 0; i < topology.numLumps; ++i)
            if (topology.defaultConstraints[i] != Constraint::free)
            {
                state.x[i] = topology.restX[i];
                state.y[i] = topology.restY[i];
                state.vx[i] = state.vy[i] = 0.0f;
            }
    }

    applyFreeEdges();
}

void Voice::applyFreeEdges() noexcept
{
    for (std::uint32_t i = 0; i < topology.numLumps; ++i)
    {
        if (topology.defaultConstraints[i] == Constraint::free)
            continue;
        if (freeEdges)
        {
            setConstraint (state, i, Constraint::free);
            state.homeX[i] = topology.restX[i];   // displacement still measured from rest
            state.homeY[i] = topology.restY[i];
        }
        else
        {
            setConstraint (state, i, topology.defaultConstraints[i]);
        }
    }
}

void Voice::render (float* output, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        // Integer-valued accumulator: exact step timing for integer rates.
        stepPhase += physicsRate;
        if (stepPhase >= sampleRate)
        {
            stepPhase -= sampleRate;
            step (topology, state, stepParams);
            scan.capture (topology, state);
        }

        output[i] = highPass.process (scan.read (phase)) * volume;

        phase += phaseIncrement;
        if (phase >= 1.0)
            phase -= 1.0;
    }
}

void Voice::HighPass::setup (double sr, double cutoffHz) noexcept
{
    const double w = std::tan (std::numbers::pi * cutoffHz / sr);
    const double norm = 1.0 / (1.0 + std::numbers::sqrt2 * w + w * w);
    b0 = norm;
    b1 = -2.0 * norm;
    b2 = norm;
    a1 = 2.0 * (w * w - 1.0) * norm;
    a2 = (1.0 - std::numbers::sqrt2 * w + w * w) * norm;
    reset();
}

float Voice::HighPass::process (float in) noexcept
{
    const double x0 = in;
    const double y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1; x1 = x0;
    y2 = y1; y1 = y0;
    return (float) y0;
}

} // namespace slink
