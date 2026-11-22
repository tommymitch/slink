# Slink

Data-oriented mass-spring "scanned synthesis" instrument.

- `DESIGN.md`: design decisions and data layout
- `ToDo.md`: staged plan, backlog and benchmark results log

## Build

Requires CMake ≥ 3.22 and JUCE 8 (defaults to `~/Development/Libs/JUCE`; override with `-DJUCE_DIR=...`).

```sh
cmake -B build
cmake --build build --target slink_bench
build/bench/slink_bench_artefacts/Release/slink_bench --label "my-change"
```
