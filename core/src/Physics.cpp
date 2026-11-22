#include "slink/Physics.h"

#include <algorithm>
#include <cmath>
#include <type_traits>

namespace slink
{

namespace
{
    double stepDt (const Params& p, double physicsRateHz)
    {
        if (p.integrator == Integrator::legacy)
            return 0.5;
        return 0.5 * referencePhysicsRateHz / std::max (physicsRateHz, 1.0);
    }
}

float maxStableSpringConstant (const Params& p, double physicsRateHz) noexcept
{
    // Empirical limit on (k / m) * dt^2, measured by bisection on the 50x15
    // spider-cross with repeated maximum-velocity hits (see ToDo.md, stage 1b):
    //   legacy: ~0.3 with strong damping, lower with light damping; less
    //   smoothing lowers it (energy creeps up with no smoothing above ~0.1).
    //   time-scaled: falls as the rate rises (0.32 at 60 Hz, 0.16 at 1 kHz,
    //   0.044 at 48 kHz), because per-step damping and smoothing shrink.
    // These constants were validated over 720 cases (both modes, 30 Hz to
    // 48 kHz, k up to 1e7, m 0.1-10, smoothing 0-0.1, light and heavy damping).
    const double smoothingFactor = std::clamp ((double) p.smoothing / 0.01, 0.0, 1.0);
    double c = 0.1 + 0.1 * smoothingFactor;    // 0.2 at smoothing >= 0.01: exactly the k = 0.8 default
    if (p.integrator == Integrator::timeScaled)
        c *= std::min (1.0, std::pow (referencePhysicsRateHz / std::max (physicsRateHz, 1.0), 0.4));

    const double dt = stepDt (p, physicsRateHz);
    return (float) (std::max ((double) p.mass, 1.0e-6) * c / (dt * dt));
}

StepParams resolve (const Params& p, double physicsRateHz)
{
    const double legacyDamp = (1.0 + 3.0 * (double) p.friction) / 4.0;
    const double invMass = 1.0 / std::max ((double) p.mass, 1.0e-6);

    StepParams s {};
    s.springConstant = p.stabilityGuard ? std::min (p.springConstant, maxStableSpringConstant (p, physicsRateHz))
                                        : p.springConstant;
    s.accelScale = (float) ((double) s.springConstant * invMass);
    s.gravAccelX = (float) ((double) p.gravityX * invMass);
    s.gravAccelY = (float) ((double) p.gravityY * invMass);
    s.wallBounce = p.wallBounce;
    s.minCompression = p.minCompression;
    s.springsOn  = p.springsOn;

    // Homing is a force, so it is the same in both time modes and at every rate.
    // K <= 0.5 and C <= 1.42 stay well inside the stable range at dt = 0.5.
    s.homingK = 0.5f * std::clamp (p.homing, 0.0f, 1.0f);
    s.homingC = 2.0f * std::sqrt (s.homingK);

    if (p.integrator == Integrator::legacy)
    {
        s.damp      = (float) legacyDamp;
        s.accelDt   = 0.5f;
        s.velDt     = 0.5f;
        s.smoothing = p.smoothing;
    }
    else
    {
        // r steps of the reference model happen per step at this rate.
        const double r = referencePhysicsRateHz / std::max (physicsRateHz, 1.0);
        s.damp      = (float) std::pow (legacyDamp, r);
        s.accelDt   = (float) (0.5 * r);
        s.velDt     = (float) (0.5 * r);
        s.smoothing = (float) (1.0 - std::pow (1.0 - (double) p.smoothing, r));
    }

    return s;
}

void step (const Topology& t, State& s, const StepParams& p) noexcept
{
    const auto n = t.numLumps;

    float* __restrict x  = s.x.data();
    float* __restrict y  = s.y.data();
    float* __restrict vx = s.vx.data();
    float* __restrict vy = s.vy.data();
    float* __restrict fx = s.fx.data();
    float* __restrict fy = s.fy.data();

    // 1. Spring forces, accumulated without k (applied once per lump below).
    std::fill (fx, fx + n, 0.0f);
    std::fill (fy, fy + n, 0.0f);

    if (p.springsOn)
    {
        const auto* linA = t.linA.data();
        const auto* linB = t.linB.data();
        const auto numLin = t.linA.size();

        for (std::size_t i = 0; i < numLin; ++i)
        {
            const auto a = linA[i], b = linB[i];
            const float dx = x[b] - x[a];
            const float dy = y[b] - y[a];
            fx[a] += dx;  fy[a] += dy;
            fx[b] -= dx;  fy[b] -= dy;
        }

        const auto* nlA = t.nlA.data();
        const auto* nlB = t.nlB.data();
        const auto* rest = t.nlRest.data();
        const auto numNl = t.nlA.size();
        const float minComp = p.minCompression;

        for (std::size_t i = 0; i < numNl; ++i)
        {
            const auto a = nlA[i], b = nlB[i];
            const float dx = x[b] - x[a];
            const float dy = y[b] - y[a];
            const float len = std::sqrt (dx * dx + dy * dy);
            // ff = rest / len - 1 is the (negated) force factor on b. Optionally
            // capped below minCompression * rest (stiffness grows without bound as
            // a spring collapses).
            const float ff = rest[i] / std::max (len, minComp * rest[i] + 1.0e-12f) - 1.0f;
            fx[b] += ff * dx;  fy[b] += ff * dy;
            fx[a] -= ff * dx;  fy[a] -= ff * dy;
        }
    }

    // 2. Integrate (symplectic Euler) and bounce off the walls of the unit square.
    {
        const auto* moveMask = s.moveMask.data();
        const auto* maskX = s.accelMaskX.data();
        const auto* maskY = s.accelMaskY.data();
        const float k = p.accelScale, gx = p.gravAccelX, gy = p.gravAccelY;
        const float damp = p.damp, aDt = p.accelDt, vDt = p.velDt, bounce = p.wallBounce;
        const float hK = p.homingK, hC = p.homingC;
        const auto* hx = s.homeX.data();
        const auto* hy = s.homeY.data();
        const auto* freeMask = s.filterMask.data();

        // Compiled twice, so homing costs nothing when it is off.
        auto integrate = [&] (auto withHoming)
        {
        for (std::uint32_t i = 0; i < n; ++i)
        {
            // Homing: a critically damped spring to home, on free lumps only. (The
            // reference model blended position towards home instead; where springs
            // or gravity hold a lump away from home that leaves a constant velocity
            // that never decays. As a force, it comes properly to rest.)
            float ax = fx[i] * k + gx, ay = fy[i] * k + gy;
            if constexpr (decltype (withHoming)::value)
            {
                const float hm = hK * freeMask[i], hd = hC * freeMask[i];
                ax += hm * (hx[i] - x[i]) - hd * vx[i];
                ay += hm * (hy[i] - y[i]) - hd * vy[i];
            }
            ax *= maskX[i];
            ay *= maskY[i];
            const float m = moveMask[i];

            const float nvx = damp * vx[i] + aDt * ax;
            const float nvy = damp * vy[i] + aDt * ay;
            float px = x[i] + m * vDt * nvx;
            float py = y[i] + m * vDt * nvy;
            float pvx = vx[i] + m * (nvx - vx[i]);
            float pvy = vy[i] + m * (nvy - vy[i]);

            const bool loX = px < 0.0f, hiX = px > 1.0f;
            const bool loY = py < 0.0f, hiY = py > 1.0f;
            px  = loX ? -px : (hiX ? 2.0f - px : px);
            py  = loY ? -py : (hiY ? 2.0f - py : py);
            pvx = (loX || hiX) ? -pvx * bounce : pvx;
            pvy = (loY || hiY) ? -pvy * bounce : pvy;

            x[i] = px;  y[i] = py;
            vx[i] = pvx; vy[i] = pvy;
        }
        };

        if (hK > 0.0f)
            integrate (std::true_type {});
        else
            integrate (std::false_type {});
    }

    const auto* filterMask = s.filterMask.data();

    // 3. Smoothing: mix each free lump towards the average of its neighbours.
    //    Averages are computed for every lump first, then applied (Jacobi), as
    //    in the reference model.
    if (p.smoothing > 0.0f)
    {
        float* __restrict ax = s.tmpX.data();
        float* __restrict ay = s.tmpY.data();
        const auto* start = t.nbrStart.data();
        const auto* idx = t.nbrIndex.data();
        const auto* invCount = t.nbrInvCount.data();

        for (std::uint32_t i = 0; i < n; ++i)
        {
            float sx = 0.0f, sy = 0.0f;
            for (auto j = start[i]; j < start[i + 1]; ++j)
            {
                sx += x[idx[j]];
                sy += y[idx[j]];
            }
            ax[i] = sx * invCount[i];
            ay[i] = sy * invCount[i];
        }

        const auto* offX = t.smoothOffsetX.data();
        const auto* offY = t.smoothOffsetY.data();
        const float sm = p.smoothing;
        for (std::uint32_t i = 0; i < n; ++i)
        {
            const float amt = sm * filterMask[i] * (invCount[i] > 0.0f ? 1.0f : 0.0f);
            x[i] += amt * (ax[i] - x[i] - offX[i]);
            y[i] += amt * (ay[i] - y[i] - offY[i]);
        }
    }
}

void hitNoise (State& s, const std::vector<std::uint32_t>& lumps, float amplitude, Random& random) noexcept
{
    for (auto i : lumps)
    {
        const float m = s.moveMask[i];
        s.x[i] += m * amplitude * random.bipolar();
        s.y[i] += m * amplitude * random.bipolar();
    }
}

} // namespace slink
