#include "slink/Voice.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace slink
{

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

void Voice::reset() noexcept
{
    resetState (topology, state);
    scan.reset();
    highPass.reset();
    stepPhase = 0.0;
    phase = 0.0;
}

void Voice::noteOn (int noteNumber, float velocity) noexcept
{
    phaseIncrement = 440.0 * std::pow (2.0, (noteNumber - 69) / 12.0) / sampleRate;

    // Same scaling as the reference model: 0.002 per MIDI velocity unit.
    const float amplitude = 0.002f * (velocity * 127.0f) * velocitySensitivity;
    hitNoise (state, topology.scanPath, amplitude, random);
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
