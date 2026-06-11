# RND-F04 — Modern RHI further evolution（现代 RHI 进一步演进）

## Meta

| Field | Value |
|-------|--------|
| **Feature ID** | `RND-F04` |
| **Type** | Refactor |
| **Status** | **Done**（S01–S04；2026-06-11 维护者编译+目视 OK） |
| **Owner** | (maintainer) |
| **Last updated** | 2026-06-11 |
| **Branch** | `render` |
| **Depends on** | `RND-F02` **Done**；`RND-F03` **M4 P0–P3 Done**（`SubmitDraw`、PSO 归 Pass、Material BindingSet 缓存） |
| **Blocks** | `RND-F05`（Vulkan 第二后端应在 **完整** 现代 RHI 语义上实现，而非再迁 Renderer） |
| **Related** | [RND-F02](./RND-F02_MODERN_RHI_DESIGN.md) · [RND-F03](./RND-F03_LEGACY_RHI_REMOVAL_DESIGN.md) §16 · [RND-F03-M4](./RND-F03-M4_PIPELINE_REFACTOR_DESIGN.md) · [RND-F05](./RND-F05_VULKAN_MODERN_RHI_COMPLETION_DESIGN.md) · [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md) |

## TL;DR

**问题（第一轮评审结论）：** F02/F03 建立了现代 RHI **词汇**，但 **编排心智** 仍是 OpenGL 即时状态：双路径 binding、缺 `RHIPipelineLayout`、draw packet 不完整、Setup/Execute 模糊、PSO/SRV/BindingSet 热路径无缓存、`setIndex` 无效等——**全部**需在单一 Feature 内收口，而非继续打补丁。

**方案：** 按 F02 GPU 模型 **重组 Renderer ↔ RHI 契约**：三层边界、`RHIPipelineLayout`、`MeshDrawPacket`（Pass 级完整 GPU 状态包）、`SubmitMeshDrawPacket` 唯一提交入口、Setup/Execute 分界、Pass 级 PSO/SRV 缓存、RHI 契约补全（`Transition` no-op、`setIndex` 真语义）。

**状态：** S01–S04 已落地。小尾巴（Set0 脏重建、Material SRV flyweight）记入 [TECH_DEBT](../TECH_DEBT.md) TD-013–TD-014。

---

## Scope

### In（本 Feature 一次性搞定）

| 类别 | 内容 |
|------|------|
| **架构** | Renderer / RHI 公共 / Backend 三层；Setup vs Execute；取代 F03-M4 §9.6 binding 双路径 |
| **RHI 类型** | `RHIPipelineLayout`；`RHIGraphicsPSODesc::PipelineLayout`；`RHICmdTransition`（GL no-op） |
| **Renderer 类型** | `MeshDrawPacket`；`PreparePackets`；`SubmitMeshDrawPacket` |
| **删除/降级** | `SubmitDrawBinding`、`SubmitDraw`/`SubmitDrawMesh` 对外路径、`BindSceneDrawResources`、`DrawMeshCommand` |
| **缓存** | Pass 级 PSO cache；SRV flyweight；BindingSet 仅资源指针变时重建 |
| **后端** | OpenGL：`setIndex` 实现；PSO Apply 与 desc 对齐（或显式标注 GL-ignored 字段） |
| **Pass 迁移** | Present/Post → Base/Translucent/Shadow 全走 packet |

### Out

- RenderGraph（`RND-F01`）
- Vulkan 代码（`RND-F05`）
- UE `FMeshPassProcessor` / 全量 draw state cache / RDG
- 并行 CommandList、多队列、Bindless
- EnvMap / IBL 恢复（**不阻塞** F04；恢复时须走 packet 模型，另开工作）

---

## Reader quick start

1. **§3 评审结论** — 第一轮对话问题全集（本文权威）
2. **§4–§5** — 现状与差距（对照代码）
3. **§6–§8** — 目标架构与 API（实现依据）
4. **§11** — 已拍板决策
5. **§12** — 执行切片 S01–S04

