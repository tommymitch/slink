// SlinkCore unit tests. No framework: each test is a function; CHECK records failures.

#include "slink/MeshBuilders.h"
#include "slink/Physics.h"
#include "slink/Voice.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

namespace
{

int failures = 0;

#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (! (cond)) { std::printf ("  FAILED: %s  (%s:%d)\n", #cond, __FILE__, __LINE__); ++failures; } \
    } while (false)

bool allFinite (const std::vector<float>& v)
{
    for (auto f : v)
        if (! std::isfinite (f))
            return false;
    return true;
}

void spiderCrossLayout()
{
    const auto t = slink::makeSpiderCross (50, 15);

    CHECK (t.numLumps == 750);
    CHECK (t.numSprings() == 2850);
    CHECK (t.linA.size() == 2100);   // spokes + both diagonals
    CHECK (t.nlA.size() == 750);     // rings
    CHECK (t.scanPath.size() == 50);
    CHECK (t.scanPath.front() == 350 && t.scanPath.back() == 399);   // ring 7

    int pinned = 0;
    for (auto c : t.defaultConstraints)
        pinned += c == slink::Constraint::fixXY ? 1 : 0;
    CHECK (pinned == 100);           // inner and outer rings

    CHECK (t.nbrIndex.size() == 2 * t.numSprings());
    auto degree = [&] (std::uint32_t i) { return t.nbrStart[i + 1] - t.nbrStart[i]; };
    CHECK (degree (0) == 5);         // inner ring: 2 ring + 1 spoke + 2 diagonals
    CHECK (degree (360) == 8);       // middle: 2 ring + 2 spokes + 4 diagonals
    CHECK (degree (749) == 5);       // outer ring

    // Ring springs keep their natural length: chord of the ring.
    const double expected = 2.0 * (1.0 / 30.0) * std::sin (3.14159265358979 / 50.0);
    CHECK (std::abs (t.nlRest[0] - expected) < 1.0e-6);
}

void settledShapeIsInBalance()
{
    const auto t = slink::makeSpiderCross();
    slink::State s;
    slink::initState (t, s);

    slink::Params params;
    params.smoothing = 0.0f;
    slink::step (t, s, slink::resolve (params, 60.0));

    float maxForce = 0.0f;
    for (std::uint32_t i = 0; i < t.numLumps; ++i)
        if (t.defaultConstraints[i] == slink::Constraint::free)
            maxForce = std::max ({ maxForce, std::abs (s.fx[i]), std::abs (s.fy[i]) });
    CHECK (maxForce < 1.0e-5f);

    // The free rings settle ~20% inwards, evenly.
    const auto r = std::hypot (t.restX[375] - 0.5f, t.restY[375] - 0.5f);
    CHECK (std::abs (r - 0.21465f) < 1.0e-4f);
}

void startsAtStandstill()
{
    for (auto shape : { slink::RestShape::settled, slink::RestShape::built })
    {
        slink::Voice v (slink::makeSpiderCross (50, 15, shape));
        v.prepare (48000.0, 1000.0);

        std::vector<float> out (480);
        float peak = 0.0f;
        for (int b = 0; b < 1000; ++b)   // 10 s, no notes
        {
            v.render (out.data(), (int) out.size());
            for (auto x : out)
                peak = std::max (peak, std::abs (x));
        }

        float drift = 0.0f;
        const auto& t = v.getTopology();
        const auto& s = v.getState();
        for (std::uint32_t i = 0; i < t.numLumps; ++i)
            drift = std::max (drift, std::hypot (s.x[i] - t.restX[i], s.y[i] - t.restY[i]));

        if (shape == slink::RestShape::settled)
        {
            CHECK (drift < 1.0e-5f);   // perfectly still
            CHECK (peak < 1.0e-5f);    // silent
        }
        else
        {
            CHECK (drift > 1.0e-2f);   // the built shape visibly settles (reference-model behaviour)
        }
    }
}

void pinnedLumpsStayPut()
{
    slink::Voice v (slink::makeSpiderCross());
    v.prepare (48000.0, 1000.0);

    std::vector<float> out (512);
    for (int b = 0; b < 200; ++b)
    {
        if (b % 20 == 0)
            v.noteOn (60, 1.0f);
        v.render (out.data(), (int) out.size());
    }

    const auto& t = v.getTopology();
    const auto& s = v.getState();
    for (std::uint32_t i = 0; i < t.numLumps; ++i)
        if (t.defaultConstraints[i] == slink::Constraint::fixXY)
        {
            CHECK (s.x[i] == t.restX[i]);
            CHECK (s.y[i] == t.restY[i]);
        }
}

