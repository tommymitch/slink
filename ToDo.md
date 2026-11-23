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
- [x] `plugin/` JUCE target: Standalone + VST3, synth, MIDI in, mono/stereo out
- [x] Plays SlinkCore directly on the audio thread; generic editor + status line + on-screen keyboard
- [x] APVTS parameters: physics rate (60 Hz to 16 kHz, or audio rate), time mode, spring, mass, damping (= 1 - friction), smoothing, velocity sensitivity, output gain (default +12 dB; a typical note peaks around -22 dBFS raw)
- [x] Correct MIDI handling: sample-accurate, all events, last-note priority (note-off falls back to the previous held pitch without a new hit; otherwise the mesh rings on), All Sound Off / Reset All Controllers reset the mesh
- [x] **Stability guard:** `maxStableSpringConstant()` clamps k to `m * c / dt^2`; the editor shows when it is limiting. Measured limits by bisection (repeated max-velocity hits, m = 1) on `(k/m)*dt^2`: legacy about 0.3 with heavy damping and lower with light damping; time-scaled falls as the rate rises (0.32 at 60 Hz, 0.16 at 1 kHz, 0.044 at 48 kHz). With no smoothing, lightly damped meshes slowly gain energy above about 0.1. So c = 0.1 to 0.2 (by smoothing 0 to 0.01), times `min(1, (60/rate)^0.4)` in time-scaled mode. Validated on 720 cases: both modes, 30 Hz to 48 kHz, k up to 1e7, m 0.1 to 10, smoothing 0 to 0.1, light and heavy damping. Guard tests are in `slink_core_tests`. The default k = 0.8 sits exactly at the legacy limit.
- [x] Spring compression cap (`Params::minCompression`) kept as an option, off by default. It didn't help stability and it changes the sound.
- [x] Build instructions in README
- [x] Basic mesh view (pulled forward): lock-free triple-buffer position snapshot published once per block, drawn per display refresh with plain JUCE graphics. Scan ring highlighted; display-only exaggeration control (default 4x)
- [ ] Listen: compare against the reference model by ear, then retire `refmodel/`

### Stage 1c: expose reference-model features
- [x] Homing ("still"), now a continuous control. **Changed:** homing is a critically damped spring to home (a force) rather than the reference model's position blend. The blend left a constant velocity that never decayed wherever springs or gravity held a lump away from home: it was heard in speed scan mode and kicked the mesh when homing was switched off. It is now also identical in both time modes at every rate.
- [x] Drift (springs off)
- [x] Gravity (downwards, amount). The reference model's gravity button did nothing: the line that set the gravity vector was commented out.
- [x] Free edges: unpins the rim and hub; re-pinning puts them back at rest (the jump back excites the mesh). Kept across reset.
- [x] Reset mesh button (editor -> audio thread; now through the command queue)
- [x] Excitation: noise (scan ring, as before), sine (whole-mesh spatial harmonic with x/y harmonic numbers; additive, where the reference model set positions absolutely) or strike (raised-cosine bump at a point). Level-matched to noise (sine x0.8, strike x2).
- [x] Scan mode: displacement, speed, spring stretch, y position. Level-matched to displacement with noise hits (x0.25, x0.42, x0.47). Sine excitation read as spring stretch stays ~25 dB quieter: a smooth shape barely stretches the ring.
- [x] Stability: 960-case sweep (both modes, 60 Hz/1 kHz/48 kHz, k 0.8 and max, smoothing 0/0.01, gravity, homing, free/pinned, all excitations). All pinned cases decay under heavy damping. Free-edge cases are bounded (energy ratio <= 1.23, nothing diverges) but don't fully decay, see next item.
- [ ] **Smoothing is still a position blend**, the same flaw homing had. When a mesh is held away from its rest shape (freed edges collapsing it into a knot, or gravity sag), smoothing fights the springs and leaves a small constant velocity: ~3e-5 with gravity (about -100 dB in speed mode, inaudible), larger in a collapsed free mesh. Fix: make smoothing a force (a Laplacian damper), which changes the parity sound, so decide by ear.
- [ ] Free edges collapse the spider-cross: its spokes and diagonals are zero-rest-length springs, so without the rim pinned they pull everything to the centre. Consider rest-length spokes for free-edge meshes, or treat this as part of the new-shapes work.
- [ ] Not exposed: speed (just 1/sqrt(mass)), syrup (a friction preset), asymmetry (needs per-lump/per-spring arrays and a worst-case guard), the remaining touch modes and custom scan paths (stage 4; the command queue they needed is in).

