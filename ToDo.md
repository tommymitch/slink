# Slink ToDo

Work in small stages. **Run the benchmark at the end of every stage** and record the result here (see *Benchmarking* below).
Design decisions live in `DESIGN.md`.

## Stages

### Stage 0: project + benchmark baseline ✅
- [x] New `Slink/` CMake project (JUCE 8 via `JUCE_DIR`)
- [x] Local-only reference model in `refmodel/` (git-ignored) as the stage 0 baseline
- [x] `slink_bench`: times `process()` per block against the callback budget and appends to `bench/results.csv`
- [x] Baseline run of the reference model (`stage0-refmodel-baseline`)

### Stage 1: SlinkCore (data-oriented physics), behaviour parity ✅
- [x] `core/` library, no JUCE: `Topology`, `State`, `Params`, `step()`, `Voice`
- [x] Spider-cross 50×15 builder (identical layout, constraints and scan path to the reference model)
- [x] Linear (zero-rest-length) and non-linear springs in separate arrays
- [x] Integrator option. The reference model's "Heun" blend turned out to be exactly symplectic Euler with a fixed step of 0.5, so there is one integrator with two time modes: **legacy** (fixed step: mesh speed scales with physics rate) and **time-scaled** (step, damping and filters scaled with the rate: same mesh speed at any rate)
- [x] Homing filter, smoothing (averaging) filter, walls, gravity, drift (springs off)
- [x] Excitation: `hitNoise()` on the real scan path (all 50 lumps; the reference model missed one). A note no longer turns homing off as a side effect.
- [x] Scan path reader (displacement mode) with old → new table crossfade over one physics frame
- [x] Output: 20 Hz Butterworth high-pass computed for the actual sample rate (was hard-coded for 44.1 kHz)
- [x] Unit tests (`slink_core_tests`, run with `ctest`): layout, pinned lumps, audio-rate stability (both modes), modes identical at 60 Hz, determinism, NaN guard, drift
- [x] `slink-core` and `slink-core-ts` engines in the benchmark
- [x] Parity test (local only): topology identical; gentle hit matches at 79 dB signal-to-error over 10 s; normal hit is chaotic (the reference model diverges from its own float-rounded start at the same rate), loudness within 0.35 dB
- [x] Benchmark and record: **~4.2× faster; audio rate now real-time** (42% mean, 54% max at 64-sample blocks)
- [x] `slink-core` is the committed baseline from here on
- [x] **Start at true rest (option b):** the built spider-cross isn't in force balance (the zero-length diagonals pull the rings in), so it used to breathe for ~5 minutes and settle ~20% smaller. Now `settleRestShape()` solves the equilibrium at build time (double precision, ~200 steps, ~3 ms), and the scan measures displacement from that settled shape: a silent mesh until hit, and a new, brighter timbre (the scan reads |vibration| rather than mostly radial motion). Smoothing now acts on displacement from rest, so it no longer disturbs a mesh at rest. `RestShape::built` keeps the reference-model behaviour (used by the parity test).
- [ ] Retire the local `refmodel/` (keep until you're happy with the sound in stage 1b)

### Stage 1b: bare-bones listening plugin
So each later stage can be judged by ear as well as by benchmark numbers.
- [ ] `plugin/` JUCE target: Standalone + VST3, synth, MIDI in, mono/stereo out
- [ ] Plays SlinkCore directly on the audio thread; no mesh view (default generic editor)
- [ ] A few APVTS parameters: physics rate, integrator, spring, mass, friction, smoothing, volume
- [ ] Correct MIDI handling from the start (all events, note-off)
- [ ] **Stability guard:** clamp spring/mass (and minimum smoothing) to the stable range for the current mode and physics rate. Measured with a max-velocity hit: legacy mode is stable up to k ≈ 0.8 with smoothing 0.01, but blows up at k ≥ 0.85, or at k ≥ 0.7 with smoothing 0. Time-scaled mode's limit grows with the rate squared: k ≤ ~0.8 at 60 Hz, ≤ 2 at 120 Hz, ≤ 100 at 1 kHz, ≥ 1000 at 48 kHz; 30 Hz is unstable even at the default.
- [ ] Build instructions in README

### Stage 2: optimise
- [ ] Run the benchmark loop at real-time / audio-thread priority (macOS QoS), so OS scheduling outliers don't mask real worst-case changes (one 48 kHz run showed a single 155% block with a normal 49% p99)
- [ ] Profile (Instruments / `perf`-style sampling) to find the real hotspots
- [ ] Spring ordering for cache locality
- [ ] SIMD: batch springs by graph colouring so no batch shares a lump, if profiling says it's worth it
- [ ] Goal: **audio-rate physics at 64-sample blocks with max load < 50%**
- [ ] Benchmark and record each change

### Stage 3: Slink plugin (builds on the 1b target)
- [ ] Physics rate derived from the host sample rate
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
- [ ] **Scan offset as a timbre control?** With the old built-shape home, a static offset made the scan read mostly radial motion (smoother sound). Re-introducing an adjustable offset could give a continuous control between that and the settled shape's rectified sound.
- [ ] **Hit strength vs chaos:** normal hits (up to ~0.13 displacement on a 0.033 ring spacing) drive the mesh into a chaotic regime. Consider making the excitation shape and strength a parameter, and decide whether chaos is a feature.
- [ ] **3D sphere:** a mass-spring sphere, scanned along a path on its surface while the sphere spins. Needs 3D state (`z, vz`), a sphere builder, and a scan path that moves relative to the rotating body.
- [ ] **High-performance graphics** (to discuss): GPU-rendered mesh view, either a JUCE OpenGL component or a WebGL front end (JUCE WebBrowserComponent). Should draw from the lock-free position snapshot and scale to larger meshes and the 3D sphere.
- [ ] Polyphony: several `State`s sharing one `Topology`
- [ ] New mesh shapes, plus ports of the legacy ones (square-cross, line, grounded line, triangle, holed spider, droplet with pressure)
- [ ] Other scan modes: speed, spring force, y-position
- [ ] **Anti-aliased scan reading** (to discuss): the scan path is read as a raw linearly interpolated wavetable, so it aliases. Options: oversampling + decimation filter, mipmapped (band-limited) tables rebuilt per physics frame, or band-limited interpolation. Must stay cheap enough for audio-rate physics, where the table changes every sample.
- [ ] Cubic interpolation for scan reading (may be folded into the anti-aliasing work)
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
| 1 | slink-core | 60 Hz | 64 | 0.09% | 0.8% | 1.5% | |
| 1 | slink-core | 1 kHz | 64 | 0.92% | 1.7% | 2.8% | |
| 1 | slink-core | 4 kHz | 64 | 3.6% | 5.0% | 10.2% | |
| 1 | slink-core | 48 kHz (audio rate) | 64 | **42.4%** | 47.6% | 53.6% | **Real-time.** ~8.8 µs per step (4.2× faster). Just over the 50% max goal; stage 2. Time-scaled mode costs the same |
