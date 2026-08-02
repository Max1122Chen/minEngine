# Granite-style RDG + Frame Resource Ownership — Implementation Plan

## Meta
- **ID:** `RND-F07`
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-08-02
- **Related:** [Design Spec](./RND-F07_GRANITE_RDG_RESOURCE_REFACTOR_DESIGN.md)
- **Granite:** `D:\Dev\GitRepo\Granite\renderer\render_graph.hpp` / `.cpp`

## TL;DR

两阶段大重构：**S01–S03** 停工拆除（无帧 RT 分配、真渲染不活动）；**S04–S06** 化用 Granite 式 RDG 核心 + 最小竖切；**S07–S09** 接回主路径并删除旧所有权。当前切片：设计 Draft → 待用户确认后 S01。

## Scope
- **In:** 见 Design；本表切片与验证命令。
- **Out:** Phase 1 保画面；自创 Bake 哲学；完整 VK transient（可降级跟踪 F05）。

## Reader quick start
1. [Design](./RND-F07_GRANITE_RDG_RESOURCE_REFACTOR_DESIGN.md)
2. 下表
3. Granite `RenderGraph::bake()`（`setup_dependencies` → validate → 依赖回溯 → `build_physical_*` → …）

---

## 1) 切片总览

| Slice ID | 内容 | 状态 | 验证 |
|----------|------|------|------|
| RND-F07-S01 | 帧路径停工：`ForwardRenderer` 早退，不跑场景/阴影/后处理图 | **Done** | 启动不崩；无 Pass Execute |
| RND-F07-S02 | 拆除/禁用帧资源分配点（SceneRT、Shadow Ensure、Post buffer） | **Done** | 审查 + 可选断言 |
| RND-F07-S03 | 测试与文档：render-graph 移出 smoke；ACTIVE_WORK Phase1 记录 | **Done** | smoke 非渲染绿 |
| RND-F07-S04 | Granite 式图核心落地（资源声明、`bake` 流水线骨架、`setup_attachments`、物理表） | **Done** | 单测：声明→bake→物理 index |
| RND-F07-S05 | 最小 GPU 竖切（图分配 color → clear/present 或等价） | **Done** | 目视或测试钩子 |
| RND-F07-S06 | Pass 接口对齐 Granite 阶段（dependencies / setup / prepare / build） | **Done** | 竖切用新接口 |
| RND-F07-S07 | 接回 SceneColor/Depth + 主场景 Pass | **Done** | 黄金场景可画 |
| RND-F07-S08 | 接回 Post + Shadow（删 ShadowManager 纹理所有权） | **Done*** | 阴影/后处理回归 |
| RND-F07-S09 | 删除旧 Manual `RegisterExternal` 主模型与实验 Bake 产品路径；文档收口 | **Done** | grep 清洁 + Registry |

\*S08：Post 图拥有；Shadow 路径已接回但 atlas 仍 Manager 分配 → **TD-020**。

状态：`Planned | In Progress | Done | Blocked | Deferred | Cancelled`

---

## 2) 切片详情

### RND-F07-S01 — 真渲染停工
- **Goal:** 主渲染入口不再活动（不执行依赖帧 RT 的图）。
- **Touch:** `ForwardRenderer.cpp`（`Execute` / 构图调用）；必要时 Editor 视口容忍黑屏。
- **DoD:**
  - [ ] 帧循环可走完；无场景/阴影/后处理 Pass 录制（或整图不 enqueue）。
  - [ ] 不引入新的帧 RT 分配「顶替方案」。
- **Verify:** 手启 Editor/Playground；日志或断点确认早退。

### RND-F07-S02 — 拆除帧资源分配
- **Goal:** **无人**为帧合成分配 RT / shadow / post buffer。
- **Touch:** `SceneRenderTarget` 帧路径；`ShadowResourceManager::Ensure*`；`ForwardRenderer` post 纹理创建；一切 `RegisterExternal` 喂帧 RT 的调用可删或成死代码。
- **DoD:**
  - [ ] 上述创建调用点从帧路径消失或断言失败。
  - [ ] 资产纹理加载路径未误伤。
- **Verify:** 代码审查 +（可选）Debug 断言「禁止帧路径 CreateTexture 用于 Scene/Shadow/Post」。

### RND-F07-S03 — Phase 1 收口
- **Goal:** 测试与文档承认中间态。
- **Touch:** `RenderGraphTest` / 依赖 RT 的用例；`ACTIVE_WORK`；`PROGRESS_LOG`。
- **DoD:**
  - [ ] 依赖真渲染的测试 SKIP 或暂时移出门禁，并写明原因。
  - [ ] Design Phase 1 验收勾选。
