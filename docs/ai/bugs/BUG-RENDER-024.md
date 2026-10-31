# BUG-RENDER-024 — 门禁解引用了已释放的几何：packet 只带裸帧视图时，任何模式下都不许再问对象

- **Status:** Fixed
- **First seen:** 真机 Single 拓扑，PIE 退出时崩溃
- **Area:** Render / RHI 提交门禁

## 现象

```
FaultThread: role=Game+Render+RHI
ExceptionCode=0xC0000005  ExceptionAddress=0x0  FaultRIP=0x0
Stack[0] libminEngine.dll+0x6AC44A -> minEngine::GetDeferredUse(RHIDeferredCreateTarget const*)
          RHIDeferredCreate.h:139
```

`FaultRIP=0x0` 表示虚调用目标为 0：对象已经释放，它的内存被别人接管，第一个字（vtable 指针）
是 0，于是 `target->HasNativeCreateFailed()` 跳到地址 0。

## 根因

上一轮（BUG-RENDER-023）把"流要能保活几何"做成了**录制模式**的条件，
并明确写下"立即列表的包在调用内跑完，生产者的引用覆盖它，裸视图没问题"。这条判断错在
**包的寿命**，不在命令的寿命：

- 立即列表的 packet 也可以是**跨帧容器**里的包（`BasePass::m_DrawPackets` 这类成员），
  生产者（场景代理、chunk 网格、widget）在自己的更新里放下了最后一个句柄；
- packet 只填了裸帧视图 `packet.IndexBuffer`，没填 `IndexBufferRef`，于是没有任何人持有对象；
- 门禁为了判断"能不能命名"去问 `GetDeferredUse(packet.IndexBuffer)` ——**先解引用，再问**。
  这一步本身就是崩溃点，与命令什么时候跑无关。

结论：判据的正确顺序不是"先看能不能用，再看有没有句柄"，而是
**先纯数据地问有没有句柄；有句柄才允许碰对象；没有句柄直接拒绝并计数**。
门禁碰不到对象，就不会碰到已释放的对象。

## 修复

1. `RHICommandList::SubmitDrawBuffers` 拆成两段：
   - 第一段只读 packet 自身字段：索引缓冲、顶点缓冲、顶点布局**有没有句柄**。
     任何一项只有裸视图 → 拒绝、按（几何种类 × 命令路线）计数、限频报错，**不再读裸视图**；
   - 第二段只用句柄取对象（`…Ref.get()`），句柄在 packet 里就是所有者，对象一定活着；
     再问 `GetDeferredUse` 是否可用。
2. 两条路线一律如此：立即列表的包可能来自跨帧容器，录制流回放时 packet 早已析构。
3. 引擎内部造包路径补齐句柄：Present / PostProcess / SkyBox 三个 pass 原来只把顶点布局挂在
   PSO 上（裸指针），现在同时填 `VertexInputLayoutRef`；EnvMapCapture 的三处裸绑定
   （`SetVertexBuffer` + `Draw`）改走 `SubmitMeshDrawPacket`，并补上捕获网格的就绪检查。
4. 计数按（几何种类 × 路线）分槽：修好一条路线的生产者不会把另一条从报告里藏掉。

## 回归

- `Tests/Suites/DrawPacketLifetimeTest.cpp`
  - `a packet that names a released object is refused, never dereferenced`：
    构造"对象已释放、内存被清零（vtable 指针为 0）"的裸视图 packet，立即与录制两条路线各提交；
    修复前 SIGSEGV 于 `RHIDeferredCreate.h:139`（gdb 栈：`GetDeferredUse` ← `CanNameGeometry`
    ← `SubmitDrawBuffers`），修复后被拒绝、无绘制、计数报告出现；
  - `a packet that names a released vertex buffer or layout is refused too`：同形状的顶点缓冲与布局；
  - `every packet shape the engine's own passes build is submitted`：引擎各造包形状（场景网格 /
    阴影 / 半透 / ScreenUI / Present / PostProcess / SkyBox）在正常帧里不被误拒，拒绝计数为 0；
  - `geometry handed over without a handle is refused on either route`：两条路线都拒绝，
    补上句柄后同一个 packet 立刻可画。
- 仍然成立的既有不变式：`draw-packet-lifetime`、`scene-proxy-*`、`screen-ui-*`、
  `mesh-assembly-determinism`、`rhi-*` 全绿。

## 遗留

- PSO 描述里的 `RHIGraphicsPSODesc::VertexInputLayout` 仍是裸指针：包**完全没有**布局时门禁会退到
  它（这是"布局由流水线状态绑定"的设计）。该指针的寿命跟着创建 PSO 的人，改成句柄是 RHI 接口
  变更，本缺陷未涉及。
- `DebugDrawPass` 仍直接用 `SetVertexBuffer` 记录裸绑定（它不造包）：几何由 pass 自己持有到
  回放，但它是渲染器里最后一条裸绑定录制路径。
