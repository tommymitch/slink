# Slink

Data-oriented mass-spring "scanned synthesis" instrument.

- `DESIGN.md`: design decisions and data layout
- `ToDo.md`: staged plan, backlog and benchmark results log

## Build

Requires CMake ≥ 3.22 and JUCE 8 (defaults to `~/Development/Libs/JUCE`; override with `-DJUCE_DIR=...`).

```sh
cmake -B build
cmake --build build
ctest --test-dir build                                            # core unit tests
build/bench/slink_bench_artefacts/Release/slink_bench --label "my-change"
```

Plugin only (Standalone + VST3):

```sh
cmake --build build --target Slink_Standalone Slink_VST3
open build/plugin/Slink_artefacts/Release/Standalone/Slink.app
```

The VST3 is at `build/plugin/Slink_artefacts/Release/VST3/Slink.vst3`. To have it copied to `~/Library/Audio/Plug-Ins/VST3` on every build, configure with `-DSLINK_COPY_PLUGIN_AFTER_BUILD=ON`. For an Xcode project, use `cmake -B build-xcode -G Xcode`.

- `core/`: SlinkCore, the JUCE-free physics library (`Topology`, `State`, `step()`, `Voice`)
- `bench/`: `slink_bench`, engine cost against the audio-callback budget
- `plugin/`: the Slink synth (Standalone + VST3): parameters, MIDI and a generic editor with an on-screen keyboard
