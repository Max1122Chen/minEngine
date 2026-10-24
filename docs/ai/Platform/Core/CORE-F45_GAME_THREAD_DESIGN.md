# CORE-F45 — Game Thread 重构 Design

## Meta
- **ID:** `CORE-F45`（子 Design · Game）
- **Parent:** [综述](./CORE-F45_FRAME_THREAD_SEPARATION_DESIGN.md)
- **Status:** `In Progress`（维护者 ACK 后已重写 S01/S02；验收见下）
- **Last updated:** `2026-10-24`
- **Slices:** S00（配合）/ **S01（Proxy 推送）** / **S02（ViewIntent）** 主责
- **UE 源码树（只读）：** `D:/Dev/GitRepo/UnrealEngine`
- **主锚点文件：**
  - `Engine/Source/Runtime/Renderer/Private/RendererScene.cpp`
  - `Engine/Source/Runtime/Renderer/Private/ScenePrivate.h`（`FPrimitiveUpdateParams` / `FUpdateLightTransformParameters` / `FUpdateLightColorParameters`）
  - `Engine/Source/Runtime/Engine/Private/Components/ActorComponent.cpp`（`MarkRenderStateDirty` / `MarkRenderTransformDirty` / `RecreateRenderState_Concurrent`）

---

## 角色

**Game = 主窗口线程**：`PollEvents`、仿真、GO 所有权；经 **`EnqueueRenderCommand`** 把场景对象变更交给 Render。不 submit GPU。

**硬规则：** 对**已登记进 RenderScene 列表**的 Proxy，Game **不得**直接写其热字段；变更只能：  
① 专用**小参数** Enqueue → RT Apply；或 ② **Recreate**（Destroy 旧代理登记 + Create 新代理 + Add）。

**Mode：** Single / Multi 同一套 Game API（综述）。

---

## 从 UE 学到的模型（必须先懂再实现）

### 1) 对象在哪线程创建？热数据何时写入？

| 阶段 | 线程 | UE 做什么 | minEngine 对应 |
|------|------|-----------|----------------|
| **Create 代理对象** | **Game** | `CreateSceneProxy()`；可在 GT 填构造期数据 | 同左 |
| **登记进 Scene / SetTransform 初值 / CreateRenderThreadResources** | **Render** | `ENQUEUE_RENDER_COMMAND(AddPrimitiveCommand)` 捕获 **proxy\* + 初值矩阵/Bounds** | `Enqueue Add*_RT(proxy*)`；初值 transform 可随 Add 小包或 Add 后第一次 Transform 更新 |
| **之后改 transform** | Game 打包 → Render 写 | `UpdatePrimitiveTransform` → `FPrimitiveUpdateParams` | 见下节 Primitive |
| **改「代理布局/资源语义」** | Game | `MarkRenderStateDirty` → EOF **`RecreateRenderState_Concurrent`** = Destroy + Create | `MarkRenderStateDirty` → Remove+Create+Add |

要点：**Create ≠ 入 Scene**。Create 可以在 GT；**入列表 / 对已入 Scene 的热写在 RT**。

### 2) Dirty 不是一种，至少两种（UE）

摘自 `UActorComponent`：

| API | 效果 |
|-----|------|
| `MarkRenderTransformDirty()` | EOF **Update**（走 `UpdatePrimitiveTransform` / `UpdateLightTransform` 等） |
| `MarkRenderStateDirty()` | EOF **Recreate**（`RecreateRenderState_Concurrent`：DestroyRenderState + CreateRenderState） |
| `MarkRenderDynamicDataDirty()` | EOF 动态数据更新（本 Feature MVP **可后置**；骨骼等可先 Recreate 或后续专用路径） |

**禁止**把所有变更都当成「打一个全家桶 Snapshot 再 Enqueue」。

### 3) 快路径 = **具名小参数结构**，不是「该家族所有字段」

UE 例：

```text
// ScenePrivate.h
FPrimitiveUpdateParams {
  Scene*, PrimitiveSceneProxy*,
  WorldBounds, LocalBounds, LocalToWorld,
  PreviousTransform?, AttachmentRootPosition
}

FUpdateLightTransformParameters { LightToWorld, Position }
FUpdateLightColorParameters    { NewColor, NewIndirectLightingScale, NewVolumetricScatteringIntensity }
```

Light 其它大量属性变更 → **MarkRenderStateDirty → Recreate**，而不是扩成 `LightUpdateSnapshot` 巨型 POD。

### 4) Recreate 的形状（UE）

`RecreateRenderState_Concurrent`：若已有 render state → `DestroyRenderState_Concurrent`；再 `CreateRenderState_Concurrent`（内部再 `CreateSceneProxy` + `Scene->Add*`）。

`UpdatePrimitiveTransformInternal`：若 `ShouldRecreateProxyOnUpdateTransform()` → **`RemovePrimitive` + `AddPrimitive`**（整颗重建）。

### 5) Sky（UE）

