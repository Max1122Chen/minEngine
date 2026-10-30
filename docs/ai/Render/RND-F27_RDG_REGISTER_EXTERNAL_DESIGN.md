# RND-F27 — RDG RegisterExternal（外部物理纹理绑定）

## Meta
- **ID:** `RND-F27`
- **Type:** Feature
- **Complexity:** `L2`
- **Status:** `Done`
- **Owner:** agent + @mikumikumikumiku
- **Last updated:** 2026-10-25
- **Related:** [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [RND-F07](./RND-F07_GRANITE_RDG_RESOURCE_REFACTOR_DESIGN.md) · [RND-F25](./RND-F25_RENDER_TO_TEXTURE_DESIGN.md) · [RND-F28](./RND-F28_TEXTURE2D_RENDER_TARGET_DESIGN.md)（依赖本 Feature）
- **Branch:** `feat/render`
- **消费方驱动：** minCraft 离屏方块图标（见 minCraft `ENGINE_FEEDBACK_PROMPT_OFFSCREEN_TEXTURE2D_WRAP.md`）

## TL;DR

当前 `RenderGraph::SetupAttachments` 只会按 ViewKey 缓存**自行创建**物理纹理，无法把调用方已有的 `RHITexture` 当作 SceneColor（`swapchainOrNull` 参数是空 stub）。本 Feature 增加**窄接口** `RegisterExternal`，让预创建的 RT 占据活动视图上某个逻辑资源的 physical slot，从而支撑 RND-F28 的零拷贝 `RenderInto`。

## Pre-flight

| 项 | 结论 |
|----|------|
| 前置 | RDG Bake/SetupAttachments + ViewKey 缓存（**健全**）；OffscreenRenderer（**健全**，F25） |
| 技术债风险 | 若 External 做成万能 Import → **中**；首版仅 SceneColor（Depth 可选） |
| 同分支 WIP | F26 Done；F24 S04 待办 — **可推进**（正交） |
| 哲学 | 纯机制；不碰 Texture2D / 游戏策略 |
| 建议 | **Go** — 评审通过后实现，再接线 F28 |

## 问题

1. Offscreen / SceneRenderTarget 只在图跑完后 **Publish** 图拥有的 color，不能把外部 color **喂进**图。
2. 同一 Offscreen ViewId 复用会覆写同一块 physical SceneColor——在不做拷贝/重建的前提下，无法「每个 bake 目标一张耐久贴图」。
3. `SetupAttachments(rhi, swapchainOrNull)` 文档暗示外部路径，实现却是 `(void)swapchainOrNull;`。

## 需求

- R1：`Bake()` 之后，调用方可将已有 `RHITextureRef` 绑定到活动 **ViewKey** 上的**命名逻辑纹理**，使 `SetupAttachments` **不再**为该 physical slot 新建纹理。
- R2：绑定须相对 Bake 后的尺寸（宽、高、format、layers）及所需 usage（颜色输出至少 `RenderTarget`；图若声明了则还需 `ShaderResource`）做校验。
- R3：校验失败**不得**静默回退到内部创建——打 Error 日志，**本帧该次提交失败**（不安装错误纹理、不继续当作成功绘制）。见「失败模型」（评审锁定）。
- R4：外部绑定 **按 ViewKey**；经显式 Unregister / `Reset` / 再次 Register 替换清除；不得泄漏到无关视图。
- R5（评审收窄）：**本 Feature 验收面**为 RenderGraph API + 单测（SceneColor）。**不要求**在 F27 内改 Offscreen/ForwardRenderer 产品路径；端到端 Offscreen 接线属 **F28**。
- R6：未注册 External 的视图行为与今日完全一致（回归：主视口、缩略图）。

## 约束

- C1：本 Feature 不重做 Bake / pass 依赖 / Granite rename-alias。
- C2：不在此引入 Texture2D / Asset 类型（属 F28）。
- C3：物理缓存仍为每 slot 一个 `shared_ptr<RHITexture>`；External 使用同类型（与调用方共享所有权）。
- C4：不改变当前 ForwardRenderer 单线程提交假设。

## 偏好

- P1：统一用 `RegisterExternalTexture`，废弃或删除半死不活的 `swapchainOrNull` 第二路径。
- P2：`kRDGSceneDepth` External 可放 S02；S01 可仅 SceneColor。

## 假设

- A1：调用方纹理已具备 `RenderTarget | ShaderResource`（或与声明 usage 匹配）——F28 工厂会创建此类纹理。
- A2：离屏时 SceneColor 的解析尺寸（Absolute，或 SwapchainRelative 解析为 Offscreen 视口尺寸）在配置正确时与外部纹理一致。
- A3：无多线程并发 Register。

## 非目标

- 完整 UE/Granite「任意资源 Import 进任意 pass」框架。
- CPU readback、blit 退路（若 F27 延期，仅可在 F28 临时讨论，不在本 Feature 实现）。
- 把 swapchain image 做成一等 External（超出删除/统一 stub）。
- 改变 PresentPass / 编辑器 ImGui 采样契约（除 SceneColor 已是 shared_ptr 安全之处）。

## 当前架构

```
SetActiveViewKey(ViewId, SceneRenderTarget*)
  → BindPhysicalTexturesForActiveView()
Bake()
  → BuildPhysicalResources() → physical 下标 + m_PhysicalDims
SetupAttachments(rhi, swapchainOrNull≈未用)
  → 每个 physical：create 或 reuse RHICreateTexture2D
Enqueue…
ForwardRenderer::EnqueueFrameRenderGraph
  → PublishGraphColorTexture(GetPhysicalTextureShared(SceneColor))
```

- 缓存：`ViewPhysicalCache`，键为 `RDGViewKey`。
- SceneRenderTarget 不再自建 color（仅 Publish）。

## 方案架构

### 核心思路

对活动视图将部分 physical slot 标为 **ExternallyOwned**。`SetupAttachments` 对这些 slot 跳过 `RHICreateTexture2D`，安装已注册的 `RHITextureRef`。

```
RegisterExternalTexture("SceneColor", tex)  // 活动 ViewKey
SetupAttachments
  → slot[i] = externalRef  （不 create）
GetPhysicalTextureShared(SceneColor) → 同一 tex
Publish → SRT 可持有同一 shared_ptr（可选；F28 可不依赖 Publish）
```

### 逻辑名 → physical（评审补充）

- Register 以**逻辑名**为键（如 `kRDGSceneColor`）。
- Bake 后通过 `FindTextureResource(name)->GetPhysicalIndex()` 解析 slot。
- 若该逻辑资源经 **color input alias** 与另一资源共享同一 physical（`AllocatePhysicalForTexture` 别名路径），External 安装到该 **physical 下标**即可；以该名 Register 即覆盖该 physical。S01 测试用无别名的最小图即可；别名场景在 S01 加一条「Register SceneColor 后别名读者看到同一指针」的测试（若现有图构造成本高，可 Defer 到 S02，但实现须按 physical 安装而非「仅改名字映射」）。

### API（RenderGraph）

```cpp
/** 在活动 ViewKey 上绑定；同逻辑名再次 Register 则替换。须在 SetupAttachments 之前。 */
bool RegisterExternalTexture(
    const std::string& logicalName,
    const RHITextureRef& texture,
    std::string* outError = nullptr);

void UnregisterExternalTexture(const std::string& logicalName);

/** 清除活动 ViewKey 上全部 External（Reset / 丢弃 view cache 时亦调用）。 */
void ClearExternalTextures();
```

内部：

- 每 `ViewPhysicalCache`：`unordered_map<string, RHITextureRef> Externals`，或 Bake 后 physicalIndex → external。
- slot 标记 external vs 图创建，避免后续 SetupAttachments 在尺寸碰巧匹配时把 External「升级」成内部重建。
- Unregister 只清理由本次注册装进去的 slot：名字从未注册（或共享该 slot 的别名仍未解绑）时为空操作；否则 ForwardRenderer 每帧的 else 分支会让图自建槽每帧重建，推翻 Bake 保留槽位以便编辑器继续采样的意图。

### SetupAttachments 变更

对每个 physical 下标：

1. 若有逻辑资源映射到该下标且已 Register → 校验 `wantDesc` vs `texture->GetDesc()` → `slot = texture` → continue。
2. 否则走现有 create/reuse。
3. **删除** `swapchainOrNull` 参数（评审锁定 P1/OpenQ3）；调用方（ForwardRenderer、测试）同步改签名。

### 校验规则（硬）

外部纹理须**全部**满足：

| 字段 | 规则 |
|------|------|
| Width / Height | 等于 `m_PhysicalDims[i]` |
| Format | 相等 |
| Layers | 相等（2D → 1） |
| Flags | `(tex.Flags & requiredUsage) == requiredUsage`（`requiredUsage` 来自 `MakeCreateDesc` / dims.Usage） |
| Dimension | SceneColor MVP 为 Texture2D |

**推荐调用序：** `Bake` → `RegisterExternal*` → `SetupAttachments`。  
允许 Bake 前 Register，但**始终在 SetupAttachments 做完整校验**；校验失败则本帧失败（见失败模型）。

### ForwardRenderer / Offscreen

F27 **不改** Offscreen / ForwardRenderer 产品路径。DoD 以扩展 `RenderGraphTest`（或 `rdg-external`）为准。产品接线在 **F28**。

## 职责

| 单元 | 负责 | 不负责 |
|------|------|--------|
| `RenderGraph` | External 表、校验、跳过创建、Reset 时清理 | 创建 Texture2D、Offscreen UX |
| `ViewPhysicalCache` | 持有该视图的 external ref | 调用方主所有者的生命周期 |
| 调用方（F28 / 测试） | 创建可作 RT 的纹理；尺寸匹配；绘制期间保持 shared_ptr | 知道 physical 下标 |

## 边界

- **允许：** RDG ↔ RHITextureRef；测试调用 RegisterExternal。
- **禁止：** RenderGraph include `Texture.h` / Asset；游戏代码碰 physical 下标 API；尺寸不匹配时静默成功。

## 依赖

- **依赖：** F07/F17 级 RDG（ViewKey）— Done。
- **被依赖：** `RND-F28`。
- **不依赖** F28（先实现 F27）。

## 所有权与寿命

- 调用方持有 `shared_ptr`（F28 的 Texture2DRenderTarget，或测试中的 ref）。
- 图在 ViewKey 注册期间再持有一份 `shared_ptr`。
- Unregister / Clear / Reset / 销毁 view cache 释放图侧引用；GPU 资源活到最后一份所有者释放。
- **非法：** Register 后、在 SetupAttachments/Enqueue 完成前销毁全部外部所有者。

## 数据流

`调用方 RHITextureRef → RegisterExternal(逻辑名) → ViewCache.Externals → SetupAttachments 安装 physical → Pass 读写 →（可选）Publish 到 SRT`

## 控制流

同步，与今日 `SubmitSceneDraw` 相同。无新异步。

## 状态与不变量

- I1：每个 ViewKey 上每个逻辑名至多一个 External。
- I2：External slot 的 `shared_ptr` 在 Unregister/Clear/重建前等于注册纹理。
- I3：Clear 后非 External slot 不得残留 external 标记。
- I4：对本 Bake 中不存在的逻辑名 Register → 错误。

## API 语义

**`RegisterExternalTexture`**

- **职责：** 将逻辑名与纹理关联到活动 ViewKey。
- **输入：** 非空逻辑名；非空 texture。
- **输出：** 成功存储为 `true`；失败 `false` + 可选错误串。
- **所有权：** 共享；不独占窃取。
- **错误：** 空纹理；无活动 ViewKey；未知逻辑名；校验失败（可在 Setup 阶段才暴露）。
- **线程：** 仅渲染/游戏提交线程（与现图一致）。

## 失败模型（评审锁定）

| 失败 | 行为 |
|------|------|
| Register 时空指针 / 无 ViewKey | `false` + 错误；不改状态 |
| 尺寸/format/flags 不匹配 | SetupAttachments：**不**安装该 External、**不**内部 create 顶替该 slot；打 Error；返回表示失败或使后续 `GetPhysicalTexture` 为空，**调用方须视本帧绘制失败**（测试断言失败路径） |
| 缺少 RenderTarget 标志 | 拒绝 |
| Unregister 未知名 | 空操作 |

禁止「校验失败却 create 一张内部纹理假装成功」。

## 方案对比

### 方案 A（推荐）— 按逻辑名 RegisterExternal

- 优点：与 pass 声明方式一致；SceneColor 清晰；可不经 Offscreen 单测。
- 缺点：须在 Bake 后解析 logical→physical。

### 方案 B — 仅硬编码 Override SceneColor

- 未选：Depth 等还要第二套 API；机制不够诚实。

### 方案 C — 只做 blit、不改 RDG

- 未选为唯一方案：RDG 能力缺口仍在；F28 会永远过渡。

### 方案 D — 只实现 swapchainOrNull

- 未选：过窄；stub 已造成困惑；RegisterExternal 可覆盖。

## 集成影响

- `RenderGraph.h/.cpp`；可能小幅 `RDGTypes`。
- 扩展 `RenderGraphTest`。
- 删除 `SetupAttachments` 的 `swapchainOrNull` 并更新调用点。

## 兼容与迁移

- 默认路径不变。
- 无资产迁移。
- 签名变更可接受于 `feat/render`。

## 验证策略

- **套件：** 扩展 `test render-graph`（或 `rdg-external`）。
- 用例：
  1. 创建匹配尺寸的 RT → Bake → Register SceneColor → SetupAttachments → `GetPhysicalTexture` 指针 == `external.get()`。
  2. 尺寸不匹配 → 失败；图不得静默使用错误纹理或内部顶替成功。
  3. 第二视图未 Register → 仍为图创建纹理。
  4. Clear/Unregister → 下次 SetupAttachments 恢复内部创建。
  5. 既有 render-graph 用例回归。
- F27 DoD **不要求**手工 Offscreen 冒烟。

## 开放问题（评审后状态）

1. Bake 前是否允许 Register？— **允许；Setup 必校验。** 已接受。
2. S01 是否含 SceneDepth？— **否，S02 可选。** 已接受。
3. 是否同 PR 删除 `swapchainOrNull`？— **是。** 已接受。

## 设计评审

- **Verdict:** `Ready with deferred items`
- **Reviewer / date:** agent · 2026-10-25 · Mode A（与 F28 一并）
- **摘要：** 机制边界清晰，可测。收窄 R5（F27 不接 Offscreen）；锁定失败不得静默 create；logical→physical / alias 按 physical 安装。Defer：Depth External、别名专用测试若 S01 成本过高可进 S02。
- **阻塞实现？** 否（S01 可开），待主人确认升 `Planned`。

## 实现就绪

- [x] 需求 / 约束 / 架构 / 职责 / 所有权 / API / 失败 / 方案对比 / 验证
- [x] 开放问题已有默认并经评审锁定
- [ ] 主人确认后 Status → `Planned`，再写 Impl / 开工

**可否开工？** 主人确认后 **yes（仅 S01）**。

## 建议切片

| Slice | 内容 |
|-------|------|
| S01 | Register/Unregister/Clear + SetupAttachments 兑现 + SceneColor 测试；删除 swapchainOrNull |
| S02 | 可选 SceneDepth；alias 共享 physical 测试（若未进 S01） |
| S03 | （取消或并入 F28）产品路径接线 |

## 验收清单

- [x] 设计评审完成（本文）
- [x] 主人确认 / Status Planned（实现授权后直落 S01）
- [x] S01：Register/Unregister/Clear + SetupAttachments + SceneColor 单测；删除 swapchainOrNull
- [x] Unregister 后强制重建 graph-owned 物理槽（不得复用外部纹理指针）
- [x] `minEngineTests.exe test render-graph` PASS
- [x] 状态变更时同步 Progress / Registry
- [ ] S02（可选）：SceneDepth External；alias 共享 physical 专用测试
