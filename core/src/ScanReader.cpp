#include "slink/ScanReader.h"

#include <algorithm>
#include <cmath>

namespace slink
{

void ScanReader::prepare (const Topology& topology)
{
    const auto n = std::max<std::size_t> (topology.scanPath.size(), 1);
    oldTable.assign (n, 0.0f);
    newTable.assign (n, 0.0f);
    reset();
}

void ScanReader::reset() noexcept
{
    std::fill (oldTable.begin(), oldTable.end(), 0.0f);
    std::fill (newTable.begin(), newTable.end(), 0.0f);
    crossfade = 0.0f;
}

void ScanReader::capture (const Topology& topology, const State& s) noexcept
{
    std::swap (oldTable, newTable);   // swaps pointers only: no allocation

    const auto& path = topology.scanPath;
    for (std::size_t i = 0; i < path.size(); ++i)
    {
        const auto l = path[i];
        const float dx = s.x[l] - s.homeX[l];
        const float dy = s.y[l] - s.homeY[l];
        newTable[i] = std::sqrt (dx * dx + dy * dy) * (1.0f / 1.5f);   // scaling as in the reference model
    }

    crossfade = 0.0f;
}

float ScanReader::read (double phase) noexcept
{
    const auto n = (int) newTable.size();
    const double along = phase * n;
    int i0 = (int) along;
    const float frac = (float) (along - i0);
    i0 = std::clamp (i0, 0, n - 1);
    const int i1 = i0 + 1 < n ? i0 + 1 : 0;

    const float oldSample = oldTable[(size_t) i0] + frac * (oldTable[(size_t) i1] - oldTable[(size_t) i0]);
    const float newSample = newTable[(size_t) i0] + frac * (newTable[(size_t) i1] - newTable[(size_t) i0]);

    crossfade = std::min (crossfade + crossfadeIncrement, 1.0f);
    const float sample = oldSample + crossfade * (newSample - oldSample);

    return std::clamp (sample, -0.99f, 0.99f);
}

} // namespace slink
