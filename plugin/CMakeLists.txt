# Slink plugin: Standalone + VST3 synth wrapping SlinkCore.

option(SLINK_COPY_PLUGIN_AFTER_BUILD "Copy the plugin to the system plug-in folder after building" OFF)

juce_add_plugin(Slink
    PRODUCT_NAME                "Slink"
    VERSION                     ${PROJECT_VERSION}
    COMPANY_NAME                "teamaxe"
    BUNDLE_ID                   "teamaxe.Slink"
    PLUGIN_MANUFACTURER_CODE    Tmax
    PLUGIN_CODE                 Slnk
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES             Instrument Synth
    COPY_PLUGIN_AFTER_BUILD     ${SLINK_COPY_PLUGIN_AFTER_BUILD}
    FORMATS                     Standalone VST3)

target_sources(Slink PRIVATE
    Source/Parameters.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/MeshView.cpp)

target_compile_definitions(Slink PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Slink
    PRIVATE
        SlinkCore
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
