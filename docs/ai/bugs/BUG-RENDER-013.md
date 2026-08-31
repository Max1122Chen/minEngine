# BUG-RENDER-013 — VK multi-light shadow failure (RDG scheduling / resource lifetime)

## Meta
- **ID:** BUG-RENDER-013
- **Status:** **Fixed / Verified** — [RND-F14](../Render/RND-F14_SHADOW_PASS_UBO_LIFETIME_DESIGN.md) Phase A（ShadowPass per-draw UBO offset）；用户 2026-08-31 VK full-map 验收通过
- **Owner:**
- **Found:** 2026-08-30
- **Last updated:** 2026-08-31 (关闭：RND-F14 落地；Dir/Spot/Point 各自正常)
- **Affects:** Vulkan Editor; shared Forward/Manual shadow + set1 + VK RHI path; `test` scene
- **Related Feature/Slice:** **[RND-F14](../Render/RND-F14_SHADOW_PASS_UBO_LIFETIME_DESIGN.md)**（修复轨 Draft）· [RND-F13](../Render/RND-F13_MANUAL_RENDERER_DESIGN.md)（诊断场地）· BUG-RENDER-010 · RND-F12（降级）· RND-TD025 · ED-F01-S06

## TL;DR

With **Dir + Spot + Point** shadow maps active on Vulkan, directional shadow is **wrong or intermittent** (visibility coupled to point Cast Shadow, latched stale state, multi-copy cascade artifacts). **Dir-only isolation (`MAX_*_SHADOW_MAPS=0`) → VK Dir error severity reduced (not correct)** — still faint/wrong placement vs GL; only **less broken** than full-map.

**Revised conclusion (2026-08-31 / RND-F13 S02):** Full-map **`ManualRenderer` (no RDG) shows the same wrong shadows as `ForwardRenderer`+RDG**. Therefore **RDG bake/enqueue is not the primary failure mode**. Root cause class shifts to **shared shadow pipeline** (ShadowPass / CSM command build / set1 / UBO) and **Vulkan-specific resource create/update** (depth array layers, layout/transition, SRV/descriptor, clear per layer).

**Resolution (2026-08-31):** [RND-F14](../Render/RND-F14_SHADOW_PASS_UBO_LIFETIME_DESIGN.md) Phase A — `ShadowUniformBuffers` + per-command `BufferOffset` for ViewProj/Params; removed single-mat4 overwrite path. User verified VK full-map: Dir / Spot / Point shadows independent and correct. **RND-F12** remains hygiene only.

**Still demoted:** ad-hoc set1 pool patches alone; clip-space as sole cause; RDG-first; 「只拆 Pass 类」而不改 UBO 槽。

**Dir-only 定性（2026-08-31，user 更正）：** 不能称为「正确」，只是 **错误程度较小**；与 Spot≈正确 + full-map 恶化 一并支持「多 command 抢同一 LightViewProj/Params」模型。

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
4. Compare: `MAX_*_SHADOW_MAPS=0` (dir-only) → Dir **less severe** (still not GL-correct).

## 环境

- Branch: `feat/render`
- OS: Windows; Editor Debug
- RDG design: [RND-F07](../Render/RND-F07_GRANITE_RDG_RESOURCE_REFACTOR_DESIGN.md) (Granite reference: `D:\Dev\GitRepo\Granite\renderer\render_graph.*`)

## 根因（定性）

### 已坐实的实验事实

| 实验 | 结果 | 推论 |
|------|------|------|
| Dir-only shadow maps (`MAX_*_SHADOW_MAPS=0`) | VK Dir **less wrong** (not correct vs GL) | TD-025 + 写入链 **部分可用**；仍有质量/坐标残留问题 |
| Full maps (Dir+Spot+Point) | VK **abnormal** | Failure appears when **multi-pass shadow graph** participates |
| S2–S4 binding patches | **No durable fix** | Patching set0/set1/descriptor pool treats symptoms |

### 主假设（2026-08-31）：共享阴影管线 + VK 资源语义

| 优先级 | 缺口 | 说明 |
|--------|------|------|
| **P1** | Multi-map / multi-cascade **command + UBO + set1** 一致性 | Full map 才炸；dir-only 正常 → 槽位/层索引/矩阵数组污染可疑 |
| **P2** | VK **depth array / cube** create、per-layer clear、layout transition、SRV | Manual 与 RDG 共用 RHI；错影两边一样 |
| **P3** | ShadowPass PSO / attachment / ArraySlice | 级联层写入是否落到正确 layer |
| **P4** | CSM / spot/point ViewProj 与 LightUBO `Params.w` 索引 | 与 BUG-010 重叠；先在 Manual 上隔离 |

### 已降级 / 排除为主因

