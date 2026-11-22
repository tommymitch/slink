# SlinkCore: the data-oriented physics library. Plain C++20, no JUCE.

add_library(SlinkCore STATIC
    src/Topology.cpp
    src/State.cpp
    src/Physics.cpp
    src/MeshBuilders.cpp
    src/ScanReader.cpp
    src/Voice.cpp)

target_include_directories(SlinkCore PUBLIC include)
target_compile_features(SlinkCore PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(SlinkCore PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
endif()

# ------------------------------------------------------------------------------
# Tests:  ctest --test-dir build   (or run build/core/slink_core_tests)
# ------------------------------------------------------------------------------
add_executable(slink_core_tests tests/CoreTests.cpp)
target_link_libraries(slink_core_tests PRIVATE SlinkCore)
add_test(NAME slink_core_tests COMMAND slink_core_tests)
