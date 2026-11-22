# slink_bench: times each engine's process() against the audio-callback budget.
# Run from a Release build:  cmake --build build --config Release --target slink_bench

juce_add_console_app(slink_bench PRODUCT_NAME "slink_bench")
juce_generate_juce_header(slink_bench)

target_sources(slink_bench PRIVATE
    main.cpp
    SlinkCoreEngine.cpp)

# Optional local-only reference model (not in the repository). If present it is
# compiled in as the "refmodel" engine for comparison.
if(EXISTS "${PROJECT_SOURCE_DIR}/refmodel/refmodel.cmake")
    include("${PROJECT_SOURCE_DIR}/refmodel/refmodel.cmake")
    message(STATUS "slink_bench: reference model found, 'refmodel' engine enabled")
endif()

target_compile_definitions(slink_bench PRIVATE
    SLINK_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(slink_bench PRIVATE
    SlinkCore
    juce::juce_core
    juce::juce_graphics
    juce::juce_audio_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags)
