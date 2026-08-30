# BUG-RENDER-013 — VK multi-light shadow failure (RDG scheduling / resource lifetime)

## Meta
- **ID:** BUG-RENDER-013
- **Status:** Open — tracked under **[RND-F12](../Render/RND-F12_GRANITE_RDG_BAKE_SEMANTICS_DESIGN.md)** (Planned)
- **Owner:**
- **Found:** 2026-08-30
- **Last updated:** 2026-08-30 (S1 enqueue reverted; VK depth layout only)
- **Affects:** Vulkan Editor; ForwardRenderer + `RenderGraph` shadow/scene pass ordering; `test` scene
- **Related Feature/Slice:** **[RND-F12](../Render/RND-F12_GRANITE_RDG_BAKE_SEMANTICS_DESIGN.md)**（主修复轨）· BUG-RENDER-010 · RND-F07（shell）· RND-TD025 · ED-F01-S06

## TL;DR

With **Dir + Spot + Point** shadow maps active on Vulkan, directional shadow is **wrong or intermittent** (visibility coupled to point Cast Shadow, latched stale state, multi-copy cascade artifacts). **Dir-only isolation (`MAX_*_SHADOW_MAPS=0`) → VK Dir visually normal.**

**Revised conclusion (2026-08-30):** TD-025 clip/viewport/cull and per-light shader convention are **largely ruled out** as the primary failure mode. Symptoms match **Granite-style RDG gaps**: missing or ineffective **read edges** (shadow atlas → scene lit passes), **pass scheduling** not enforcing write-before-read within the frame graph, **static bake fingerprint** not invalidating when shadow topology changes, and possible **single-buffer SceneColor** lifetime (TD-024). Reference implementation: Granite `render_graph.cpp` (`bake`, `setup_attachments`, pass dependency closure).

**Not the primary hypothesis anymore:** ad-hoc VK descriptor/set1 binding patches (S2–S4 reverted; insufficient). **S1 pass-filter / manual enqueue split also reverted** — fix must come from correct RDG dependencies, not C++ scheduling band-aids.

---

## 症状

- VK: Dir Cast Shadow on, Point Cast Shadow **off** → directional shadow **intermittent**.
- VK: Point Cast Shadow **on** → Dir-looking mesh shadows often appear; **position can correlate with point light** while point shadows are active.
- After disabling point Cast Shadow, if Dir shadow **remains**, moving the point light **no longer** moves it → **latched state** (stale graph attachment or descriptor generation).
- Multi-cascade → multiple copies of the **same mesh** shadow; `FORCE_CASCADE=0` → single copy (BUG-010 overlap).
- OpenGL baseline OK for convention comparison; failure is **VK + full shadow graph**.

## 期望

- RDG must guarantee: all `Shadow.*` passes **finish writing** depth atlases before any scene pass **samples** them.
- Toggling point/spot shadow maps must **invalidate bake** or equivalent when logical/physical shadow resources change.
- Directional shadow must not depend on unrelated lights' shadow passes being enabled.

## 复现

1. `test` scene, `--rhi vulkan`, restore `MAX_*_SHADOW_MAPS=2` in `ShadowTypes.h`.
2. Directional Cast Shadow **on**; Point Cast Shadow **off** → note Dir shadow.
3. Enable Point Cast Shadow → observe Dir shadow change / appear.
4. Compare: `MAX_*_SHADOW_MAPS=0` (dir-only) → Dir normal.

## 环境

- Branch: `feat/render`
- OS: Windows; Editor Debug
- RDG design: [RND-F07](../Render/RND-F07_GRANITE_RDG_RESOURCE_REFACTOR_DESIGN.md) (Granite reference: `D:\Dev\GitRepo\Granite\renderer\render_graph.*`)

## 根因（定性）

### 已坐实的实验事实

| 实验 | 结果 | 推论 |
|------|------|------|
| Dir-only shadow maps (`MAX_*_SHADOW_MAPS=0`) | VK Dir **normal** | Dir CSM math + TD-025 clip scheme A **can work** on VK |
| Full maps (Dir+Spot+Point) | VK **abnormal** | Failure appears when **multi-pass shadow graph** participates |
| S2–S4 binding patches | **No durable fix** | Patching set0/set1/descriptor pool treats symptoms |

### 主假设：RDG（对齐 Granite 语义）