void stableAtAudioRate (slink::Integrator integrator)
{
    slink::Voice v (slink::makeSpiderCross());
    v.prepare (48000.0, 48000.0);
    auto p = v.getParams();
    p.integrator = integrator;
    v.setParams (p);

    std::vector<float> out (480);
    bool finite = true;
    float peak = 0.0f;
    for (int b = 0; b < 100; ++b)   // 1 s of audio, 48000 physics steps
    {
        if (b % 25 == 0)
            v.noteOn (48 + b / 25, 1.0f);
        v.render (out.data(), (int) out.size());
        finite = finite && allFinite (out);
        for (auto s : out)
            peak = std::max (peak, std::abs (s));
    }

    const auto& s = v.getState();
    CHECK (finite);
    CHECK (allFinite (s.x) && allFinite (s.y) && allFinite (s.vx) && allFinite (s.vy));
    CHECK (peak > 1.0e-4f);   // it actually made a sound
    CHECK (peak <= 1.0f);
}

void timeScaledMatchesLegacyAtReferenceRate()
{
    auto run = [] (slink::Integrator integrator)
    {
        slink::Voice v (slink::makeSpiderCross());
        v.prepare (48000.0, slink::referencePhysicsRateHz);
        auto p = v.getParams();
        p.integrator = integrator;
        v.setParams (p);

        std::vector<float> all, out (256);
        for (int b = 0; b < 400; ++b)
        {
            if (b % 50 == 0)
                v.noteOn (60, 0.8f);
            v.render (out.data(), (int) out.size());
            all.insert (all.end(), out.begin(), out.end());
        }
        return all;
    };

    const auto a = run (slink::Integrator::legacy);
    const auto b = run (slink::Integrator::timeScaled);
    float maxDiff = 0.0f;
    for (size_t i = 0; i < a.size(); ++i)
        maxDiff = std::max (maxDiff, std::abs (a[i] - b[i]));
    CHECK (maxDiff < 1.0e-6f);
}

void deterministic()
{
    auto run = []
    {
        slink::Voice v (slink::makeSpiderCross());
        v.prepare (44100.0, 4000.0);
        std::vector<float> all, out (128);
        for (int b = 0; b < 300; ++b)
        {
            if (b % 40 == 0)
                v.noteOn (55 + b / 40, 0.7f);
            v.render (out.data(), (int) out.size());
            all.insert (all.end(), out.begin(), out.end());
        }
        return all;
    };

    CHECK (run() == run());
}

void coincidentLumpsStayFinite()
{
    const auto t = slink::makeSpiderCross (8, 4);
    slink::State s;
    slink::initState (t, s);
    slink::releaseAllConstraints (s);

    // Put every lump in the same place: every spring has zero length.
    std::fill (s.x.begin(), s.x.end(), 0.5f);
    std::fill (s.y.begin(), s.y.end(), 0.5f);

    const auto p = slink::resolve (slink::Params {}, 60.0);
    for (int i = 0; i < 100; ++i)
        slink::step (t, s, p);

    CHECK (allFinite (s.x) && allFinite (s.y) && allFinite (s.vx) && allFinite (s.vy));
}

void driftModeHasNoSpringForce()
{
    const auto t = slink::makeSpiderCross();
    slink::State s;
    slink::initState (t, s);

    slink::Params params;
    params.springsOn = false;
    params.smoothing = 0.0f;
    const auto p = slink::resolve (params, 60.0);

    for (int i = 0; i < 100; ++i)
        slink::step (t, s, p);

    for (std::uint32_t i = 0; i < t.numLumps; ++i)
    {
        CHECK (s.x[i] == t.restX[i]);
        CHECK (s.y[i] == t.restY[i]);
    }
}

} // namespace

int main()
{
    const std::vector<std::pair<const char*, std::function<void()>>> tests {
        { "spider-cross layout",                       spiderCrossLayout },
        { "settled shape is in force balance",         settledShapeIsInBalance },
        { "starts at standstill (settled)",            startsAtStandstill },
        { "pinned lumps stay put",                     pinnedLumpsStayPut },
        { "stable at audio rate (legacy)",             [] { stableAtAudioRate (slink::Integrator::legacy); } },
        { "stable at audio rate (time-scaled)",        [] { stableAtAudioRate (slink::Integrator::timeScaled); } },
        { "time-scaled == legacy at reference rate",   timeScaledMatchesLegacyAtReferenceRate },
        { "deterministic",                             deterministic },
        { "coincident lumps stay finite",              coincidentLumpsStayFinite },
        { "drift mode has no spring force",            driftModeHasNoSpringForce },
    };

    for (auto& [name, fn] : tests)
    {
        const int before = failures;
        fn();
        std::printf ("%s %s\n", failures == before ? "PASS" : "FAIL", name);
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