代码入口：`Render/RHI/`、`Render/RenderPipeline/`、`EngineSceneBindingSets.*`、`DrawCommands/MeshDrawCommand.h`

---

## 1) 背景

### 1.1 演进路线

```text
RND-F02  现代 RHI 契约 + GL 首适配 + Pass CommandList     → Done
RND-F03  Legacy 清零 + M4 管线编排（过渡态）               → In Progress（M3 尾）
RND-F04  现代 RHI 进一步演进（本文）                      → Planned
RND-F05  Vulkan 第二后端 + 契约在 VK 上补全               → Planned（退后）
```

### 1.2 F02/F03 已交付 vs 仍缺的「现代语义」

| 已交付 | 仍缺（F04 范围） |
|--------|------------------|
| `RHICreate*` / `RHICmd*` 分离 | `RHIPipelineLayout` 粘合 PSO 与 binding |
| Pass 无 `glad` | **完整** draw packet，非半套 Submit |
| `SubmitDraw` 四步糖 | 唯一入口 `SubmitMeshDrawPacket` |
| PSO 在 Pass 构建（M4 P2） | PSO cache；desc 与 Apply 一致 |
| Material `m_MaterialBindingSet` 缓存（M4 P3） | Scene Set 重建节流；SRV flyweight |
| `EngineShaderBindings` 冻结表 | 表 → layout 的明确分层，非假装 layout |

### 1.3 与 F03 M4 的关系

- F03 **Done** = Legacy 公共 API 清零 + M3 后端绞杀（**不**要求 F04 全部完成）。
- F03-M4 §9.1–§9.5、§9.7 **保留**；§9.6 binding 双路径由 **本文取代**（维护者 D4：是）。
- F04 是 F03 §16「现代 RHI 心智」的 **结构化落地**，不是 F03 的小尾巴。

---

## 2) 总评（第一轮评审核心判断）

> 方向对；复杂感主要来自 **两层模型叠在一起**——名义层已现代，行为层仍是即时 GL。

| 层 | 现状 |
|----|------|
| **名义层** | PSO、BindingSet、RenderPass、CommandList、`SubmitDraw` |
| **行为层** | 立即 `glUseProgram` / `glBind*`、每帧 `new` 瞬态 FBO、draw 时拼一半状态 |

**北极星：**

> Renderer 在 CPU 把一次 draw 录成 **完整 Packet**（PSO + 全部 DescriptorSet + 几何）；CommandList 只按 Packet 执行。`RHIPipelineLayout` 是 PSO 与 Binding 之间的 **契约**。

---

## 3) 问题清单（全文讨论并在本 Feature 搞定）

### 3.1 架构层（六大根因）

| # | 问题 | 现状症状 | F04 对策 |
|---|------|----------|----------|
| 1 | **PipelineLayout 缺失** | PSO 与 Binding 无声明式契约；`setIndex` 被忽略 | 新增 `RHIPipelineLayout`；PSO desc 必填 |
| 2 | **Draw packet 不完整** | Set0/1 在 `BindSceneDrawResources`；Set2 在 `SubmitDraw` | `MeshDrawPacket` 含 **全部** set（D1） |
| 3 | **`SubmitDrawBinding` 名实不符** | 像 layout，实为单个 set 绑定项 | 删除；并入 packet |
| 4 | **Setup / Execute 未切开** | 队列、PSO、Set、UBO 更新散落三处 | `BuildFrameSetup` + `PreparePackets` + `Execute` |
| 5 | **PSO 名义权威、实际不全** | desc 字段多、`Apply` 子集；每 draw `Create` 无 cache | Pass 级 PSO cache；Apply 对齐或标注 |
| 6 | **Pass 编排双路径** | Scene bind 与 Submit 分裂 | 单一 `SubmitMeshDrawPacket` |

