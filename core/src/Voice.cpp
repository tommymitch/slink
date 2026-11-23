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

    // Real-time smoothing of the grab position (seconds).
    constexpr double grabSmoothingTime = 0.005;

    float grabCoefficient (double physicsRate)
    {
        return (float) (1.0 - std::exp (-1.0 / (std::max (physicsRate, 1.0) * grabSmoothingTime)));
    }
}

Voice::Voice (Topology t) : topology (std::move (t))
{
    grabLump.assign (topology.numLumps, 0);
    grabWeight.assign (topology.numLumps, 0.0f);
    grabStartX.assign (topology.numLumps, 0.0f);
    grabStartY.assign (topology.numLumps, 0.0f);

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
    grabSmoothing = grabCoefficient (physicsRate);

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
    grabSmoothing = grabCoefficient (physicsRate);
    stepPhase = std::min (stepPhase, sampleRate - physicsRate);   // keep the accumulator in range
}

void Voice::setPitch (int noteNumber) noexcept
{
    phaseIncrement = 440.0 * std::pow (2.0, (noteNumber - 69) / 12.0) / sampleRate;
}

void Voice::reset() noexcept
{
    numGrabbed = 0;                 // resetState() rebuilds the masks the grab changed
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

    release();   // the grabbed set may include edge lumps whose constraint is about to change
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

void Voice::grab (std::uint32_t centre, float radius) noexcept
{
    release();
    if (centre >= topology.numLumps)
        return;

    const float cx = state.x[centre], cy = state.y[centre];
    const float r = std::max (radius, 1.0e-6f);
    std::uint32_t count = 0;

    for (std::uint32_t i = 0; i < topology.numLumps; ++i)
    {
        if (state.constraint[i] != Constraint::free)
            continue;
        const float d = std::hypot (state.x[i] - cx, state.y[i] - cy);
        if (d >= r)
            continue;
        const float w = 0.5f * (1.0f + std::cos (std::numbers::pi_v<float> * d / r));
        if (w < 0.02f)
            continue;

        grabLump[count] = i;
        grabWeight[count] = w;
        grabStartX[count] = state.x[i];
        grabStartY[count] = state.y[i];
        ++count;

        // Held by applyGrab(), not integrated; skipped by homing and smoothing.
        // (Only the masks change: the constraint stays free and home stays put.)
        state.moveMask[i] = 0.0f;
        state.filterMask[i] = 0.0f;
        state.vx[i] = state.vy[i] = 0.0f;
    }

    numGrabbed = count;
    grabTargetX = grabTargetY = grabOffsetX = grabOffsetY = grabPrevX = grabPrevY = 0.0f;
}

void Voice::dragTo (float dx, float dy) noexcept
{
    grabTargetX = dx;
    grabTargetY = dy;
}

void Voice::release() noexcept
{
    if (numGrabbed == 0)
        return;

    // Velocity units are such that x' = x + velDt * v.
    const float invVelDt = 1.0f / std::max (stepParams.velDt, 1.0e-9f);
    const float vx = (grabOffsetX - grabPrevX) * invVelDt;
    const float vy = (grabOffsetY - grabPrevY) * invVelDt;

    for (std::uint32_t k = 0; k < numGrabbed; ++k)
    {
        const auto i = grabLump[k];
        setConstraint (state, i, state.constraint[i]);   // restores the masks (free: home unchanged)
        state.vx[i] = grabWeight[k] * vx;
        state.vy[i] = grabWeight[k] * vy;
    }

    numGrabbed = 0;
}

void Voice::applyGrab() noexcept
{
    grabPrevX = grabOffsetX;
    grabPrevY = grabOffsetY;
    grabOffsetX += grabSmoothing * (grabTargetX - grabOffsetX);
    grabOffsetY += grabSmoothing * (grabTargetY - grabOffsetY);

    for (std::uint32_t k = 0; k < numGrabbed; ++k)
    {
        const auto i = grabLump[k];
        const float w = grabWeight[k];
        // Inside the walls, or step()'s wall reflection would fight the hold.
        state.x[i] = std::clamp (grabStartX[k] + w * grabOffsetX, 0.0f, 1.0f);
        state.y[i] = std::clamp (grabStartY[k] + w * grabOffsetY, 0.0f, 1.0f);
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
            if (numGrabbed > 0)
                applyGrab();
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
