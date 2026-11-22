#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace slink::plugin
{

/** Lock-free audio -> GUI hand-over of lump positions (triple buffer).

    The audio thread publish()es the latest positions; the GUI thread fetch()es
    the newest complete copy. Neither side ever blocks or allocates, and the GUI
    never sees a half-written frame. Sized once at construction. */
class MeshSnapshot
{
public:
    struct Frame
    {
        std::vector<float> x, y;
    };

    explicit MeshSnapshot (std::size_t numLumps)
    {
        for (auto& f : frames)
        {
            f.x.assign (numLumps, 0.0f);
            f.y.assign (numLumps, 0.0f);
        }
    }

    /** Audio thread. */
    void publish (const float* x, const float* y) noexcept
    {
        auto& f = frames[(size_t) back];
        std::copy (x, x + f.x.size(), f.x.begin());
        std::copy (y, y + f.y.size(), f.y.begin());
        back = middle.exchange (back | dirtyBit, std::memory_order_acq_rel) & indexMask;
    }

    /** GUI thread. Returns true if a newer frame is now available via latest(). */
    bool fetch() noexcept
    {
        if ((middle.load (std::memory_order_relaxed) & dirtyBit) == 0)
            return false;
        front = middle.exchange (front, std::memory_order_acq_rel) & indexMask;
        hasFrame = true;
        return true;
    }

    /** GUI thread: the frame from the last successful fetch(). */
    const Frame& latest() const noexcept { return frames[(size_t) front]; }
    bool hasData() const noexcept { return hasFrame; }

private:
    static constexpr int dirtyBit = 4, indexMask = 3;

    std::array<Frame, 3> frames;
    std::atomic<int> middle { 1 };
    int back = 0;        // audio thread only
    int front = 2;       // GUI thread only
    bool hasFrame = false;
};

} // namespace slink::plugin
