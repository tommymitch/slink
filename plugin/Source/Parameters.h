#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "slink/Physics.h"

namespace slink::plugin
{

namespace ids
{
    inline constexpr const char* physicsRate = "physicsRate";
    inline constexpr const char* timeMode    = "timeMode";
    inline constexpr const char* spring      = "spring";
    inline constexpr const char* mass        = "mass";
    inline constexpr const char* damping     = "damping";
    inline constexpr const char* smoothing   = "smoothing";
    inline constexpr const char* velocity    = "velocity";
    inline constexpr const char* gain        = "gain";
    inline constexpr const char* homing      = "homing";
    inline constexpr const char* drift       = "drift";
    inline constexpr const char* gravity     = "gravity";
    inline constexpr const char* freeEdges   = "freeEdges";
    inline constexpr const char* excitation  = "excitation";
    inline constexpr const char* harmonicX   = "harmonicX";
    inline constexpr const char* harmonicY   = "harmonicY";
    inline constexpr const char* strikeX     = "strikeX";
    inline constexpr const char* strikeY     = "strikeY";
    inline constexpr const char* scanMode    = "scanMode";
}

/** Physics rate choices. The last entry means "one step per sample". */
inline constexpr double physicsRates[] = { 60, 120, 250, 500, 1000, 2000, 4000, 8000, 16000, 0 };
inline constexpr int numPhysicsRates = (int) std::size (physicsRates);

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

/** Cached raw parameter pointers, read on the audio thread. */
struct ParameterRefs
{
    explicit ParameterRefs (juce::AudioProcessorValueTreeState& state);

    std::atomic<float>* physicsRate;
    std::atomic<float>* timeMode;
    std::atomic<float>* spring;
    std::atomic<float>* mass;
    std::atomic<float>* damping;
    std::atomic<float>* smoothing;
    std::atomic<float>* velocity;
    std::atomic<float>* gain;
    std::atomic<float>* homing;
    std::atomic<float>* drift;
    std::atomic<float>* gravity;
    std::atomic<float>* freeEdges;
    std::atomic<float>* excitation;
    std::atomic<float>* harmonicX;
    std::atomic<float>* harmonicY;
    std::atomic<float>* strikeX;
    std::atomic<float>* strikeY;
    std::atomic<float>* scanMode;

    /** Physics rate in Hz for the current choice (audio rate = sampleRate). */
    double physicsRateHz (double sampleRate) const noexcept;

    /** The SlinkCore Params for the current parameter values. */
    slink::Params coreParams() const noexcept;
};

} // namespace slink::plugin