- **RDG bake / PermanentOutput / read edges** — Manual full-map **同错** → 非主因（F12 仍可作图卫生，不挡 BUG-013）。
- Pure clip-space / flipY / cull convention alone（dir-only 仍不完美）.
- S2–S4 set1 pool patches alone — **reverted**; did not resolve.

## 当前工作区

Fix path: **ManualRenderer 上复现并修共享阴影/VK 路径**；修通后回归 Forward+RDG。RND-F12 降级为并行卫生项。

## 修复方向（下一步）

**执行方案（2026-08-31）：** [BUG-RENDER-013_VK_SHADOW_FIX_EXECUTION_PLAN.md](../Render/BUG-RENDER-013_VK_SHADOW_FIX_EXECUTION_PLAN.md)

摘要：S00 定界 → **S01 矩阵单源** → S02 depth layout → S03 set1 卫生 → S06 full-map → S07 Forward 回归；（条件）S04 Piccolo R32 A/B。

1. ManualRenderer full-map：对照 shadow draw 顺序、每层 `ArraySlice`、clear、transition。
2. 核对 `BuildShadowDrawCommands` / cascade UBO / spot·point slot → set1 SRV 是否串槽。
3. VK：`Texture2DArray` / cube depth 创建、framebuffer layer、descriptor `imageLayout`。
4. 浅影（BUG-010 质量）与错误多影分开记；先修「错影/耦合」。

## Piccolo 对比（教学向 VK 引擎，`D:\Dev\GitRepo\Piccolo`）

> **完整对照（含 BindingSet、Descriptor 分离、VK 审计、排查顺序）：** [VK_SHADOW_PICCOLO_MINENGINE_REFERENCE.md](../Render/VK_SHADOW_PICCOLO_MINENGINE_REFERENCE.md)

只读对比 Piccolo 与 minEngine 阴影路径，找 **架构/语义差异**（非要求照搬 Piccolo）。

| 维度 | Piccolo | minEngine | 可能端倪 |
|------|---------|-----------|----------|
| **Shadow map 存储** | `R32_SFLOAT` **color** attachment；`mesh_directional_light_shadow.frag` 写 `gl_FragCoord.z`；depth 为 **transient**（`STORE_OP_DONT_CARE`） | `DEPTH32` **depth-only** pass；`ShadowPass.frag` 写 `gl_FragDepth` | Piccolo 避开 depth texture 的 layout/采样语义；我们依赖 `DEPTH_STENCIL_READ_ONLY` + `sampler2DArray` 读 `.r` |
| **Layout 过渡** | Render pass `finalLayout = SHADER_READ_ONLY`（color）；由 subpass dependency 驱动 | `EndRenderPass` + `RHICmdTransition` 手动 barrier；粗粒度 `ALL_COMMANDS` | Piccolo 路径更「教科书」；我们 tracked layout 与 barrier 更脆弱 |
| **CSM** | **无**；单张 `4096²` map，`CalculateDirectionalLightCamera` 拟合视锥 | **4 cascade** `Texture2DArray` + `SelectDirectionalCascadeIndex` | Piccolo 不暴露 array layer / 多 pass 往返；我们 full-map 复杂度更高 |
| **Raster** | `CULL_MODE_BACK` + `FRONT_FACE_CCW`；**depth bias 关闭** | `CullMode::Front`（TD-025 scheme A）+ slope bias 1.5 / constant 0 | 面剔除策略相反；我们 VK constant bias=0（GL=4）→ 浅影/痤疮差异 |
| **Viewport** | **静态** viewport/scissor  baked in PSO；无 dynamic state | `VK_DYNAMIC_STATE_VIEWPORT/SCISSOR`；`ShadowMap2D` 不 flip | 我们每 pass 需显式 `SetViewport`（已实现） |
| **矩阵/UBO** | Ring buffer + **`STORAGE_BUFFER_DYNAMIC`** + per-draw **dynamic offset** | 单 `HOST_VISIBLE` UBO；每 cascade `UpdateSubresource` offset 0 | Piccolo 无「多 draw 共享同一 UBO 槽」问题；我们同 CB 顺序录制理论安全但模式更脆 |
| **采样** | `sampler2D` + `texture(...).r`；`closest_depth >= current_depth`；bias `0.000075` | `sampler2DArray` + layer；`currentDepth - bias > sampledDepth`；PCF/Poisson/PCSS | 我们多 cascade layer + 更重 filter；Piccolo 单次 compare 极简 |
| **Point shadow** | `R32` **2D array** color；shader 球面展开 + `texture(..., vec3(uv, layer))` | **Depth cube** + 6 face passes | 完全不同的资源模型；full-map 时我们 spot/point 与 dir **争用 set1/UBO** |

