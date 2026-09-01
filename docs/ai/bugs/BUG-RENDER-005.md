# BUG-RENDER-005 — Vulkan Cube Mesh Completely Invisible

## Meta
- **ID:** BUG-RENDER-005
- **Status:** Verified
- **Severity:** S0
- **Owner:**
- **Found:** 2026-08-26
- **Last updated:** 2026-08-26
- **Affects:** Vulkan Editor opaque mesh draws, `EngineSceneBindingSets` / Per-Object UBO
- **Related Feature/Slice:** ED-F01 visual parity bugfix (BF-S02)

## TL;DR
On Vulkan Editor, the scene Cube was **completely invisible** (not merely dark). Root cause: a single host-visible Per-Object UBO overwritten between recorded draws so every draw saw the last matrix. **Fixed** with a per-draw UBO ring + buffer-range descriptor bindings.

---

## 症状
- `test` scene: plane textured and visible; Cube at ~Y=51 missing entirely under `--rhi vulkan`.
- OpenGL same scene: Cube visible.
- Not a brightness / HDR contrast issue.

## 期望
- Each opaque/translucent/shadow draw uses its own model matrix.

## 复现
1. `Editor.exe --rhi vulkan --project ..\MyMEProject\MyMEProject.meproject`
2. Open `test` scene; look for Cube near the plane.

## 环境
- Branch: `feat/render`
- Vulkan Editor

## 根因
`UpdatePerObjectModel` `memcpy`s into one HOST_VISIBLE UBO while recording multiple `vkCmdDrawIndexed` that all read that buffer at submit time. Last write wins → Cube draw uses Plane's matrix (or is lost as a distinct object).

## 修复
Per-Object uniform **ring** (aligned slots) + `RHIShaderBinding` buffer offset/range so each draw binds a distinct UBO region. Same path for ShadowPass casters.

## 回归验证
- [x] VK: Cube visible at correct world pose (user 2026-08-26)
- [x] GL: Cube still correct

## 关联
- BUG-RENDER-006 (same root cause)
- `docs/ai/Editor/ED-F01_VULKAN_VISUAL_PARITY_BUGFIX_DESIGN.md`

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-26 | Open → Fixed (UBO ring) |
| 2026-08-26 | User verified → Verified |