### Stage 2: optimise
- [ ] Run the benchmark loop at real-time / audio-thread priority (macOS QoS), so OS scheduling outliers don't mask real worst-case changes (one 48 kHz run showed a single 155% block with a normal 49% p99)
- [ ] Profile (Instruments / `perf`-style sampling) to find the real hotspots
- [ ] Spring ordering for cache locality
- [ ] SIMD: batch springs by graph colouring so no batch shares a lump, if profiling says it's worth it
- [ ] Goal: **audio-rate physics at 64-sample blocks with max load < 50%**
- [ ] Benchmark and record each change

### Stage 3: Slink plugin (builds on the 1b target)
- [x] Physics rate derived from the host sample rate (fixed rates, or "audio rate" = the host's rate)
- [x] Parameters through APVTS, applied in `processBlock` (work with the editor closed)
- [x] Parameters: physics rate, integrator, spring, mass, friction, smoothing, homing ("still"), drift, gravity, velocity sensitivity (speed left out, see stage 1c)
- [x] Correct MIDI handling: iterate all events sample-accurately, note-off with last-note priority, all-sound-off resets
- [x] GUI → audio command FIFO: `plugin/Source/CommandQueue.h` (grab, drag, release, reset; lock-free, fixed size, drags dropped when it's half full so grab/release always fit). The audio → GUI position snapshot is `plugin/Source/MeshSnapshot.h`
- [ ] Benchmark the real `processBlock` (via the plugin's processor in `slink_bench`)
- [ ] Validate with pluginval (not installed yet)

### Stage 4: editor and interaction
- [x] Mesh view drawn from the snapshot (plain JUCE graphics; the GPU renderer is in the backlog)
- [x] Touch: grab and drag. Click near the mesh to grab a patch (radius = "Grab size", raised-cosine falloff) and drag it; releasing plucks the mesh, a fast drag flings it. Grabbed lumps are held kinematically (masks only: constraints and homes are untouched), the drag is smoothed over 5 ms so block-rate updates don't click, and release hands over the drag velocity. Drags are divided by the view exaggeration, so the grabbed lump stays under the mouse. Tested in both time modes at 60 Hz, 1 kHz and 48 kHz; no cost when not grabbing.
- [ ] Touch modes, as a selector: force field (push/pull without pinning), stick/unstick (pin lumps where they are), paint a spatial harmonic
- [x] Toggles: free edges, gravity, drift (as parameters)
- [ ] Animate toggle (pause the display), if still wanted

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
| 1b | slink-core | 48 kHz (audio rate) | 64 | 42.3% | 48.5% | 60.0% | Guard + optional compression cap: no change in cost (max is OS jitter; time-scaled 53.6%) |
| 1c | slink-core | 48 kHz (audio rate) | 64 | 42.4% | 48.6% | 70.1% | Homing force in the integrate loop: +2% until specialised (loop compiled with/without homing); now back to stage 1 cost. Max is a single OS outlier (time-scaled 57.4%) |
| 4 | slink-core | 48 kHz (audio rate) | 64 | 41.3% | 47.6% | 121.7% | Touch grab (one branch per step when not grabbing): no change in cost. Max is OS jitter on a loaded machine (load average 5.6; time-scaled 108.9%) |