**高信号差异（优先验证）：**

1. **Color-encoded depth vs native depth** — Piccolo 全程 color `R32` + render-pass layout；我们是 depth attachment → shader read。建议在 RenderDoc 对比：shadow pass 输出值域、base pass 采样 layout 是否为 `DEPTH_STENCIL_READ_ONLY`。
2. **无 CSM 的简洁路径** — Piccolo 证明「单 map + 正交拟合」在 VK 上可工作；我们 dir-only 仍不完美 → 问题更可能在 **depth 读路径 / bias / 采样坐标**，而非 CSM 分裂数学 alone。
3. **`early_fragment_tests`** — Piccolo shadow frag 启用；我们 depth-only 依赖固定管线 depth test + `gl_FragDepth`；语义接近但实现层不同。
4. **Front vs Back cull** — 若 shadow map 本体在 RenderDoc 中 **过空或只有部分 caster**，可试对照 Piccolo 的 back-face 策略（属实验，非本次改代码）。

参考文件：
- Piccolo：`directional_light_pass.cpp`、`mesh_directional_light_shadow.frag`、`mesh_lighting.inl`（dir 采样）、`render_helper.cpp`（`CalculateDirectionalLightCamera`）
- minEngine：`ShadowPass.cpp`、`ShadowPass.frag`、`MaterialSceneShadows.glslinc`

## 实验隔离

见 [RND-TD025 §8 P7](../Render/RND-TD025_SHADOW_CONVENTION_GAP_DESIGN.md). `MAX_*_SHADOW_MAPS=0` → **VK Dir less wrong** (control experiment; **not** GL-correct).

### RND-F13 ManualRenderer（2026-08-30）

**隔离矩阵（已测）：** `MAX_*_SHADOW_MAPS=0`、`MAX_CASCADES=1`、`DIR_SHADOW_FORCE_CASCADE=0`；`--renderer manual`（日志确认 ManualRenderer）。

| 观察 | 推论 |
|------|------|
| ManualRenderer viewport 可显示（修 Sky clear 后） | Editor 不依赖 PresentPass；需 Sky pass clear depth 再 Base |
| Manual ≈ Forward（同矩阵） | 浅影 **非 RDG 独有**；共用 shadow/shader/UBO 链（→ BUG-010 质量轨） |
| 未测 full map manual vs forward | **S02 待做**；恢复生产常量后对照 |
| **Full map Manual == Forward 错影**（user 2026-08-31） | **RDG 非主因**；主因在共享阴影管线 + VK 资源路径 |

**当前工作：** 在 ManualRenderer 上定位/修复；RDG 路径预期随之修复。

## 回归验证

- [x] VK dir-only: Dir shadow **less severe** — user 2026-08-30；**2026-08-31 更正：不能称正确**
- [x] RND-F13 dir-only: Manual ≈ Forward; faint shadow shared (user 2026-08-30)
- [x] RND-F13 **full map**: Manual VK **same wrong shadows** as Forward+RDG (user 2026-08-31) → RDG demoted
- [x] Manual full-map shadows correct (user 2026-08-31)
- [x] Forward+RDG inherits same ShadowPass fix (shared code path)
- [x] VK full-map: Dir / Spot / Point each work independently (user 2026-08-31)
- [x] GL regression — user accepted with batch close 2026-08-31

## 关联

- [BUG-RENDER-010](./BUG-RENDER-010.md) — cascade multi-copy / faint shadow（与 013 重叠，质量轨并行）
- [RND-F13](../Render/RND-F13_MANUAL_RENDERER_DESIGN.md) — 当前主诊场地
- [VK_SHADOW_PICCOLO_MINENGINE_REFERENCE.md](../Render/VK_SHADOW_PICCOLO_MINENGINE_REFERENCE.md) — **Piccolo 对照 + Descriptor 分离 + VK 审计摘要**
- [RND-F12](../Render/RND-F12_GRANITE_RDG_BAKE_SEMANTICS_DESIGN.md) — RDG 卫生（降级）
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
| 2026-08-30 | RND-F13 dir-only：Manual≈Forward、浅影非 RDG 独有；恢复 full map 待 S02 |
| 2026-08-31 | **S02：** Manual full-map == RDG 错影 → **RDG 降级**；主诊转向 Manual + VK 阴影资源/管线 |
| 2026-08-31 | **Dir-only 定性修正**（非正确，仅错误较轻）；只读对比 Piccolo 阴影架构差异 |
| 2026-08-31 | **主因假设锁定：** ShadowPass host UBO 同 offset 覆盖；登记 **RND-F14** Design Draft |
| 2026-08-31 | **Fixed / Verified：** RND-F14 Phase A 落地；用户 VK full-map 三种光源各自正常 |
