# ED-F01 Vulkan Visual Parity Bugfix — Design Spec

## Meta
- **ID:** ED-F01（附属缺陷收口；不新开 Feature）
- **Type:** Feature（bugfix batch under In Progress ED-F01）
- **Status:** Done
- **Owner:**
- **Last updated:** 2026-08-26
- **Related:** [ED-F01 Design](./ED-F01_VULKAN_EDITOR_PARITY_DESIGN.md) · [Impl](./ED-F01_VULKAN_EDITOR_PARITY_IMPLEMENTATION.md) · `TD-024` / `TD-025`
- **Bug IDs:** `BUG-RENDER-005` … `BUG-RENDER-009`（均 Verified）

## TL;DR
OpenGL Editor 对照正常，Vulkan Editor 出现一组视觉/资源生命周期问题。用户澄清：**Cube 是完全不可见**（不是发黑/对比度偏差）。根因主线为共享 Per-Object UBO 录制覆盖；另含 bake Y-flip 与 buffer 立即销毁。**代码已落地且用户目视验收通过（2026-08-26）。**

## Scope
- **In（已完成）:**
  - Cube **完全不可见**
  - Plane 改 Y-scale「厚度」
  - Plane 纹理相对 GL「放大」（确认为 UBO 副作用）
  - Skybox ±Y 割裂
  - Inspector 热切换 mesh → `DEVICE_LOST`
- **Out（仍 deferred）:**
  - VK Editor 打开完整 shadow/post（ED-F01-S06）
  - IBL irradiance/prefilter 真卷积
  - `TD-025` 完整 `RHICapabilities` API

## Reader quick start
1. 本文件：假设排序、切片、验收
2. 对照：`Editor.exe --rhi opengl` vs `--rhi vulkan`，同 project `test` scene
3. 代码入口：`EngineSceneBindingSets`（Per-Object ring）、`EnvMapCapture`（bake flipY）、`VulkanRHI`（retired buffers）

---

## Pre-flight

| Item | Assessment |
|------|------------|
| Prerequisites | ED-F01 S05/S07 HDR sky **sound**；VK Editor 主路径可用 |
| Debt risk | **medium-high** — UBO 生命周期 / bake orientation / mesh destroy 缠在一起 |
| WIP | `ED-F01` In Progress；本批为同 Feature 缺陷收口 |
| True fix vs band-aid | **true fix**：Per-draw 变换用 ring + buffer range |
| Recommendation | **Go** — 已实现并验收 |

---

## 1) 背景与目标

用户确认：VK 上 Cube **完全看不见**；Plane / Sky / mesh 热替换等问题一并收口。成功标准已由用户目视验收达成。

---

## 2) 现象清单与判定（验收后）

| ID | 现象 | 严重度 | 结论 |
|----|------|--------|------|
| BUG-RENDER-005 | Cube **完全不可见** | S0 | **Verified** — Per-Object UBO 覆盖 |
| BUG-RENDER-006 | Plane 改 Y-scale → 像有厚度 | S1 | **Verified** — 与 005 同源 |
| BUG-RENDER-007 | Plane 纹理相对 GL 更「放大」 | S2 | **Verified** — UBO 副作用 |
| BUG-RENDER-008 | Sky ±Y 面割裂 | S1 | **Verified** — bake 关闭 Y-flip |
| BUG-RENDER-009 | 切换 mesh → `DEVICE_LOST` | S0 | **Verified** — deferred destroy |
| （非 bug） | Cube 不投影到 plane | — | 属 **S06** |

---

## 3) 根因与修复（实施结果）

### H2 — 共享 Per-Object UBO 覆盖 → **已修**
Ring buffer（对齐槽位）+ `RHIShaderBinding` BufferOffset/Range；scene 与 shadow 每 draw 独立区间。

### H1 — Sky ±Y → **已修**
`RHICmdSetViewport(..., flipY)`；EnvMap bake 传 `false`。

### H3 — Plane UV → **已关闭为 005 副作用**
用户确认密度正常；无独立 sampler 改动。

### H4 — Mesh 热切换 DEVICE_LOST → **已修**
`VulkanRHI` retire 队列；fence 后 / Shutdown 时 flush。

---

## 4) 方案切片

| Slice | 内容 | 状态 |
|-------|------|------|
| **BF-S01** | bug 记录 005–009；ACTIVE_WORK | Done |
| **BF-S02** | Per-Object UBO ring | Done + Verified |
| **BF-S03** | Sky bake flipY 解耦 | Done + Verified |
| **BF-S04** | Plane UV 复核 | Done（无单独代码） |
| **BF-S05** | Mesh buffer deferred destroy | Done + Verified |

---

## 5) 备选

| 选项 | 结论 |
|------|------|
| A. 本设计分片修 | **已选用并完成** |
| B. 立刻开满 VK shadow+post | 仍拒绝作为本批默认（S06） |
| C. 只文档「VK 不保证」 | 拒绝 |

---

## 6) 风险（遗留）

| 风险 | 缓解 |
|------|------|
| Ring 512 槽耗尽 | 超限打 ERROR 并 wrap；场景规模上升时再扩 |
| S06 开 shadow 后 lit 观感再变 | 单独做 S06 |

---

## 7) 验收标准

- [x] VK：`test` scene Cube 在正确世界位姿可见
- [x] VK：Plane 改 Y-scale 不再表现为厚盒子
- [x] VK：Sky 上下不再明显割裂
- [x] VK：Plane 纹理密度目视接近 GL
- [x] VK：Inspector 切换 StaticMesh 不再 `DEVICE_LOST`
- [x] GL：用户对照未报回归
- [x] PROGRESS_LOG + bug 记录 Verified；本 Meta → Done

## 8) Status note

Done — 用户 2026-08-26 目视验收通过。后续主线：ED-F01-S06 shadow/post。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-26 | 初稿：用户 GL 对照反馈后的假设与切片 |
| 2026-08-26 | 修订：Cube=完全不可见；提升共享 Per-Object UBO 覆盖为 H2 |
| 2026-08-26 | 实施：UBO ring + bake flipY + retired buffers |
| 2026-08-26 | 用户验收通过 → Meta Done；bug 005–009 Verified |