`SetSkyLight(FSkyLightSceneProxy*)` / `DisableSkyLight(proxy*)`：Enqueue **只带 proxy 指针**，在 RT 改 Sky 栈/当前指针。  
**不是**每帧拷贝 Sky 全字段 Snapshot。

### 6) 反模式（实现曾踩坑 · 永久禁止）

| 反模式 | 为何不是 UE |
|--------|-------------|
| `CreateEmpty` + `SyncSceneProxy(scratch)` + `CopyHotStateFrom` | 把全量 Sync 换线程执行 |
| `LightUpdateSnapshot` / `SkyBoxUpdateSnapshot` / `WidgetUpdateSnapshot` **全家桶** | UE 用**具名小更新**或 **Recreate** |
| Single 下在 `Update*` 里直接写已登记 Proxy 热字段 | 旁路队列；破坏 N−1 / Multi |
| 一个中央 `SceneProxyUpdateCommand` 巨石 | 已否决 |

---

## minEngine 目标 API 形状（S01 锁定）

保留 Component → `RenderScene` 入口，但**入口语义按 UE 拆清**（可仍由一个 `UpdatePrimitive` 内部分流，或拆成显式方法；**推荐显式**，减少再偏）：

### Primitive

| 操作 | Game | Enqueue 载荷 | RT |
|------|------|--------------|-----|
| **Add** | `CreateSceneProxy()`（GT，含 Create 内 Sync） | `{ proxy* }` 或 `{ proxy*, initialLocalToWorld, … }` | 挂 `m_Primitive*` / owners；可选 `SetTransform` |
| **UpdateTransform** | 读 Component → **仅** `PrimitiveTransformUpdateParams` | 小 POD | 只写 transform / castShadow / enabled（及日后 Bounds） |
| **Recreate** | 等价 Destroy+Create：Enqueue Remove(old) → GT Create → Enqueue Add(new) | 指针 | 摘旧挂新 |
| **Remove** | | `{ component* 或 proxy* }` | 摘列表 + destroy owner |

```text
PrimitiveTransformUpdateParams（对齐 FPrimitiveUpdateParams 的 MVP 子集）
  PrimitiveSceneProxy* Proxy
  Transform             LocalToWorld   // 或 Matrix4
  bool                  CastShadow
  bool                  Enabled
  // 日后可加：WorldBounds / LocalBounds（本切片可不实现）
```

**分流规则：**

```text
if 无 proxy → Add
else if ShouldRecreateSceneProxy() || MarkRenderStateDirty 本帧 → Recreate
else if MarkRenderTransformDirty → UpdateTransform
else → no-op
```

`ShouldRecreateSceneProxy()`：对齐 `ShouldRecreateProxyOnUpdateTransform` + State dirty；**默认 State dirty ⇒ Recreate**；仅 Transform dirty ⇒ UpdateTransform。  
mesh / material / 子类型 / 骨骼资源布局 ⇒ **必须 State dirty → Recreate**。

**禁止** `UpdatePrimitive` 路径调用 `SyncSceneProxy` 去更新**已登记** proxy。`SyncSceneProxy` **仅**允许在 `CreateSceneProxy`（GT，尚未 Add）内使用。

### Light（对齐 Add / UpdateTransform / UpdateColor / Recreate）

| 操作 | Game | 载荷 | 说明 |
|------|------|------|------|
| **Add** | `CreateSceneProxy()`（GT 填类型相关字段） | `{ proxy*, LightType }` | RT 只挂表 |
| **UpdateTransform** | | `{ proxy*, Position, Direction? }` 等**小包** | 对齐 `UpdateLightTransform` |
| **UpdateColor** | | `{ proxy*, Color, Intensity, DiffuseFactor, SpecularFactor }` **小包** | 对齐 `UpdateLightColorAndBrightness`（字段按 minEngine 现有代理裁剪） |
| **其它**（类型变、锥角/衰减等布局敏感） | `MarkRenderStateDirty` | Recreate = Remove+Create+Add | **不要**打进 Color/Transform 小包 |
| **Remove** | | component* | 摘三类列表 + owners |

Component setter 映射（实现必须遵守）：

- 变换 / 方向（Dir）→ `MarkRenderTransformDirty` → `UpdateLightTransform`
- 颜色 / Intensity / Diffuse / Specular → `MarkRenderColorDirty` 或复用 Transform EOF 中的 **Color 小包路径**（可单独 dirty 位）→ `UpdateLightColor`
- Cone / Attenuation / LightType / CastShadow 策略若影响代理布局 → **State → Recreate**（MVP：宁可 Recreate，勿 Snapshot）

**禁止** `LightUpdateSnapshot` 一次拷贝全部字段。

### SkyBox（对齐 SetSkyLight / Disable）

| 操作 | Game | 载荷 |
|------|------|------|
| **Set / Add** | `CreateSceneProxy()`（GT 填齐） | Enqueue `{ proxy* }` → RT 设为当前 Sky（替换旧） |
| **Disable / Remove** | Detach | Enqueue `{ component* 或 proxy* }` → RT 清空 |
| **属性变更**（强度/朝向/EnvMap） | MVP：**Recreate**（Remove+Create+Set） | 或日后若有高频字段再加**具名**小包；**禁止** Sky 全家桶 Snapshot |

