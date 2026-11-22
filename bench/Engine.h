#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace slink::bench
{

/** A synth engine as seen by the audio callback: the benchmark drives every
    implementation (reference model, Slink core, ...) through this interface
    so results are directly comparable. */
struct Engine
{
    virtual ~Engine() = default;

    /** Called once before processing, like AudioProcessor::prepareToPlay(). */
    virtual void prepare (double sampleRate, double physicsRateHz, int maxBlockSize) = 0;

    /** A MIDI note-on, applied at the start of the next block. */
    virtual void noteOn (int noteNumber, float velocity) = 0;

    /** Renders one mono block, like processBlock(). Must not allocate. */
    virtual void process (float* output, int numSamples) = 0;
};

struct EngineInfo
{
    std::string name;          // used on the command line and in results.csv
    std::string description;
    std::function<std::unique_ptr<Engine>()> create;
};

/** Every engine the benchmark knows about. Add new implementations here. */
std::vector<EngineInfo> allEngines();

} // namespace slink::bench
