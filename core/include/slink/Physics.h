#pragma once

#include "State.h"

#include <cstdint>

namespace slink
{

/** How a physics step relates to real time.

    The reference model's "Heun" update works out algebraically to symplectic
    Euler with a fixed time step of 0.5 and damping (1 + 3 * friction) / 4:

        v' = damp * v + 0.5 * a
        x' = x + 0.5 * v'

    Both modes use that same integrator; they differ only in what one step means. */
enum class Integrator : std::uint8_t
{
    /** Every step advances the mesh by 0.5 time units, as in the reference model.
        Raising the physics rate speeds the mesh up proportionally (at 48 kHz it
        moves 800x faster than at 60 Hz). */
    legacy,

    /** The time step, damping and filter amounts are scaled with the physics
        rate, so the mesh moves at the same real-world speed at any rate (matching
        the reference model's speed at 60 Hz). A higher rate is then just a more
        accurate simulation. */
    timeScaled
};

/** User-facing physics settings. Defaults match the reference model's plugin. */
struct Params
{
    float springConstant = 0.8f;      // k
    float mass           = 1.0f;
    float friction       = 0.99991f;  // per-step velocity retention, as in the reference model (at 60 Hz)
    float homing         = 0.0f;      // 0..1 pull towards home per step ("still")
    float smoothing      = 0.01f;     // 0..1 averaging-filter amount per step
    float wallBounce     = 0.5f;      // velocity kept when bouncing off the [0, 1] walls
    float gravityX       = 0.0f;
    float gravityY       = 0.0f;
    bool  springsOn      = true;      // false = "drift" (springs exert no force)
    Integrator integrator = Integrator::legacy;
};

/** Params resolved for a particular physics rate: the per-step coefficients
    that step() actually uses. */
struct StepParams
{
    float accelScale;    // k / m: spring sums are accumulated without k
    float gravAccelX, gravAccelY;
    float damp;          // velocity multiplier per step
    float accelDt;       // a -> v
    float velDt;         // v -> x
    float homing;
    float smoothing;
    float wallBounce;
    bool  springsOn;
};

/** The physics rate at which `timeScaled` matches `legacy` exactly. */
inline constexpr double referencePhysicsRateHz = 60.0;

StepParams resolve (const Params& params, double physicsRateHz);

/** Advances the mesh by one physics step. Allocation-free.
    Order (as in the reference model): spring forces -> integrate + walls ->
    homing -> smoothing. */
void step (const Topology& topology, State& state, const StepParams& p) noexcept;

/** Small, fast, deterministic random number generator (real-time safe). */
struct Random
{
    std::uint32_t seed = 0x9e3779b9u;

    /** Uniform in [-1, 1). */
    float bipolar() noexcept
    {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        return (float) (seed >> 8) * (2.0f / 16777216.0f) - 1.0f;
    }
};

/** Excites the mesh: adds uniform noise of up to +/- amplitude to the x and y
    position of each free lump in `lumps` (normally the scan path). */
void hitNoise (State& state, const std::vector<std::uint32_t>& lumps, float amplitude, Random& random) noexcept;

} // namespace slink
