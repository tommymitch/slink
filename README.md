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

- `core/`: SlinkCore, the JUCE-free physics library (`Topology`, `State`, `step()`, `Voice`)
- `bench/`: `slink_bench`, engine cost against the audio-callback budget
