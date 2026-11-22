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
    float homing         = 0.0f;      // 0..1: critically damped pull towards home ("still"); 1 stops the mesh in a few reference steps
    float smoothing      = 0.01f;     // 0..1 averaging-filter amount per step
    float wallBounce     = 0.5f;      // velocity kept when bouncing off the [0, 1] walls
    float gravityX       = 0.0f;
    float gravityY       = 0.0f;
    bool  springsOn      = true;      // false = "drift" (springs exert no force)
    Integrator integrator = Integrator::legacy;
    bool  stabilityGuard = true;      // clamp k to maxStableSpringConstant() (off only for experiments)
    float minCompression = 0.0f;      // >0: springs stop stiffening when crushed below this fraction of rest length (0 = off)
};

/** Params resolved for a particular physics rate: the per-step coefficients
    that step() actually uses. */
struct StepParams
{
    float springConstant;  // k actually used, after the stability guard
    float accelScale;    // k / m: spring sums are accumulated without k
    float gravAccelX, gravAccelY;
    float damp;          // velocity multiplier per step
    float accelDt;       // a -> v
    float velDt;         // v -> x
    float homingK;       // homing spring (acceleration per unit displacement)
    float homingC;       // homing damping (critical: 2 sqrt(homingK))
    float smoothing;
    float wallBounce;
    float minCompression;
    bool  springsOn;
};

/** The physics rate at which `timeScaled` matches `legacy` exactly. */
inline constexpr double referencePhysicsRateHz = 60.0;

/** The largest spring constant that stays stable for these params at this
    physics rate (measured on the 50x15 spider-cross with maximum-velocity hits).

    Symplectic Euler is stable while the mesh's fastest vibration is slow
    compared with the time step, so the limit scales with mass / dt^2. At the
    reference step (dt = 0.5) it is k/m = 0.8 with smoothing >= 0.01, falling
    to 0.5 with no smoothing (smoothing damps the fastest modes). */
float maxStableSpringConstant (const Params& params, double physicsRateHz) noexcept;

/** Resolves params for a physics rate. The spring constant is clamped to
    maxStableSpringConstant(), so no parameter combination can blow up. */
StepParams resolve (const Params& params, double physicsRateHz);

/** Advances the mesh by one physics step. Allocation-free.
    Order (as in the reference model): spring forces -> integrate (with gravity
    and homing forces) + walls -> smoothing. */
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
