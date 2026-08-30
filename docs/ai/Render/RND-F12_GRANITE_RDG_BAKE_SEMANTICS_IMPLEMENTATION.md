# Granite RDG Semantic Parity — Implementation Plan

## Meta
- **ID:** `RND-F12`
- **Status:** Planned → **In Progress** (Phase A: S01–S02 Done, S03 in progress)
- **Owner:** project maintainer
- **Last updated:** 2026-08-30
- **Related:** [Design](./RND-F12_GRANITE_RDG_BAKE_SEMANTICS_DESIGN.md), [BUG-RENDER-013](../bugs/BUG-RENDER-013.md)
- **Granite:** `D:\Dev\GitRepo\Granite\renderer\render_graph.cpp`

## TL;DR

**北极星：** 语义复刻 Granite RDG 全表（Design §4），分 **Phase A→D** 交付；**不复制代码**。

Phase A（S01–S07）闭合 read edge + barrier + 删补丁，验收 BUG-013。Phase B–D 补齐 transient、reorder、merge、swapchain、多队列等。

## Scope
- **In:** Design §4 Mandatory 切片；Adapter 层不改 Granite 语义。
- **Out:** 粘贴 Granite 源码；Def 行除非 promoted。

## Reader quick start
1. [Design §4 全景表](./RND-F12_GRANITE_RDG_BAKE_SEMANTICS_DESIGN.md#4-granite-语义全景表复刻清单)
2. 下表
3. Granite `bake()` / `enqueue_render_passes`

---

## 1) 阶段与切片总览

### Phase A — Bake 闭包 + Barrier（**BLOCK BUG-013**）

| Slice ID | Design §4 | 内容 | 状态 | 验证 |
|----------|-----------|------|------|------|
| RND-F12-S01 | 4.1 #3–5, 4.2 | Read edge；`pass_dependencies`；missing-writer throw；删 shadow ForceInclude；序单测 | **Done** | render-graph |
| RND-F12-S02 | 4.1 #1, §3.4 | 删 Fingerprint；每帧 Bake 或 prove-safe 缓存；结构化 invalidate | **Done** | grep + rebake |
| RND-F12-S03 | 4.1 #11–12, 4.3 events, 4.4 barrier | `RDGTextureAccess`；`Enqueue` 间 `RHICmdTransition`；VK 013 矩阵 | **In Progress** | VK validation |
| RND-F12-S06 | §3.2 | set1 dirty ← physical 变化；删 pending shadow invalidate | Planned | 013 toggle |
| RND-F12-S07 | Phase A 收口 | Registry；BUG-013；PROGRESS_LOG | Planned | smoke + 目视 |

### Phase B — Bake 后半

| Slice ID | Design §4 | 内容 | 状态 | 验证 |
|----------|-----------|------|------|------|
| RND-F12-S04 | 4.1 #9, 4.3 InputRelative | `build_transients`；`InputRelative` | Planned | render-graph |
| RND-F12-S09 | 4.1 #6 | `reorder_passes` 简化版 | Planned | 序单测 |
| RND-F12-S10 | 4.1 #7, 4.3 Persistent | depth rename；persistent 语义接入 | Planned | render-graph |

### Phase C — Execute / Present

| Slice ID | Design §4 | 内容 | 状态 | 验证 |
|----------|-----------|------|------|------|
| RND-F12-S08 | 4.1 #13 | swapchain alias；`SetupAttachments(swapchain)` | Planned | Present 链 |
| RND-F12-S11 | 4.1 #8, 4.2 merge | `build_physical_passes` | Deferred | 性能需要 |
| RND-F12-S12 | 4.1 #10 | 集中 `build_render_pass_info` | Planned | Pass clear/load |

### Phase D — Granite 全量（按需 promote）

| Slice ID | Design §4 | 内容 | 状态 |
|----------|-----------|------|------|
| RND-F12-S13 | 4.4 TaskComposer | 多队列 / semaphore | Deferred |
| RND-F12-S14 | 4.2 storage, 4.3 Buffer | Buffer RDG | Deferred |
| RND-F12-S15 | 4.3 history, 4.4 scale | history / mipgen / swapchain scale | Deferred |

---

## 2) Phase A 切片详情

### RND-F12-S01 — Read dependency 闭包

- **Goal:** Granite `generic_texture_inputs` 语义闭合。
- **Touch:** `BasePass`, `TranslucencyPass`, `SkyBoxPass`, `PostProcessPass`；`RenderGraph.cpp`（`pass_dependencies`、missing-writer throw）；`ForwardRenderer` 删 shadow `ForceInclude`
- **DoD:**
  - [x] Scene pass `AddTextureInput` 含 shadow atlases
  - [x] Read 无 writer → `Bake` throw
  - [x] `m_PassStack` shadow before scene（单测）；与 `reverse` 等价性有测试
  - [x] Shadow `ForceInclude` 为零
- **Verify:** `test render-graph`

### RND-F12-S02 — Invalidate；每帧 Bake

- **Goal:** §3.4 策略 A；删 §6 指纹项。
- **Touch:** `ForwardRenderer`；`RenderGraph::Bake` 调用频率
- **DoD:**
  - [x] 无 `Fingerprint` / `m_PendingShadowBindingInvalidate`（shadow 路径）
  - [x] `Execute` 每帧 `Bake()` 或文档+单测证明缓存安全
- **Verify:** viewport resize rebake；grep

### RND-F12-S03 — Barrier + Access 元数据

- **Goal:** `build_barriers` / `physical_events` 最小实现。
- **Touch:** `RenderPass::AddTextureInput`；`RenderGraph::EnqueueRenderPasses`；`VulkanRHITexture` layout；`RHIResourceTransition`
- **DoD:**
  - [ ] `RDGTextureAccess` 落地（deferred — 当前用 `RHITextureTransitionInfo` 最小 shader-read）
  - [x] `EnqueueRenderPasses` 在 consumer pass 前 `InsertPassInputBarriers` → `RHICmdTransition`
  - [ ] VK depth shadow：attachment → shader read transition（需用户 013 目视）
  - [ ] BUG-RENDER-013 全矩阵
- **Verify:** VK validation + Editor `test` scene

### RND-F12-S06 — Binding 生命周期

- **Goal:** set1 服从 physical；无 latched stale。
- **Touch:** `EngineSceneBindingSets.cpp`
- **DoD:**
  - [ ] Dirty on physical index/ptr/dims change
  - [ ] 013 toggle 矩阵
- **Verify:** 手动 + 013 checklist

### RND-F12-S07 — Phase A 收口

- **DoD:**
  - [ ] Design §6 删除清单 = 0
  - [ ] BUG-RENDER-013 → Verified（或残余归 010 并记录）
  - [ ] FEATURE_REGISTRY：F12 Phase A Done 或 In Progress 注明
  - [ ] PROGRESS_LOG
- **Verify:** `test smoke` + GL/VK 目视

---

## 3) 依赖顺序

```text
Phase A:  S01 → S02 → S03 → S06 → S07

Phase B:  S04 ∥ S09 → S10  （A Done 后）

Phase C:  S08 → S12 → S11?  （B 核心 Done 后）

Phase D:  按需 promote
```

**禁止（全程）：**
- 新增 `ForceInclude` / fingerprint / 手工 enqueue 拆段
- 用 shader-only patch 代替 S03 barrier（除非 S03 完成且 validation 证明无 layout 问题）

---

## 4) Granite 语义追踪（Living checklist）

实施时在 PR / Progress 勾选 Design §4 表行号。

| Phase | 闭合行 | 目标日期 |
|-------|--------|----------|
| A | 4.1 #3,4,5,11,12；4.2 generic+throw；4.4 barrier | S07 |
| B | 4.1 #6,7,9；4.3 InputRelative, Persistent | TBD |
| C | 4.1 #8,10,13；4.4 invalidate_early | TBD |
| D | 4.2 storage；4.4 TaskComposer；history | Def |

---

## 5) 验证习惯

| 检查 | 命令 |
|------|------|
| RDG | `minEngine\bin\minEngineTests.exe test render-graph` |
| Smoke | `minEngine\bin\minEngineTests.exe test smoke` |
| VK Editor | `minEngine\bin\Editor.exe --rhi vulkan --project ..\MyMEProject\MyMEProject.meproject` |
| GL | `Editor.exe --rhi opengl --project …` |

---

## 6) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-30 | 初稿 S01–S07 |
| 2026-08-30 | S01–S02 Done：read edge、pass_dependencies、每帧 Bake、删 Fingerprint/ForceInclude |
