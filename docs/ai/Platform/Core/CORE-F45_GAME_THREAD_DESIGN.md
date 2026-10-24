# CORE-F45 — Game Thread 重构 Design

## Meta
- **ID:** `CORE-F45`（子 Design · Game）
- **Parent:** [综述](./CORE-F45_FRAME_THREAD_SEPARATION_DESIGN.md)
- **Status:** `Review`（待维护者 ACK；随综述 Ready）
- **Last updated:** `2026-10-24`
- **Slices:** S00（配合）/ S01 / S02 主责
- **UE：** `ENQUEUE_RENDER_COMMAND`；按家族 Add/UpdateTransform/AddLight/SetSkyLight

## 角色

**Game = 主窗口线程**：`PollEvents`、仿真、GO 所有权、经 **`EnqueueRenderCommand`** 把各家族 Proxy 变更交给 Render。不 submit GPU，不在 GT 写 Proxy 热字段。

**Mode：** Single / Multi 同一套 Game API（综述）。

## 今日 → 目标

| 今日 | 目标 |
|------|------|
| `RenderScene::UpdatePrimitive/Light/SkyBox/Widget` 内 Create 或 **当场 Sync 热字段** | 同名 API 仍由 Component 调用；内部改为 Create（若需）+ **Enqueue**；热写仅 RT |
| 仅 Primitive 在文档里被强调 | **四家族一等公民**（与 `RenderScene.h` 一致） |
| （曾草案）中央 `SceneProxyUpdateCommand` 巨石 | **撤回** — 难扩展；见下节 UE 对齐 |

## 职责表

| 做 | 不做 |
|----|------|
| PollEvents、Logical、Physics、ScreenUI 仿真、Drain | Cull/Sort/Batch；Present |
| `CreateSceneProxy`（各 Component 虚函数） | GT 上 `SyncSceneProxy` / 手写 Light 热字段 |
| 调用 `RenderScene::Update*` / `Remove*`（语义入口） | 直接改 `m_*SceneProxies` 列表 |
| `SubmitViewIntent` | 假设同帧已画最新逻辑态 |

---

## Proxy 推送：是否学正了 UE？

### UE 实际做法（源码锚点）

| 机制 | 锚点 | 含义 |
|------|------|------|
| GT `CreateSceneProxy`，再 `ENQUEUE_RENDER_COMMAND(AddPrimitiveCommand)` | `RendererScene.cpp` `BatchAddPrimitivesInternal` | Proxy **对象**可在 GT 建；**入 Scene / SetTransform / CreateRenderThreadResources** 在 RT |
| Transform 快路径：打 `FPrimitiveUpdateParams`（矩阵/Bounds/Proxy*）再 Enqueue | 同文件 `UpdatePrimitiveTransformInternal` | **小参数包**，不是「所有属性一个大 struct」 |
| 需重建：`ShouldRecreateProxyOnUpdateTransform` → Remove+Add | 同上 | 复杂变化走重建，不塞进 UpdateParams |
| Light：GT Create + 填 proxy，Enqueue `FAddLightCommand` 捕获 `LightSceneInfo*` | `AddLight` | **另一家族另一条命令** |
| Sky：`SetSkyLight` / `DisableSkyLight` 独立 Enqueue | 同文件 | 再一家族 |

**结论：** UE 学的是「**命令管道 + 按场景对象家族分流 + 快路径小快照 / 慢路径重建**」，**不是**「一个可扩展字段无限膨胀的统一 Payload」。

### 曾草案问题（已否决）

中央 `SceneProxyUpdateCommand { Op, Matrix, enabled, mesh… }`：

- Light cone / Sky cubemap / Widget 布局塞不进也不该塞进同一结构；
- 每加家族就改中央类型 → 难维护；
- 与现有 `RenderScene` 已分好的 UpdatePrimitive/Light/SkyBox/Widget **打架**。

### minEngine 锁定口径（S01）

1. **保留** `RenderScene` 家族 API 为 Component 侧唯一入口（已存在）：
   - Primitive：`UpdatePrimitive` / `RemovePrimitive`
   - Light：`UpdateLight` / `RemoveLight`
   - Sky：`UpdateSkyBox` / `RemoveSkyBox`
   - Widget：`UpdateWidget` / `RemoveWidget`
2. 各 `Update*` 在 **Game** 上：
   - 若无 Proxy → `CreateSceneProxy()`（类型由 Component 决定）；
   - 判定 **Recreate vs 快路径 Update**（见下）；
   - **`EnqueueRenderCommand`** 投递 **该家族专用** 的小参数或 lambda，在 Render 上执行「挂列表 / 写热字段 / Remove」。
