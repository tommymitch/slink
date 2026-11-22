#include "slink/Physics.h"

#include <algorithm>
#include <cmath>

namespace slink
{

StepParams resolve (const Params& p, double physicsRateHz)
{
    const double legacyDamp = (1.0 + 3.0 * (double) p.friction) / 4.0;
    const double invMass = 1.0 / std::max ((double) p.mass, 1.0e-6);

    StepParams s {};
    s.accelScale = (float) ((double) p.springConstant * invMass);
    s.gravAccelX = (float) ((double) p.gravityX * invMass);
    s.gravAccelY = (float) ((double) p.gravityY * invMass);
    s.wallBounce = p.wallBounce;
    s.springsOn  = p.springsOn;

    if (p.integrator == Integrator::legacy)
    {
        s.damp      = (float) legacyDamp;
        s.accelDt   = 0.5f;
        s.velDt     = 0.5f;
        s.homing    = p.homing;
        s.smoothing = p.smoothing;
    }
    else
    {
        // r steps of the reference model happen per step at this rate.
        const double r = referencePhysicsRateHz / std::max (physicsRateHz, 1.0);
        s.damp      = (float) std::pow (legacyDamp, r);
        s.accelDt   = (float) (0.5 * r);
        s.velDt     = (float) (0.5 * r);
        s.homing    = (float) (1.0 - std::pow (1.0 - (double) p.homing, r));
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

        for (std::size_t i = 0; i < numNl; ++i)
        {
            const auto a = nlA[i], b = nlB[i];
            const float dx = x[b] - x[a];
            const float dy = y[b] - y[a];
            const float len = std::sqrt (dx * dx + dy * dy);
            // ff = rest / len - 1 is the (negated) force factor on b; guard len -> 0.
            const float ff = rest[i] / std::max (len, 1.0e-12f) - 1.0f;
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

        for (std::uint32_t i = 0; i < n; ++i)
        {
            const float ax = (fx[i] * k + gx) * maskX[i];
            const float ay = (fy[i] * k + gy) * maskY[i];
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
    }

    const auto* filterMask = s.filterMask.data();

    // 3. Homing: pull free lumps towards home.
    if (p.homing > 0.0f)
    {
        const auto* hx = s.homeX.data();
        const auto* hy = s.homeY.data();
        const float h = p.homing;

        for (std::uint32_t i = 0; i < n; ++i)
        {
            const float amt = h * filterMask[i];
            x[i] += amt * (hx[i] - x[i]);
            y[i] += amt * (hy[i] - y[i]);
        }
    }

    // 4. Smoothing: mix each free lump towards the average of its neighbours.
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