| 优先级 | 缺口 | minEngine 现状 | Granite 期望 |
|--------|------|----------------|--------------|
| **R1** | Scene lit passes lack **texture read dependency** on shadow atlases | `BasePass` / `TranslucencyPass` historically no `AddTextureInput(DirShadowAtlas, Spot*, Point*)`; ordering relied on `ForceInclude` + manual enqueue | `setup_dependencies` closure: consumer pass declares inputs; bake orders producer before consumer |
| **R2** | **Enqueue order** vs **bake order** decoupled | ~~S1: split `EnqueueRenderPasses` by name~~ **reverted**; single `EnqueueFrameRenderGraph` — order must come from **bake + read edges**, not ForwardRenderer filter | `setup_dependencies` closure: consumer declares inputs; bake orders producer before consumer |
| **R3** | **Static shadow fingerprint** | `BuildShadowResourceFingerprint` → `"fixed:<resolution>"` only; toggle point shadow may not rebake | Resource/budget change → invalidate bake / physical allocation |
| **R4** | **Attachment / layout lifetime** | SceneColor single physical texture across frames (TD-024 note in `VulkanRHI::BeginFrameRecording`) | Transient lifetime or explicit per-frame physical alias |
| R5 | VK depth **descriptor imageLayout** | S1: `DEPTH_STENCIL_READ_ONLY_OPTIMAL` in `VulkanRHIShaderBindingSet` for shadow SRV slots | Correct Vulkan convention; **necessary but not sufficient** |

### 已降级 / 排除为主因

- Dir `Params.w` / `UpdateLightUBO` path (does not require point light).
- Pure clip-space / flipY / cull convention (TD-025 + dir-only OK).
- S2–S4 hypotheses (ring pollution, pool exhaustion, per-frame set1 rebuild) — **reverted**; did not resolve.

## 当前工作区（相对 `3154700`）

| 改动 | 文件 | 状态 |
|------|------|------|
| Depth SRV `imageLayout` | `VulkanRHIResources.cpp` | **保留** — VK convention，与 RDG 正交 |
| Pass filter / shadow→set1→scene 拆 enqueue | `RenderGraph`, `ForwardRenderer` | **已回退** — 不靠手工调度修 shadow |

Fix path: **[RND-F12](../Render/RND-F12_GRANITE_RDG_BAKE_SEMANTICS_DESIGN.md)** Phase A — Granite RDG **语义全复刻**（read edge / barrier / invalidate）；非 Renderer 补丁。F07 仅 shell。

## 修复方向（下一步）

1. **Read Granite** `render_graph.cpp`: how `bake()` builds pass order from `add_texture_input` / outputs; compare to minEngine `RenderPass::AddTextureInput` usage for shadow atlases.
2. **Declare shadow atlas inputs** on scene passes (or central graph builder) so bake **proves** shadow before opaque — not only `ForceInclude` + string filter.
3. **Fingerprint / invalidate**: shadow map count, slot validity, resolution → `InvalidateBake()` when topology changes.
4. **Optional isolation**: bypass RDG for one frame (direct shadow RT → set1) to confirm R1–R3 vs residual RHI.
5. Re-verify BUG-010 cascade multi-shadow after RDG fix (may be separate shader/index issue).

## 实验隔离

见 [RND-TD025 §8 P7](../Render/RND-TD025_SHADOW_CONVENTION_GAP_DESIGN.md). `MAX_*_SHADOW_MAPS=0` → **VK Dir normal** (control experiment).

## 回归验证

- [x] VK dir-only: Dir shadow visually correct (user 2026-08-30)
- [ ] RDG: shadow atlas read edges + bake order → Dir stable with full maps
- [ ] VK: toggle Point Cast Shadow → Dir unchanged (aside from point contribution)
- [ ] GL regression

## 关联

- [BUG-RENDER-010](./BUG-RENDER-010.md) — cascade / convention; Dir-only OK → defer Dir math until RDG fixed
- [RND-F07](../Render/RND-F07_GRANITE_RDG_RESOURCE_REFACTOR_DESIGN.md) — Granite RDG reference
- [RND-TD025_SHADOW_CONVENTION_GAP_DESIGN.md](../Render/RND-TD025_SHADOW_CONVENTION_GAP_DESIGN.md) §8

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-30 | Filed; point-shadow coupling on VK |
| 2026-08-30 | Dir-only isolation → binding pollution hypothesis |
| 2026-08-30 | S1 band-aid; S2–S4 binding patches (later reverted) |
| 2026-08-30 | **Reframe:** primary hypothesis → **RDG** (Granite reference); VK convention largely excluded |
| 2026-08-30 | **Revert S1 enqueue:** `RenderGraph` filter + `ForwardRenderer` shadow/scene split removed; only VK depth SRV layout remains |
| 2026-08-30 | 登记 **RND-F12**；本 bug 作为 F12 验收探针；恢复 F07 Design UTF-8 |
