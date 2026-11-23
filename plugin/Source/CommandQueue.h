#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <cstdint>

namespace slink::plugin
{

/** Something the GUI asks the audio thread to do to the mesh. */
struct Command
{
    enum class Type : std::uint8_t
    {
        grab,       // lump, a = radius
        drag,       // a, b = offset from the grab point (mesh units)
        release,
        reset
    };

    Type type = Type::reset;
    std::uint32_t lump = 0;
    float a = 0.0f, b = 0.0f;
};

/** Lock-free GUI -> audio command queue (single producer: the message thread;
    single consumer: the audio thread). Fixed size; never allocates or blocks.

    The counterpart of MeshSnapshot, which carries positions the other way. */
class CommandQueue
{
public:
    /** Message thread. Returns false if the queue is full (audio not running). */
    bool push (const Command& c) noexcept
    {
        const auto scope = fifo.write (1);
        if (scope.blockSize1 + scope.blockSize2 == 0)
            return false;
        buffer[(size_t) (scope.blockSize1 > 0 ? scope.startIndex1 : scope.startIndex2)] = c;
        return true;
    }

    /** Message thread: true when the queue is filling up, so drags (which only
        the latest one matters for) can be dropped to keep room for the rest. */
    bool isBusy() const noexcept { return fifo.getNumReady() > capacity / 2; }

    /** Audio thread: calls fn (const Command&) for every queued command, in order. */
    template <typename Fn>
    void popAll (Fn&& fn) noexcept
    {
        const auto scope = fifo.read (fifo.getNumReady());
        for (int i = 0; i < scope.blockSize1; ++i)
            fn (buffer[(size_t) (scope.startIndex1 + i)]);
        for (int i = 0; i < scope.blockSize2; ++i)
            fn (buffer[(size_t) (scope.startIndex2 + i)]);
    }

private:
    static constexpr int capacity = 256;

    juce::AbstractFifo fifo { capacity };
    std::array<Command, capacity> buffer {};
};

} // namespace slink::plugin