### 3.2 实现层（技术债）

| 项 | 严重度 | F04 对策 |
|----|--------|----------|
| 每 draw `CreateGraphicsPipelineState` | 中 | S04 PSO cache |
| 每帧 Set1 `CreateSRV` + `CreateBindingSet` | 中 | S04 SRV flyweight + 脏标记 |
| `RHICmdTransition` 不存在 | 低（GL）/ 高（VK） | S04 接口 + GL no-op |
| `OpenGLRHI` 瞬态 FBO 每 Pass | 低 | **文档化**为 GL adapter 策略，非 bug |
| `Material::BindForDraw` 仅 scalar upload | OK | 保留；改名可选 `UpdateScalarUBO` |
| 文档「完成感」与实现落差 | 中 | F04 Done 作为渲染语义终态 |

### 3.3 `MeshDrawCommand` vs `MeshDrawPacket`（维护者 D2）

| 类型 | 职责 | Pass 相关状态 |
|------|------|----------------|
| **`MeshDrawCommand`** | `RenderPipeline` 收集的 **本帧逻辑绘制项**（几何、material 指针、model、sort key）；**任何** mesh pass 可消费 | **不带** |
| **`MeshDrawPacket`** | **某一 Pass 内、某一次 draw 提交** 的完整 GPU 状态包 | **带** PSO、全套 BindingSet、VB/IB |

```text
BuildRenderQueue → vector<MeshDrawCommand>   （pass 无关）

BasePass::PreparePackets(commands) → vector<MeshDrawPacket>   （Pass 固定状态 × Material × Layout）

BasePass::Execute → for (p : packets) SubmitMeshDrawPacket(p)
```

Opaque / Translucent **各 Pass 各 Prepare**，因 depth/blend 等 Pass 固定状态不同——与 M4 §9.5 一致。

### 3.4 与现代 GPU 模型差距

| 现代概念 | 现状 | F04 终态 |
|----------|------|----------|
| `VkPipelineLayout` | 无 | `RHIPipelineLayout` |
| PSO ∋ layout | 脱钩 | `RHIGraphicsPSODesc::PipelineLayout` |
| `vkCmdBindDescriptorSets` | 分散绑定 | packet 内一次绑齐 |
| 录制点状态完整 | 分裂 | `PreparePackets` |
| DescriptorSet 稳定 | 热路径重建 | cache + 脏标记 |
| Resource barrier | 无接口 | `RHICmdTransition` |

---

## 4) 现状盘点（代码，2026-06-11）

### 4.1 主场景 mesh draw 实际路径（待消除）

```text
BasePass::Render 循环：
  BindSceneDrawResources()   → SetBindingSet(set0); SetBindingSet(set1)
  material->BindForDraw()    → UpdateSubresource(scalar UBO)
  DrawMeshCommand()          → SubmitDraw(pso, [Material set only], vb, ib)
```

### 4.2 关键类型职责表

| 组件 | 实际行为 | F04 处置 |
|------|----------|----------|
| `SubmitDrawBinding` | 单 set 绑定项 | 删除 |
| `MeshDrawCommand` | 含 `m_PipelineState`（Pass 相关） | **移出** PSO 到 `MeshDrawPacket`；command 回归逻辑队列 |
| `RenderPassBase::BindSceneDrawResources` | 循环外绑 set0/1 | 删除 → `PreparePackets` |
| `OpenGLRHI::RHICmdSetBindingSet` | `(void)setIndex` | 实现 setIndex |
| `EngineSceneBindingSets::BuildSceneSet1` | 每帧可 CreateSRV/Set | 脏标记重建 |

### 4.3 一帧结构（保持 M4 外形，收紧语义）

```text
Setup:  BuildQueue → Update UBO → SceneBindings::Update（脏则重建 Set0/1）
Execute: Shadow → Scene BeginPass → Sky/Base/Translucent/Post → EndPass → Present
```

