# CORE-F28 — Noise Math Library — Implementation Plan

## Meta
- **ID:** `CORE-F28`
- **Status:** Done
- **Last updated:** 2026-09-22
- **Design:** [CORE-F28_NOISE_MATH_LIBRARY_DESIGN.md](./CORE-F28_NOISE_MATH_LIBRARY_DESIGN.md)

## Slices

| Slice | Status | Notes |
|-------|--------|-------|
| S01 Value + Hash + base | Done | `Noise`, `NoiseHash`, `ValueNoise` |
| S02 Perlin | Done | `PerlinNoise` |
| S03 Cellular | Done | Euclidean Distance → ≈ [-1,1] |
| S04 Fractal FBM | Done | `FractalNoise` + `shared_ptr` source |
| S05 Tests / docs | Done | suite `noise` |

## Code

- `Runtime/Core/Math/Noise/`
- Tests: `Tests/Suites/NoiseTest.cpp` · suite id `noise`

## Verification

- `cmake --build minEngine/build --target minEngineTests`
- `minEngineTests.exe test noise` — **5/5 PASSED** (529 assertions)