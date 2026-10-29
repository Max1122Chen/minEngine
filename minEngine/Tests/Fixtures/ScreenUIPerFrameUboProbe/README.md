# ScreenUIPerFrameUboProbe

Reproduces the failure mode where two passes publish into one per-frame uniform block: the second
writer overwrites the matrices the first pass's already-recorded draws still read at replay.

```
build_and_run.sh                  # current tree
build_and_run.sh <pre-fix dir>    # a snapshot of ScreenUIPass/EngineSceneBindingSets before the fix
```

## Why it is a fixture and not a suite

`minEngineTests` links `libminEngine.dll` through its import library, so only `MINENGINE_API`
symbols resolve. `ScreenUIPass` and `EngineSceneBindingSets` are not exported, and the export gate
forbids adding exports to make a test reachable. This probe therefore compiles the real
`ScreenUIPass.cpp` and `EngineSceneBindingSets.cpp` into its own image, stubs only the mesh-draw
submission it does not exercise, and links everything else (RHI, GLFW, glad) from the build.

The screen-space pass is reached through `Tests/Access/ScreenUIPassTestAccess.h`, which is the one
place that differs between the fixed and pre-fix runs:

| shim call | fixed | pre-fix |
|---|---|---|
| `BlockTheRendererHandsTheScreenSpacePass` | the screen-space block | the camera block |
| `AttachScreenSpacePerFrameBlock` | `m_ScreenSpacePerFrameUniformBuffer` | `m_PerFrameUniformBuffer` |
| `RouteScreenSpaceBlock` | `EngineSceneBindingSets::SetScreenSpacePerFrame` | no-op |

## Expected output

Pre-fix: `RESULT RED: 3 failing check(s)` — the camera block loses its bytes, the pass publishes no
block of its own.

Fixed: `RESULT GREEN: 0 failing check(s)` for all eight checks.

## Coverage boundary

Covered: the pass's uniform publication against a real GL uniform buffer, the renderer's block
hand-off as the shim models it, and the per-object binder routing (which block a recorded draw ends
up viewing, read back from `GL_UNIFORM_BUFFER_BINDING`).

Not covered: pixels. No shader is run and no render pass is opened, so "the 3D image comes back in
Multi" still has to be confirmed in the editor. The block content itself is checked numerically, and
the pixel-position contract of the projection is pinned by the `screen-ui-per-frame-ubo` suite.

## Record-time uploads: now commands, not immediate writes

`RHIBuffer::UpdateSubresource` still uploads immediately on the calling thread — that is what a
backend primitive is. What changed is that recording paths no longer call it: they upload through
`RHICommandList::UpdateBuffer`, which in a recording copies the payload into the command stream and
writes it at Replay, in the recorded order. A draw recorded between two uploads now reads the first
one, so the "one writer, one reader role" property is no longer something a pass has to prove.

| Site | Buffer | Now |
|---|---|---|
| `ForwardRenderer::UpdatePerFrameUBO` | camera per-frame block | `cmdList.UpdateBuffer` (recorded) |
| `ForwardRenderer::UpdateLightUBO` | light block | `cmdList.UpdateBuffer` (recorded) |
| `ScreenUIPass::BindIdentityCameraForScreenSpace` | screen-space per-frame block | `cmdList.UpdateBuffer` (recorded) |
| `PostProcessPass` post params | post parameter block | `cmdList.UpdateBuffer` (recorded) |
| `SkyBoxPass` sky frame block (graph and `Execute`) | sky frame block | `cmdList.UpdateBuffer` (recorded) |
| `DebugDrawPass` vertex stream | debug line vertices | `cmdList.UpdateBuffer` (recorded) |
| `EngineSceneBindingSets::WriteNextPerObjectModel` | per-object matrix ring | `cmdList.UpdateBuffer` (recorded) |
| `EngineSceneBindingSets` bone palette ring | bone palette ring | `cmdList.UpdateBuffer` (recorded) |
| `MaterialRenderProxy::SyncScalarUniformBuffer` | per-proxy scalar block | `cmdList.UpdateBuffer` (recorded) |
| `ForwardRenderer::BuildShadowDrawCommands`, `ClearUnusedShadowViewProjSlots`, `ShadowUniformBuffers` rings | shadow matrices / params | **unchanged**: written before `BeginRecording`, outside the recording window, where there is no stream position to hold them |

The last row is the remaining gap in this area: those writes are still immediate, and in Multi they
land while the previous frame's replay may still be reading the same blocks. That is a cross-frame
window rather than a within-frame ordering problem, and it is registered on its own.

A write that bypasses the stream is no longer silent: the backends report an immediate upload to
`RHICommandList` while a recording is open on the same thread, which logs an error once per write
and bumps `RHICommandList::GetImmediateUploadWhileRecordingCount()`. The `rhi-command-list` suite
pins both the ordering and that counter.
