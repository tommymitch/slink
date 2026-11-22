#include "Parameters.h"

namespace slink::plugin
{

namespace
{
    // A range of [lo, hi] whose midpoint sits at `centre` (for log-like controls).
    juce::NormalisableRange<float> skewedRange (float lo, float hi, float centre)
    {
        juce::NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    juce::String compactNumber (float v, int)
    {
        if (v == 0.0f)
            return "0";
        if (std::abs (v) < 0.001f || std::abs (v) >= 10000.0f)
            return juce::String (v, 2, true);   // scientific
        return juce::String (v, std::abs (v) < 1.0f ? 4 : 2);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    StringArray rateNames;
    for (auto r : physicsRates)
        rateNames.add (r <= 0 ? "Audio rate" : (r >= 1000 ? String (r / 1000, 0) + " kHz" : String (r, 0) + " Hz"));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::physicsRate, 1 }, "Physics rate", rateNames, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::timeMode, 1 }, "Time mode",
                                                        StringArray { "Legacy (per step)", "Time-scaled" }, 0));

    auto floatParam = [&] (const char* id, const char* name, NormalisableRange<float> range, float def, const char* label = "")
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
                                                           AudioParameterFloatAttributes().withLabel (label)
                                                                                          .withStringFromValueFunction (compactNumber)));
    };

    floatParam (ids::spring,    "Spring (k)",            skewedRange (0.01f, 1000.0f, 1.0f),    0.8f);
    floatParam (ids::mass,      "Mass",                  skewedRange (0.1f, 10.0f, 1.0f),       1.0f);
    floatParam (ids::damping,   "Damping",               skewedRange (1.0e-6f, 1.0e-2f, 1.0e-4f), 9.0e-5f);
    floatParam (ids::smoothing, "Smoothing",             skewedRange (0.0f, 0.1f, 0.01f),       0.01f);
    floatParam (ids::velocity,  "Velocity sensitivity",  NormalisableRange<float> (0.0f, 1.0f),  0.5f);
    floatParam (ids::gain,      "Output gain",           NormalisableRange<float> (-24.0f, 36.0f), 12.0f, "dB");

    // Forces
    floatParam (ids::homing,    "Homing (still)",        skewedRange (0.0f, 1.0f, 0.02f),       0.0f);
    floatParam (ids::gravity,   "Gravity",               skewedRange (0.0f, 0.02f, 0.002f),     0.0f);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::drift, 1 },     "Drift (springs off)", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::freeEdges, 1 }, "Free edges", false));

    // Excitation
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::excitation, 1 }, "Excitation",
                                                        StringArray { "Noise", "Sine (spatial harmonic)", "Strike" }, 0));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ids::harmonicX, 1 }, "Sine harmonic (y motion)", 1, 8, 1));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ids::harmonicY, 1 }, "Sine harmonic (x motion, 0 = off)", 0, 8, 0));
    floatParam (ids::strikeX,   "Strike X",              NormalisableRange<float> (0.0f, 1.0f), 0.5f);
    floatParam (ids::strikeY,   "Strike Y",              NormalisableRange<float> (0.0f, 1.0f), 0.7f);

    // Scan
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::scanMode, 1 }, "Scan mode",
                                                        StringArray { "Displacement", "Speed", "Spring stretch", "Y position" }, 0));

    return layout;
}

ParameterRefs::ParameterRefs (juce::AudioProcessorValueTreeState& s)
    : physicsRate (s.getRawParameterValue (ids::physicsRate)),
      timeMode    (s.getRawParameterValue (ids::timeMode)),
      spring      (s.getRawParameterValue (ids::spring)),
      mass        (s.getRawParameterValue (ids::mass)),
      damping     (s.getRawParameterValue (ids::damping)),
      smoothing   (s.getRawParameterValue (ids::smoothing)),
      velocity    (s.getRawParameterValue (ids::velocity)),
      gain        (s.getRawParameterValue (ids::gain)),
      homing      (s.getRawParameterValue (ids::homing)),
      drift       (s.getRawParameterValue (ids::drift)),
      gravity     (s.getRawParameterValue (ids::gravity)),
      freeEdges   (s.getRawParameterValue (ids::freeEdges)),
      excitation  (s.getRawParameterValue (ids::excitation)),
      harmonicX   (s.getRawParameterValue (ids::harmonicX)),
      harmonicY   (s.getRawParameterValue (ids::harmonicY)),
      strikeX     (s.getRawParameterValue (ids::strikeX)),
      strikeY     (s.getRawParameterValue (ids::strikeY)),
      scanMode    (s.getRawParameterValue (ids::scanMode))
{
}

double ParameterRefs::physicsRateHz (double sampleRate) const noexcept
{
    const auto index = std::clamp ((int) physicsRate->load(), 0, numPhysicsRates - 1);
    const auto rate = physicsRates[index];
    return rate <= 0 ? sampleRate : std::min (rate, sampleRate);
}

slink::Params ParameterRefs::coreParams() const noexcept
{
    slink::Params p;
    p.integrator     = timeMode->load() >= 0.5f ? slink::Integrator::timeScaled : slink::Integrator::legacy;
    p.springConstant = spring->load();
    p.mass           = mass->load();
    p.friction       = 1.0f - damping->load();
    p.smoothing      = smoothing->load();
    p.homing         = homing->load();
    p.gravityY       = -gravity->load();   // down
    p.springsOn      = drift->load() < 0.5f;
    return p;
}

} // namespace slink::plugin
