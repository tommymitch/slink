// The SlinkCore engine: one slink::Voice on the default 50x15 spider-cross mesh.

#include "Engine.h"

#include "slink/MeshBuilders.h"
#include "slink/Voice.h"

namespace slink::bench
{
namespace
{

class SlinkCoreEngine final : public Engine
{
public:
    explicit SlinkCoreEngine (Integrator i) : integrator (i) {}

    void prepare (double sampleRate, double physicsRateHz, int) override
    {
        voice.prepare (sampleRate, physicsRateHz);
        auto p = voice.getParams();
        p.integrator = integrator;
        voice.setParams (p);
    }

    void noteOn (int noteNumber, float velocity) override  { voice.noteOn (noteNumber, velocity); }
    void process (float* output, int numSamples) override  { voice.render (output, numSamples); }

private:
    Integrator integrator;
    Voice voice { makeSpiderCross() };
};

} // namespace

std::unique_ptr<Engine> createSlinkCoreEngine()          { return std::make_unique<SlinkCoreEngine> (Integrator::legacy); }
std::unique_ptr<Engine> createSlinkCoreTimeScaledEngine() { return std::make_unique<SlinkCoreEngine> (Integrator::timeScaled); }

} // namespace slink::bench