- **Verify:** `minEngineTests.exe test smoke`（约定子集）PASS。

### RND-F07-S04 — Granite 式图核心
- **Goal:** 化用 Granite 结构，而非在旧 `RenderGraph` 上打补丁（允许新目录/替换实现）。
- **Touch:** 新或重写的 `RenderGraph`；对照 Design **§3.6** 与 Granite `bake` 阶段列表。
- **DoD:**
  - [x] `RDGAttachmentInfo` / `RDGTextureResource` / `RenderPass` 声明 API 落地（可用子集）。
  - [x] `Bake` 产出物理资源索引与 `m_PhysicalTextures`（或等价所有权）；阶段表有实现或显式 Deferred 注释。
  - [x] `SetupAttachments` 填充物理句柄；**无** Scene/Shadow `CreateTexture` 喂主路径。
  - [x] `IRenderPass::{SetupDependencies, Setup, Prepare, BuildRenderPass}`（或等价）可编译链接。
- **Verify:** `render-graph`（重建）单测：声明→bake→`GetPhysicalTexture` 非空。 **PASS**（2026-08-02）。
- **Note:** 旧 Manual `RenderPassBuilder` / `RenderGraphFrameResources` / `RDGTexture` 已删；场景 Pass 为新接口 stub，主路径仍 Phase1 idle。

### RND-F07-S05 — 最小 GPU 竖切
- **Goal:** 证明「图拥有资源」可上 GPU。
- **Touch:** Renderer 最小构图；RHI clear/present。
- **DoD:**
  - [x] 物理纹理由图创建；一次可观察输出。
- **Verify:** `render-graph` clear 读回 PASS；Editor 视口应呈 slate-blue（非黑）。
- **Note:** `GraphClearPass` + `ForwardRenderer::Execute` 竖切；`SceneRenderTarget::PublishGraphColorTexture` 共享图拥有纹理。

### RND-F07-S06 — Pass 生命周期对齐
- **Goal:** `setup_dependencies` / bake 后 `setup` / 每帧 prepare / `build_render_pass` 对齐 Granite `RenderPassInterface`。
- **DoD:**
  - [x] 主路径 Pass 走新阶段；Manual builder 已删。
- **Verify:** `render-graph` + 接回场景。

### RND-F07-S07 — 接回场景
- **DoD:**
  - [x] SceneColor/Depth 由图提供并 Publish 到 SceneRT。
  - [x] Sky/Opaque/Translucent 接回。
- **Verify:** Editor 黄金场景（用户目视）。

### RND-F07-S08 — 接回 Post + Shadow
- **DoD:**
  - [x] Post FXAA/Sharpen 图拥有 `PostBufferA`。
  - [x] Shadow 图节点 ForceInclude + 录制接回。
  - [ ] Manager 无帧纹理所有权 → **TD-020**（未完，不挡主路径）。
- **Verify:** 阴影/后处理目视。

### RND-F07-S09 — 清理与收口
- **DoD:**
  - [x] grep：无 `RegisterExternal`。
  - [x] Registry F07 Done；F01 Superseded；ACTIVE_WORK 收口。
- **Verify:** smoke + render-graph PASS。

---

## 3) 依赖顺序

```text
S01 → S02 → S03          # Phase 1 停工拆除
  ↓
S04 → S05 → S06          # Phase 2a Granite 核心 + 竖切
  ↓
S07 → S08 → S09          # Phase 2b 接回 + 清理
```

禁止：S04 之前实现「临时帧资源池保画面」。  
禁止：在旧 experimental Bake 上继续加产品功能当作 S04。

---

## 4) 延后 / 取消切片

| Slice ID | Reason | Unblock condition | Next check |
|----------|--------|-------------------|------------|
| （可选）完整 transient / async compute / subpass merge | 需更强后端 | `RND-F05` 或 GL 能力评估 | F07 S04 后 |
| F01-S06 实验 Bake 产品化 | 方向错误 | — | **Cancelled**（由 F07 取代） |

---

## 5) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-02 | 初稿：S01–S09；Phase1 无分配停工；Phase2 Granite 化用 |
| 2026-08-02 | S01–S03 Done；图节点定名 `RenderPass` / `IRenderPass` |
| 2026-08-02 | S04 Done：Granite 式 Bake/SetupAttachments/Enqueue；删 Manual builder/frame resources |
| 2026-08-02 | S05 Done：`GraphClearPass` 竖切；图拥有 SceneColor clear + 读回；Editor 发布显示纹理 |
| 2026-08-02 | S06–S09 Done：场景/Post 接回；ForceInclude 阴影；无 RegisterExternal；TD-020 阴影图所有权尾 |
