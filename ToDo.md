# Slink ToDo

Work in small stages. **Run the benchmark at the end of every stage** and record the result here (see *Benchmarking* below).
Design decisions live in `DESIGN.md`.

## Stages

### Stage 0: project + benchmark baseline ✅
- [x] New `Slink/` CMake project (JUCE 8 via `JUCE_DIR`)
- [x] Local-only reference model in `refmodel/` (git-ignored) as the stage 0 baseline
- [x] `slink_bench`: times `process()` per block against the callback budget and appends to `bench/results.csv`
- [x] Baseline run of the reference model (`stage0-refmodel-baseline`)

### Stage 1: SlinkCore (data-oriented physics), behaviour parity
- [ ] `core/` library, no JUCE: `Topology`, `State`, `Params`, `step()`
- [ ] Spider-cross 50×15 builder (same layout, constraints and scan path as the reference model)
- [ ] Linear (zero-rest-length) and non-linear springs in separate arrays
- [ ] Integrators: **Legacy Heun** (matches the reference model) and **symplectic Euler**, selectable
- [ ] Homing filter, smoothing (averaging) filter, walls, gravity
- [ ] Excitation: `hit()` on the real scan path (not hard-coded indices 350–399)
- [ ] Scan path reader (displacement mode first) with frame interpolation for sub-audio physics rates
- [ ] `slink-core` engine in the benchmark
- [ ] Parity test: with Legacy Heun, the output stays close to `refmodel` for the same input
- [ ] Benchmark and record
- [ ] Make `slink-core` the committed baseline; retire the local `refmodel/`

### Stage 2: optimise
- [ ] Profile (Instruments / `perf`-style sampling) to find the real hotspots
- [ ] Spring ordering for cache locality
- [ ] SIMD: batch springs by graph colouring so no batch shares a lump, if profiling says it's worth it
- [ ] Goal: **audio-rate physics at 64-sample blocks with max load < 50%**
- [ ] Benchmark and record each change

### Stage 3: Slink plugin
- [ ] `plugin/` JUCE target (VST3 + Standalone, synth, MIDI in)
- [ ] Physics on the audio thread; physics rate derived from the host sample rate
- [ ] Parameters through APVTS, applied in `processBlock` (work with the editor closed)
- [ ] Parameters: physics rate, integrator, speed, spring, mass, friction, smoothing, homing ("still"), drift, gravity, velocity sensitivity
- [ ] Correct MIDI handling: iterate all events, handle note-off
- [ ] GUI → audio command FIFO, audio → GUI position snapshot (lock-free)
- [ ] Benchmark the real `processBlock` (via the plugin's processor in `slink_bench`)
- [ ] Validate with pluginval

### Stage 4: editor and interaction
- [ ] Mesh view drawn from the snapshot
- [ ] Touch modes: grab/drag, force field, spatial harmonic, stick/unstick, reset
- [ ] Toggles: free edges, gravity, drift, animate

## Backlog (later, not scheduled)
- [ ] **3D sphere:** a mass-spring sphere, scanned along a path on its surface while the sphere spins. Needs 3D state (`z, vz`), a sphere builder, and a scan path that moves relative to the rotating body.
- [ ] Polyphony: several `State`s sharing one `Topology`
- [ ] New mesh shapes, plus ports of the legacy ones (square-cross, line, grounded line, triangle, holed spider, droplet with pressure)
- [ ] Other scan modes: speed, spring force, y-position
- [ ] Cubic interpolation for scan reading
- [ ] Windows build check

## Benchmarking

```sh
cmake -B build                                    # Release by default
cmake --build build --target slink_bench
build/bench/slink_bench_artefacts/Release/slink_bench --label "stageN-what-changed"
```

Options: `--engines`, `--rates 60,1000,4000,48000`, `--blocks 64,128,256,512`, `--seconds`, `--no-csv`, `--list`.
Every run appends to `bench/results.csv` with a timestamp, label and git commit.

**Load** is the callback time as a percentage of the real-time budget (block size ÷ sample rate; 1333 µs for 64 samples at 48 kHz).
The **max** column matters most, because one late block is an audible dropout. In a real host other plugins share the budget, so aim for a max well under 50%.

### Results log (48 kHz, Apple M2 Max)

| Stage | Engine | Physics rate | Block | Mean | p99 | Max | Notes |
|---|---|---|---|---|---|---|---|
| 0 | refmodel | 60 Hz | 64 | 0.4% | 4.7% | 12.0% | Physics runs in bursts: blocks containing an update spike |
| 0 | refmodel | 1 kHz | 64 | 3.7% | 5.9% | 6.8% | |
| 0 | refmodel | 4 kHz | 64 | 14.7% | 18.2% | 24.1% | |
| 0 | refmodel | 48 kHz (audio rate) | 64 | **174%** | 184% | 214% | **Not real-time.** About 36 µs per physics step; audio rate needs ≲ 10 µs |