---

## 5) 目标架构

### 5.1 三层边界

```text
Renderer     — 队列、PreparePackets、帧 Setup；知道 Material/Pass
RHI 公共层   — PipelineLayout、PSO、BindingSet、CommandList；不知 Material
Backend      — OpenGLRHI（F04）→ VulkanRHI（F05）
```

### 5.2 Setup / Execute

```text
Frame Setup (CPU)
  BuildRenderQueue()              // → MeshDrawCommand[]，无 GPU 状态
  UpdatePerFrameUBO / LightUBO
  SceneBindings::UpdateIfDirty()

Pass Execute (RHI)
  PreparePackets(commands)        // → MeshDrawPacket[]
  for (packet) SubmitMeshDrawPacket(packet)
```

---

## 6) 核心类型与 API（normative）

### 6.1 `RHIPipelineLayout`

```cpp
class RHIPipelineLayout;  // set 0..N-1 → RHIBindingLayout*
RHIPipelineLayoutRef RHICreatePipelineLayout(std::span<RHIBindingLayout* const>);
```

| 预定义 layout | Set 0 | Set 1 | Set 2 | 用于 |
|---------------|-------|-------|-------|------|
| `SceneMeshPipelineLayout` | SceneObject | ShadowIBL | Material | Base / Translucent |
| `ShadowDepthPipelineLayout` | ShadowPass | — | — | ShadowPass |
| `FullscreenPipelineLayout` | PassLocal | — | — | Present / Post / Sky |

`EngineShaderBindings.h` = slot 常量真源；**生成** layout，自身不是 layout。

### 6.2 `RHIGraphicsPSODesc`

- 新增 `RHIPipelineLayout* PipelineLayout`（**必填**）。
- S01 允许先加字段、行为暂不变（D5）。

### 6.3 `MeshDrawPacket`（Renderer 层）

```cpp
struct MeshDrawPacket
{
    RHIGraphicsPipelineStateRef PipelineState;
    std::array<RHIBindingSet*, kMaxDescriptorSets> BindingSets{};  // D1：含 set0/1/2
    RHIBuffer* VertexBuffer = nullptr;
    RHIBuffer* IndexBuffer = nullptr;
    uint32_t IndexCount = 0;
    // ...
};
```

### 6.4 `RHICommandList::SubmitMeshDrawPacket`

唯一 mesh/fullscreen draw 提交入口；内部：PSO → 各 set `SetBindingSet` → VB/IB → Draw。

### 6.5 Binding 策略（D1：方案 A）

Packet **自带全部 BindingSets**；per-object 在 Prepare 前 `UpdateSubresource(perObjectUBO, model)`，set0 已指向该 UBO。

---

## 7) 职责表（终态）

| 谁 | 管什么 | 不管什么 |
|----|--------|----------|
| `Material` | shader；Set2 layout/set；scalar UBO | PSO；Set0/1 |
| `MeshDrawCommand` | 逻辑队列：几何、material、model、sort | Pass GPU 状态 |
| `MeshDrawPacket` | 单次 draw 完整 GPU 状态 | — |
| `*Pass` | Pass 固定状态；`PreparePackets`；Submit | 帧 UBO |
| `RenderPipeline` | 队列、帧 Setup、Pass 顺序 | 单 draw 绑 set |
| `RHI` | GPU 语义 | Scene/Material |

---

## 8) UE 对照：学与不学

| 建议学 | 不必现在学 |
|--------|------------|
| PSO = shader + layout + pass 固定状态 | 全量 `FPipelineStateCache` 体系 |
| 录制 packet、Execute submit | RenderGraph |
| 冻结 binding 表 | Bindless |
| Setup 录、Execute 提交 | 多线程 RHI |
| CommandList 转发模式 | 全量 resource barrier 图 |

---

## 9) 刻意不做

