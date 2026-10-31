# BUG-RENDER-023 — 记录流命名了没有句柄持住的几何，回忆放时后端拿到没有 native name 的缓冲

- **Status:** Fixed
- **First seen:** 2026-10-31，真机单线程与多线程各一条
- **Area:** Render / RHI 提交门禁

## 现象

```
[RHI] [Error] OpenGL draw skipped: the bound index buffer (0x…) has no native name, so the draw
would name no data (36 requested). ...
```

关键字面来自 `OpenGLRHI.cpp` 的 `ReportDrawWithoutNativeBuffer`，即
`m_BoundIndexBuffer != nullptr && GetBufferId() == 0`。

## 根因排查（先证伪，再定位）

三条常见猜测被代码证伪：

1. **显式 teardown 删掉 native 但句柄仍报可用** —— 不成立。缓冲的 native 名字只有一处会删：
   `~OpenGLRHIBuffer`（`OpenGLRHIResources.cpp:884-909`）。它在 `PostNativeDestroy` **之前**就把
   `m_BufferId` 置 0，而 `IsReady()` 正是 `m_BufferId != 0`（`OpenGLRHIResources.h:89`）。
   因此"活着的句柄 + 已删的 native"在这个后端里无法表示。
2. **延迟销毁批次尾部删了 native，就绪标志没同步** —— 同一处证据证伪：置 0 早于入队，方向相反。
3. **零尺寸/空数据创建返回 name 0 但句柄标就绪** —— 证伪：`CreateNativeBuffer` 失败就不会标记就绪，
   门禁按 `RHIDeferredUse::Pending` 拒绝（`RHICommandList.cpp` 的 `CanNameGeometry`）。

真正的不变式缺口在**门禁的判决时点**与**流持有什么**之间：

- `SubmitDrawBuffers` 的可命名判决是**录制时**做的，绘制在 `Replay` 时才跑；
- 流能把几何保活到 `Replay` 的唯一手段是 `MeshDrawPacket` 里的**句柄**
  （`m_RecordedGeometryHolds`）。一旦 packet 只填裸指针（帧视图）而没填句柄，
  流手上什么都没有：生产者放下最后一个句柄、后端登记延迟销毁、`Replay` 时就命名了一个
  已经退役的对象。门禁在录制时读到的是活的（可用），后端在回放时读到的是死的
  ——这正是日志里"门禁放过、GL 报没有 native name"的矛盾。
- 引擎自己的 Present / PostProcess / SkyBox 三个 pass 就是这么填的
  （只写 `packet.VertexBuffer`，句柄留在 pass 成员里），说明这条路径不是理论上的。
- 立即列表（Single 拓扑的栈列表）是另一回事：命令在调用内跑完，生产者自己的引用覆盖了它，
  裸视图没问题；问题只出在"录制后延后回放"。

## 修复

1. **门禁**（`RHICommandList.cpp` `SubmitDrawBuffers`）：录制中的流要求它要绘制的几何
   **有句柄可持**（`canKeepGeometry`）。只有裸视图的几何会被拒绝、独立计数、
   按 1/100 限频报 Error，文案指出"把 packet 的句柄填上，而不是只填帧视图"。
   判决在 `CanNameGeometry` 之前做：持不住的东西不该再被解引用。
   立即列表不受此约束（命令当场执行）。
2. **生产者补齐**：Present / PostProcess / SkyBox 三个 pass 在填帧视图的同时填句柄
   （`m_DrawPacket.VertexBufferRef = m_ScreenQuadVertexBuffer` / `m_CubeVertexBuffer`）。
   这些都是 pass 已经持有的 `RHIBufferRef`，改动只是把同一个句柄交给流。

## 为什么不会再绕过门禁

- 流能记录的每一条几何都被它自己持住 → `Replay` 时句柄还活着 → native 名字不可能在
  录制与回放之间消失（唯一删除者是句柄析构，见上第 1 条）；
- 持不住的几何在录制阶段就被拒（有计数、有限频日志），根本不会进流；
- 立即列表的判决与绘制在同一次调用内，中间没有可插入 teardown 的时点。

## 回归检查

- `draw-packet-lifetime` 新增："a recorded stream refuses geometry it cannot keep alive"
  （裸视图 + 录制 → 不记录、可计数、有 Log；同一 packet 走立即列表仍提交，证明没有误伤
  Present 类绘制）。红灯（临时关掉该门禁）：`CHECK( rhi.Draws.empty() )` → `values: CHECK( false )`。
- 既有用例（布局未实体化被拒、录制流持有几何/布局直到 Replay、文字布局交接等）保持通过；
  `rhi-deferred-destroy` 16 / 306、`scene-proxy-lifetime` 12 / 194、
  `sprite-translucency` 2 / 7、`mesh-assembly-determinism` 20 / 1682 全 PASS。

## 遗留

- 按裸视图直接绑定、绕过 `SubmitDrawBuffers` 的调用点（`EnvMapCapture`、`DebugDrawPass` 的
  `SetVertexBuffer` + `Draw`）不受这条门禁约束：它们靠调用点自身的引用覆盖，
  没有"录制后延后回放"的间隔，若将来改成跨帧回放需要重新审。
- 顶点输入布局仍允许只用裸指针进流（进程级共享句柄的寿命长于帧，现有生产者都带句柄）。
- 本树没有"显式销毁 native"的入口，所以本次没有引入新的销毁 API；若将来出现，
  必须同时把句柄置为不可用，否则本次修复的不变式会被绕过。
