# Slink design notes

Slink is a data-oriented mass-spring "scanned synthesis" engine.
This file records the design decisions; `ToDo.md` tracks the work.

## Decisions (agreed 2026-11-22)

| Topic | Decision |
|---|---|
| Physics rate | A parameter, not a constant. Design for ≥ 1 kHz and **try audio rate** (one physics step per sample) without committing to it yet. |
| Integrator | Selectable. The reference model's "Heun" blend is algebraically symplectic Euler with a fixed step of 0.5 (`v' = (1+3f)/4·v + ½a`, `x' = x + ½v'`), so there is one integrator with two time modes: **legacy** (fixed step per physics step, so mesh speed scales with physics rate) and **time-scaled** (step, damping and filter amounts scaled with the rate, so the mesh moves at the same real-world speed at any rate). They are identical at 60 Hz. |
| Precision | `float` throughout the physics core. |
| Polyphony | Not yet. Keep `State` separate from `Topology` so voices can share one mesh later. |
| Mesh shapes | Spider-cross 50×15 first (the reference model's default). Other and new shapes later. |
| Rest shape | Meshes are settled to force balance at build time (`settleRestShape()`) and measured from that shape, so they sit perfectly still until excited. Smoothing acts on displacement from rest. `RestShape::built` reproduces the reference model, which starts out of balance. |
| Future | A 3D sphere, scanned over its surface while it spins (see ToDo backlog). Keep the core's vector maths easy to extend from 2D to 3D. |

## Structure

```
Slink/
  core/       SlinkCore — plain C++20, no JUCE. Topology, State, Params, step(), mesh builders, scan path.
  plugin/     JUCE plugin wrapping SlinkCore.
  bench/      slink_bench — times engines against the audio-callback budget.
  refmodel/   Local-only (git-ignored) reference model: the stage 0 performance and behaviour baseline.
```

## Core data layout

- **`Topology`** is the fixed layout, built once by a mesh builder and then read-only:
  - spring endpoints as index arrays, **split into zero-rest-length (linear, no `sqrt`) and non-zero-rest-length springs**. In the 50×15 spider-cross, 2100 of the 2850 springs are linear.
  - CSR neighbour lists for the smoothing filter.
  - home (rest) positions, and per-axis free/constrained masks (0/1, multiplied rather than branched on).
  - the scan path as lump indices.
- **`State`** is per voice and changes every step: `x, y, vx, vy` and force accumulators `fx, fy`, stored as structure-of-arrays.
- **`Params`** is a small POD struct of scalars (k, inverse mass, friction, homing, smoothing, wall bounce, gravity, integrator).

`step(topology, state, params)` is a handful of flat loops: clear forces → linear springs → non-linear springs → integrate → homing → smoothing → walls.

## Threading (plugin)

- Physics runs **on the audio thread** inside `processBlock`, at the chosen physics rate.
- GUI → audio: lock-free FIFO of commands (hit, grab, drag, release, reset, topology swap).
- Audio → GUI: positions published as a snapshot for drawing. The GUI never touches live state.
- Parameters are read from `AudioProcessorValueTreeState` in `processBlock`, so automation and presets work with the editor closed.
- Timing comes from the sample rate passed to `prepareToPlay`, never from a constant.

## Reference-model bugs this design must not repeat

- MIDI iteration stalled on non-note-on events, dropping later notes. Note-offs were ignored.
- Parameters only reached the mesh through slider callbacks, so they did nothing with the editor closed.
- GUI and audio threads both mutated the mesh without synchronisation.
- `SAMPLE_RATE` was hard-coded at 44.1 kHz.
- `hit()` hard-coded lump indices 350–399 as "the scan path".
- Zero-rest-length springs computed `0 / length`, which is NaN if two lumps coincide.