### Widget（无直接 UE 对应 · 用同一哲学）

| 操作 | MVP |
|------|-----|
| Add | GT Create（Fill）→ Enqueue Add(proxy*) |
| 布局/材质/网格变更 | **Recreate**（Remove+Create+Add） |
| Remove | Enqueue Remove |

Widget **暂不**做全家桶 Snapshot；若日后证明某字段高频，再单开 `UpdateWidgetLayoutParams` 之类**具名**小包，并写进 Design 修订。

---

## Dirty 标记（minEngine · 对齐 UE 命名）

| API | 谁用 | EOF 行为 |
|-----|------|----------|
| `MarkRenderTransformDirty()` | 变换、可见/enabled 位、CastShadow 位（若进 Transform 小包） | 家族 **UpdateTransform**（或 Light Transform） |
| `MarkRenderColorDirty()`（可选；或并入 Light 专用） | Light 颜色类 | **UpdateColor** |
| `MarkRenderStateDirty()` | mesh/material/类型/锥角/Env 种类/Widget 拓扑等 | **Recreate** |

- `NotifyLocalTransformChanged` → **`MarkRenderTransformDirty`**（不要一律 State）。
- EOF：`DoEndOfFrameUpdate` 按 dirty 位调用 RenderScene 的 **Add/UpdateTransform/UpdateColor/Recreate/Remove**，而不是「一个 Update* 打 Snapshot」。

---

## Enqueue / 帧序（与综述一致）

- 载体：`EnqueueRenderCommand(std::function)` + `FrameIndex`（Decision 12–13）。
- RenderRole：**Take ViewIntent(N) → 画 World(N−1) → FlushApply(N) → publishedWorldGen=N**（S02）。
- 已在 Render 且非 Game（未来 Multi）：可同步执行可调用体。
- 无 Engine 管道的单测：允许 inline Apply（工具旁路）。

有界：`kMaxPendingRenderCommands = 4096`，`kMaxPendingViewIntents = 3`（综述 D18）。

---

## ViewIntent（S02）

- `SubmitSceneDraw` → `SubmitViewIntent`（按 logicFrame）。
- 演进自今日 `SceneDrawDesc`；满阻塞。

---

## 与错误 WIP 的关系

工作树中曾出现的 `*UpdateSnapshot` 全家桶、`CreateEmptySceneProxy`、`CopyHotStateFrom`、以及「Update* 内 Sync-scratch」：

- **与本 Design 冲突**；
- **ACK 后实现必须以本文为准整段重写**相关路径，不得在 Snapshot 模型上打补丁。

---

## 代码落点（实现清单）

| 区域 | 要求 |
|------|------|
| `RenderCommandPipe` | Enqueue / ViewIntent / Flush / 世代（可保留） |
| `RenderScene` | 显式 Add / UpdateTransform / UpdateColor(Light) / Recreate / Remove；**删除**全家桶 Snapshot 类型 |
| `SceneComponent` / Primitive / Light | Transform vs State（+ Color）dirty；EOF 分流 |
| `CreateSceneProxy` / `SyncSceneProxy` | 仅 Create/Recreate 的 GT 新建路径 |
| Profiler | `Frame.Game.Logical` / `Frame.Render.View|Record|Apply` |

---

## 验收（ACK 后实现必须满足）

- [x] Primitive：仅 **UpdateTransform 小包** 或 **Recreate**；无 Sync-scratch；无 `CopyHotStateFrom` 推送
- [x] Light：仅 **UpdateTransform / UpdateColor 小包** 或 **Recreate**；无 `LightUpdateSnapshot` 全家桶
- [x] Sky：Set/Disable（指针）或 Recreate；无 Sky 全家桶 Snapshot
- [x] Widget：Add/Remove/Recreate；无 Widget 全家桶 Snapshot（除非未来 Design 显式增加具名小包）
- [x] `RenderScene` Update 路径 **不**对已登记 Proxy 调 `SyncSceneProxy`
- [x] Single smoke + `frame-threading` / `render-command-pipe` 绿
- [x] 先画后 Apply（N−1）断言/行为符合综述

## 兄弟边界

- RT Apply / 世代 / 先画后 Apply → [Render](./CORE-F45_RENDER_THREAD_DESIGN.md)
- List → [RHI](./CORE-F45_RHI_THREAD_DESIGN.md)

## 变更记录

| Date | Change |
|------|--------|
| 2026-10-24 | 拆出；家族 Enqueue；Mode A |
| 2026-10-24 | 初版「Transform vs Recreate」仍偏薄，实现滑向 Snapshot |
| 2026-10-24 | **加厚：严格对齐 UE**（Create≠登记、Dirty 二分、具名小包、Light Transform/Color、Sky 指针、禁全家桶 Snapshot）；Status→Review 待 ACK |
| 2026-10-24 | 维护者 ACK；实现改为显式 Add/UpdateTransform/UpdateColor/Recreate/Remove；删全家桶 Snapshot；S01+S02 落地 |
