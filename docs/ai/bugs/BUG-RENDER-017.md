# BUG-RENDER-017 - Environment bake recorded nothing and ran inside the frame's recording window

## Meta
- **ID:** BUG-RENDER-017
- **Status:** Fixed
- **Severity:** S1
- **Owner:** project maintainer
- **Found:** 2026-10-29
- **Last updated:** 2026-10-29
- **Affects:** `EnvMapCapture`, `EnvironmentMap`, `ForwardRenderer`, `ManualRenderer`, `SkyBoxPass`, `EnvironmentMapLoader`, `RHICommandList`; Multi frame threading
- **Related Feature/Slice:** environment bake path (`Environment/EnvMapCapture`, `Environment/EnvironmentMap`)

## TL;DR
Every environment bake drew through a stack `RHICommandList` that never started a recording. In
Multi it asserted on the first draw (debug) or queued the commands and dropped them with the list
(release), so the cubemap came back empty while the bake reported success. The same bake was also
called from inside the frame's recording window, where it submits an immediate command buffer of its
own, and a failed bake was retried in full on every following frame.

---

## Symptoms
- Multi, debug: assertion crash on the first frame that bakes an environment, inside the command
  list's draw path.
- Multi, release: the sky and image-based lighting sample an empty cubemap; no error is logged and
  the asset load reports success.
- A bake that fails (missing HDR, driver refusing the program, a program the graphics role has not
  created yet) repeats its whole cost - asset load, six faces per mip level, mip generation,
  immediate submit - once per frame, forever.
- The bake's `RHIBeginImmediateCommands` ran with the frame's command buffer already recording,
  where the backend resets its shared immediate buffer and its bound-pass state and then returns
  without recording anything.

## Expected
- A bake produces a cubemap that really holds its faces, or it reports why not.
- A bake runs on the graphics role, before the frame opens its recording window.
- A failed bake backs off; a final failure settles and is reported once.

## Reproduction
1. Build the Multi topology (`--frame-threads multi`) and open a project scene whose
   `EnvironmentMap` has an HDR but no baked faces.
2. Render one frame.
3. Debug: assertion at the command list's draw entry. Release: the sky samples an empty cube and
   the log stays clean.

## Root cause
`EnvMapCapture::EquirectToCubemap`, `ConvolveIrradiance` and `PrefilterEnvironment` built a local
`RHICommandList` and issued draw commands on it without `BeginRecording`. In an immediate topology
that is correct; in a recording topology the list has nowhere to put a command, and the two
outcomes (assert in debug, silent drop in release) both hid the fact that nothing was drawn.

The three call sites of `EnvironmentMap::EnsureGPUResources` inside `ForwardRenderer` and
`ManualRenderer` sat between `BeginRecording` and `FinishRecording`, and `SkyBoxPass` called it
from inside the render graph's recording window; `EnvironmentMapLoader` called it from whichever
thread loaded the asset. The function did asset loads, texture creation and an immediate submit,
none of which belong inside a recording that is still being built, and the readiness fields it left
behind were read from other roles without synchronization.

## Fix
- The bake's command burst is one recorded stream: a scope opens the recording window, the burst is
  recorded, and it is replayed before the cubemap is handed over or the immediate buffer submitted.
  The commands stay on the calling thread's buffer rather than a pooled list, because a pooled list
  is replayed on the RHI role and the caller reads its result back in the same call.
- `RHICommandList` reports a command issued outside a recording window instead of asserting: the
  command is refused, the refusal is counted, and the line carries the thread, its role and the
  caller's frames.
- `EnvironmentMap::EnsureGPUResources` admits only the graphics role and only outside a recording
  window, and refuses with a counted report when either is violated.
- The renderers call it once per frame at the frame start, before `BeginRecording`; `SkyBoxPass` and
  the manual sky path only read `IsReadyForSky()` and count the frames they draw without an
  environment. `EnvironmentMapLoader` no longer bakes at asset load.
- A failed attempt backs off (`EnvironmentBakeSchedule`): a queued handle doubles a quiet period up to
  a ceiling and keeps asking, a refusal settles the map and is reported once. Readiness is published
  through atomic flags, so no other role dereferences the resource members.
- `EnvMapCapture` reports `RetryLater` or `Failed` instead of only `nullptr`, and refuses handles
  whose native object does not exist yet instead of drawing into them.

## Regression verification
- `envmap-bake` suite:
  - `a command issued outside a recording window is refused and named`
  - `a bake is admitted on the graphics role and outside a recording window`
  - `a failed bake backs off instead of repeating on every frame`
  - `a Multi bake draws six faces into the cubemap instead of nothing`
  - `a bake refuses handles with no native object instead of drawing into them`
- Command: `minEngine/bin/minEngineTests.exe test envmap-bake`
- Unchanged and still green: `rhi-command-list`, `material-ir`, `render-graph`, `shader-compiler`.

- [x] Red before the fix (the Multi bake reached the device with no pass at all; the assertion fired)
- [x] Green after it

## Links
- `minEngine/minEngine/src/Runtime/Function/Render/Environment/EnvMapCapture.cpp`
- `minEngine/minEngine/src/Runtime/Function/Render/Environment/EnvironmentMap.cpp`
- `minEngine/Tests/Suites/EnvMapBakeTest.cpp`

---

## Change log

| Date | Note |
|------|------|
| 2026-10-29 | Recorded on fix; regression suite `envmap-bake` added |
