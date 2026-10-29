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

## Record-time uploads that bypass the command list

`RHIBuffer::UpdateSubresource` uploads immediately on the calling thread; nothing about it is
recorded. Every write below therefore lands while the frame is still being recorded, and any draw
that already points at the buffer sees the new bytes at replay.

| Site | Buffer | Written from | Read by | Same risk as a shared per-frame block? |
|---|---|---|---|---|
| `ForwardRenderer.cpp:815` | camera per-frame block | `UpdatePerFrameUBO`, once before recording | every 3D draw, `DebugDrawPass` | **No longer**: one writer, one role, before any recording |
| `ScreenUIPass.cpp:81` | screen-space per-frame block | the pass, while it records | screen-space draws only | No: the pass owns the block |
| `ForwardRenderer.cpp:919` | light block | `UpdateLightUBO`, once before recording | every lit draw | No: single writer, before recording |
| `ForwardRenderer.cpp:1079,1083,1117,1175,1179` | directional/spot cascade matrices, cascade far planes | `BuildShadowDrawCommands`, before the graph records | shadow passes and every lit draw | Low: written before recording; a slot is written once per frame |
| `ForwardRenderer.cpp:1211` | unused cascade slots | `ClearUnusedShadowViewProjSlots` | lit draws | Low: same, slots are zeroed once per frame |
| `ShadowUniformBuffers.cpp:80` | point-light view-proj ring | each shadow command build | the matching shadow pass | Low: ring slot per command, aliases only when the ring wraps |
| `ShadowUniformBuffers.cpp:101` | shadow params ring | each shadow command build | the matching shadow pass | Low: same ring-slot rule |
| `EngineSceneBindingSets.cpp:159` | per-object matrix ring | every draw submit | that draw | Low: one slot per draw; a draw sees the wrong matrix only after 512 draws wrap the ring (logged as an error) |
| `EngineSceneBindingSets.cpp:193,202` | bone palette ring | skinned draws | that draw | Low: same ring-slot rule |
| `MaterialRenderProxy.cpp:284` | per-proxy scalar parameter block | `BindForDraw` → `SyncScalarUniformBuffer`, **on every draw while recording** | every draw of that proxy | **Yes, same shape**: one block for all draws of the material, rewritten at recording time. Benign while the published snapshot is stable, and it reads the wrong values if the game side republishes that proxy while the frame is still recording |
| `DebugDrawPass.cpp:163` | debug line vertex buffer | `DebugDrawPass::Prepare`, while recording | that pass's own draws | Same shape, single owner: a second recording against the buffer before the first replay would mix two frames' geometry |
| `PostProcessPass.cpp:199` | post parameter block | `PrepareDrawPacket`, while recording | that pass's own full-screen draw | Same shape, single owner |
| `SkyBoxPass.cpp:259,328` | sky frame block | `SkyBoxPass::Prepare` / `Execute` | that pass's own draw | Same shape, single owner |

The pattern behind the last four rows is the same one this probe demonstrates: a block written
between recording and replay is only safe while it has exactly one writer and one reader role. Moving
uploads onto the command list removes the constraint; until then, a new pass that writes an existing
block has to prove the same property.
