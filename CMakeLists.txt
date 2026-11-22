cmake_minimum_required(VERSION 3.22)

project(Slink VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Benchmarks are meaningless in Debug, so default single-config builds to Release.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

# ------------------------------------------------------------------------------
# JUCE  (override with -DJUCE_DIR=/path/to/JUCE)
# ------------------------------------------------------------------------------
set(JUCE_DIR "$ENV{HOME}/Development/Libs/JUCE" CACHE PATH "Path to the JUCE source tree")
if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found at '${JUCE_DIR}'. Pass -DJUCE_DIR=/path/to/JUCE")
endif()
add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")

# ------------------------------------------------------------------------------
# Targets
# ------------------------------------------------------------------------------
# core/    SlinkCore   — JUCE-free data-oriented physics library   (ToDo stage 1)
# plugin/  Slink       — JUCE plugin wrapping SlinkCore             (ToDo stage 3)
add_subdirectory(bench)