- `FMeshPassProcessor` 全套
- RenderGraph（F01）
- 并行 CommandList
- IBL/EnvMap 恢复（D7：不阻塞 F04）

---

## 10) 风险与缓解

| 风险 | 缓解 |
|------|------|
| S03 面大 | S02 Present/Post 先行模板 |
| PSO cache key 不全 | 含 shader、layout、pass 状态、RT format |
| 与 WIP 冲突 | S01 独立 PR |
| M4 §9.6 误读 | 文首已标 superseded |

---

## 11) 已拍板决策（2026-06-11）

| ID | 议题 | 决定 |
|----|------|------|
| D1 | Scene binding 进 packet | **方案 A** — packet 含 Set0/1/2 |
| D2 | Command vs Packet | **`MeshDrawCommand`** = pass 无关逻辑队列；**`MeshDrawPacket`** = 每 Pass 每次 draw 的完整状态包 |
| D3 | F03 / F04 / F05 | F03 Done = Legacy+M3；**F05（VK）依赖 F03+F04 Done** |
| D4 | 取代 M4 §9.6 | **是** |
| D5 | S01 渐进 | **是** — 先加 layout 字段 |
| D6 | 分支 | **`render`** |
| D7 | IBL/EnvMap | **不阻塞** F04；后续单独，走 packet 模型 |

---

## 12) 执行切片（RND-F04-S01 … S04）

| Slice | 状态 | 内容 | 验收 |
|-------|------|------|------|
| **S01** | **Done** | `RHIPipelineLayout` + PSO desc + `EnginePipelineLayouts` | 编译通过 |
| **S02** | **Done** | `MeshDrawPacket` + `SubmitMeshDrawPacket`；Present/Post/Sky 迁 | Pass 经 packet 提交 |
| **S03** | **Done** | `PrepareMeshDrawPackets` + `SubmitSceneMeshDrawPackets`；删 `BindSceneDrawResources`；`MeshDrawCommand` 纯逻辑队列 | 黄金场景目视 OK |
| **S04** | **Done** | PSO cache；SRV flyweight；`setIndex`；`RHICmdTransition` no-op；删 Legacy Submit API | 热路径无每 draw CreatePSO；维护者编译+目视 OK |

**顺序不可打乱。**

---

## 13) Feature Done 验收

- [x] §3 全部问题项有代码或文档对策且已落地（Set0/Material SRV 小尾巴 → TECH_DEBT TD-013/014）
- [x] `RHIPipelineLayout` + 引擎 layout 在用（S01）
- [x] 全 Pass draw 经 `SubmitMeshDrawPacket`（S02–S03）
- [x] `MeshDrawCommand` 无 Pass GPU 状态；`MeshDrawPacket` 为提交单元（S03）
- [x] `BindSceneDrawResources` 已删除（S03）
- [x] Legacy `SubmitDraw`/`SubmitDrawMesh`/`SubmitDrawBinding` 已从 public API 移除（S04）
- [x] `setIndex` 校验 + 跟踪；`RHICmdTransition` 存在（GL no-op）（S04）
- [x] PSO / Scene Set1 热路径缓存生效（S04）
- [x] 黄金场景目视 OK（维护者 2026-06-11）
- [ ] `.\scripts\verify.ps1` + material-ir（建议 commit 后补跑并记入 PROGRESS_LOG）

---

## 14) Feature 依赖链

```text
RND-F02 (Done) → RND-F03 (Legacy+M3 Done) → RND-F04 (本文) → RND-F05 (Vulkan)
```

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-06-11 | 初稿：合并第一轮评审 + 演进方案；维护者拍板 D1–D7；F04=演进、原 Vulkan 顺延 F05 |
| 2026-06-11 | S01–S03 落地：PipelineLayout、MeshDrawPacket、全 Pass packet 提交；目视验收 OK |
| 2026-06-11 | S04 落地：PSO cache、SRV flyweight、setIndex、Transition、删 Legacy Submit；Feature Done |