3. **扩展新家族**（将来 Decal 等）：加 `RenderScene::UpdateFoo` + 对应 RT apply + 自己的小参数类型；**不改**公共巨石。
4. 管道原语只有一个：`EnqueueRenderCommand`（对齐 UE 宏语义）；**命令内容按家族分散**。

```text
Component 脏
  → RenderScene::UpdateX(component)          // Game，家族入口
       → CreateSceneProxy?（GT）
       → EnqueueRenderCommand([小参数或 proxy*]{ RenderScene::Apply* _RT(...) })
  → Render Flush：执行 Apply*_RT（写热数据 / 列表）
```

---

## 脏级别与快路径（MVP 锁定 · Decision 17）

| 档 | Game | RT |
|----|------|-----|
| **Add / Recreate** | 若已有 Proxy → Enqueue Remove；`CreateSceneProxy`；Enqueue Add | 挂列表 + 初始热字段 |
| **UpdateFast** | 读 Component → **家族小快照**；Enqueue | 只写热字段 |
| **Remove** | Enqueue Remove | 摘列表 + retire |

### 何时 Recreate（MVP；不清则 Recreate）

| 家族 | UpdateFast（示例） | **必须 Recreate** |
|------|-------------------|-------------------|
| Primitive | world transform、CastShadow、enabled/visible | mesh/asset 指针变；material 实例换「代理布局」；Proxy 子类型变 |
| Light | position、color、intensity、factors、castsShadow、enabled；Dir direction；Point/Spot 衰减与锥角 | LightType 变；Proxy 子类型变 |
| SkyBox | transform/orientation、enabled；贴图 **句柄** 若代理布局不变 | Sky 资源种类/代理子类型变 |
| Widget | 与今日 Sync 等价的布局/可见/材质句柄（代理布局不变时） | Widget 代理子类型 / 几何拓扑变 |

禁止用中央巨石 POD；各家族可自有小 struct 或 lambda 捕获。扩展 = 新 `UpdateFoo` + 新 Apply，不改旧家族布局。

---

## Enqueue 语义与 Mode（Decision 12–13）

| 调用点 | Single | Multi |
|--------|--------|-------|
| Game | **入队**；RenderRole 头 Flush | SPSC 入队；满阻塞 |
| 已在 Render | 同步执行可调用体 | 同左 |

- 载体：**可调用体**（lambda / `TFunction`），管道包装 `FrameIndex`。
- **禁止** Single 在 `Update*` 内直接写热 Proxy。

---

## ViewIntent / TaskSystem / UI / 窗口

- `SubmitViewIntent`：演进 `SceneDrawDesc`；深度见综述 **D18**；满阻塞。
- Drain 仅 Game；Worker 可 Enqueue，禁直接 apply。
- ScreenUI 仿真 Game；绘制后置。
- 建窗 + `PollEvents` = Game。

## Profiler（Game）

`Frame.Game.Logical` / `Enqueue` / `ViewIntent` / `WaitRenderQueue`（综述）。

## 代码落点

| 区域 | 变化 |
|------|------|
| `RenderScene.cpp` 四家族 Update/Remove | GT 入口瘦身；新增 `Apply*_RT`（或匿名仅 RT 调用） |
| `PrimitiveComponent::SyncSceneProxy` 等 | 热写改由 RT apply；或降级为「填快照」助手且仅 GT 读 Component |
| Components 标脏 | 仍调 `Update*`；可选 `MarkRenderStateDirty` → 帧末 Flush |
| `EnqueueRenderCommand` | 新管道 |

## 验收

- [ ] Primitive **与** Light/Sky/Widget 均经 Enqueue；无 GT 热写
- [ ] 无中央巨石 payload 类型
- [ ] 新家族可加方法而不改旧家族命令布局
- [ ] Single/Multi 同 API；smoke（Single）绿
- [ ] Profiler 区段对称

## 兄弟边界

- Apply*_RT / 世代 / 先画后 Apply → [Render](./CORE-F45_RENDER_THREAD_DESIGN.md)
- List → [RHI](./CORE-F45_RHI_THREAD_DESIGN.md)

## 变更记录

| Date | Change |
|------|--------|
| 2026-10-24 | 拆出；加厚 |
| 2026-10-24 | **纠偏 Proxy 推送：** UE 按家族 Enqueue；撤回统一 POD；四家族写明 |
| 2026-10-24 | Mode A：锁 Recreate MVP 表；Enqueue 入队+Flush；随综述 Ready 待 ACK |
