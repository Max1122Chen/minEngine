# UI-GAME-VIEWPORT-QUERY 实现记录 — 游戏面视口尺寸 + 实时指针坐标（零新增导出）

- 日期：2026-06（本次交付）
- 范围：`Runtime/Function/UI/UISystem.*`、`Runtime/Function/UI/ScreenUIHitTypes.h`、`Tests/Suites/ScreenUIGameViewportTest.*`、`Tests/TestSuiteRegistration.cpp`
- 不在范围：`Render/ScreenUI/ScreenUICoords.*`、`GLFWWindowSystem.*`、`WindowSystem.h`、`Editor/**`、`minEngine/cmake/minEngine_exports.def`（均未改动）

## 1. 问题与目标

独立打包的 Game / 宿主拿不到「ScreenUI 视口尺寸」和「指针实时坐标」：能读的只有窗口像素
（`WindowSystem::Get().GetWidth()/GetHeight()`，已导出），而把鼠标换算到 Canvas reference px 的
Letterbox 基建（`ScreenUICoords::MakeLetterboxMapping` / `LetterboxMapping::MapPoint/TryUnmapPoint`）
虽然齐备，却没有任何对外读取面；`ScreenUIPointerState` 只有 Hovered/Pressed/Clicked，没有坐标。

目标：把三件事做成**可在宿主内联读取**的引擎能力，且**不新增任何导出符号**。

## 2. API 契约（全部 inline，零导出）

```cpp
// Runtime/Function/UI/ScreenUIHitTypes.h
struct ScreenUIPointerViewportSample
{
    Vector2 ReferencePx{ 0.0f, 0.0f };      // Canvas reference px；bValid 为 false 时无意义
    Vector2 ReferenceDeltaPx{ 0.0f, 0.0f }; // 与上一个有效的 ReferencePx 之差（reference px）
    bool    bValid = false;                 // false：无 viewport context / 无指针帧 / 落在 letterbox 黑边
    void Reset();
};

// Runtime/Function/UI/UISystem.h（UISystem 成员，inline）
Vector2 GetViewportSizeWindowPx() const;
ScreenUICoords::LetterboxMapping GetReferencePxMapping(const CanvasComponent* canvas) const;
bool TryUnmapViewportPointToReferencePx(const CanvasComponent* canvas,
                                        const Vector2& viewportPointWindowPx,
                                        Vector2& outReferencePx) const;
const ScreenUIPointerViewportSample& GetPointerPosition() const;   // 纯读取缓存，tick 内更新
```

契约细节：

| 读取面 | 语义 | 无效时的行为 |
| --- | --- | --- |
| `GetViewportSizeWindowPx()` | ScreenUI 视口尺寸，**窗口像素**（与鼠标坐标、命中测试同一空间） | 无注入 context 且无平台窗口 → `(0,0)` |
| `GetReferencePxMapping(canvas)` | reference px → viewport 窗口 px 的 Letterbox 映射，内部就是 `ScreenUICoords::MakeLetterboxMapping` | `canvas == nullptr` / 参考分辨率或视口非正 → `Scale = 0`（**不返回被 sanitize 成 1:1 的假映射**） |
| `TryUnmapViewportPointToReferencePx(...)` | 上一行的逆运算（`LetterboxMapping::TryUnmapPoint`） | 返回 false 且**不写** `outReferencePx`（黑边 / 无映射） |
| `GetPointerPosition()` | 上一指针帧的 reference px + 帧间 delta + 有效性位 | `bValid == false`，两个向量都清零（不返回假坐标、不留旧坐标） |

尺寸取值优先级只有一条，写在 header 内联实现里：
注入的 `ScreenUIPointerViewportContext`（valid）> 平台窗口尺寸 > `(0,0)`。
宿主把 ScreenUI 画进子矩形（编辑器 PIE 视口、自定义 render target）时以注入为准；独立宿主下等于整窗。

## 3. 引擎侧自动注入 viewport context

**注入点：`UISystem::Tick`（`Runtime/Function/UI/UISystem.cpp`），不在 Editor。**

理由：

1. `Engine::LogicalTick` 是**所有宿主共用**的那一个帧入口（独立 Game、`Engine::Run`、编辑器 PIE 都经它），
   而它在 Physics 之后调 `UISystem::Tick`；在 `Tick` 里补默认 context，等于在「UI 子系统自己的帧」里补，
   不依赖任何宿主是否记得注入。
2. `UISystem::PollPointer` 早退条件就是 `!m_bPointerRoutingEnabled`，Tick 是唯一每帧必经点。
3. 编辑器已经是「自己注入 + 自己 Poll」的合法外部所有者，把它搬进 Editor 反而要多改一处、且拿不到独立宿主。

所有权（ownership）语义，防止引擎默认值踩掉编辑器视口：

- `m_bViewportContextFromEngine` 默认 `true`（引擎拥有 = 整窗）。
- `SetPointerViewportContext()`（已导出，编辑器/测试在用）把所有权交给调用方（置 `false`）。
- `ResetPointerViewportContext()`（`Initialize` / `Shutdown` / `OnEndPIE` 调用）把上下文与指针样本清零并把所有权交还引擎。
- 引擎拥有时每帧按窗口尺寸重算（窗口 resize 因此自动反映）；**外部拥有时引擎一个字节都不写**。

PIE：`OnBeginPIE` → 路由开启 → 编辑器视口窗口仍按帧注入子矩形（优先级更高）；若某宿主只进 PIE 而不注入
（无编辑器视口窗口的路径），引擎默认的整窗 context 已经在 `Tick` 里生效。standalone：`OnBeginPIE` 从未被调用，
路由默认关闭，引擎在 `Tick` 里只**采样不派发**（`UpdatePointerSample`），因此独立 Game 也能读到指针坐标。

## 4. 坐标系一致性（硬要求）

只有一条推导：`UISystem::DerivePointerReferencePx(scene, outHit, outReferencePx)`：

1. `HitTestAtWindowMouse(scene)` → `hit.PointInReferenceSpace`（编辑器和引擎都走这条）；
2. 未命中时退回 `m_DragSession.SourceCanvas` + `TryUnmapReferencePx`（原有语义，未改）。

`UpdatePointerFromInput` 用它的返回值既填 `m_PointerSample`，又作为 `MakePointerEvent / ProcessButtonEdges`
的 `referencePx`；`UpdatePointerFrame`（测试与外部逐帧注入的接缝）用**同一个** `referencePx` 参数先
`RecordPointerSample` 再派发事件。因此 **`GetPointerPosition().ReferencePx` 与 `UIPointerEvent::ReferencePx`
是同一个变量、同一次换算**，不存在第二套像素。

固定这件事的断言在 `screen-ui-game-viewport: pointer sample matches UIPointerEvent reference px`：抓住
`OnPointerDown` 收到的 `UIPointerEvent::ReferencePx`，与 `GetPointerPosition().ReferencePx` 做**精确相等**
比较（`==`，非 Approx）。

## 5. DPI 语义：窗口像素 ≠ framebuffer 像素

- `WindowSystem::GetWidth()/GetHeight()` 来自 `GLFWWindowSystem::OnWindowSize`（`GLFWWindowSystem.cpp:549-556`），
  挂在 `glfwSetWindowSizeCallback`（`:568`）上 —— 这是 **window/screen 坐标**（内容区逻辑尺寸），
  不是 framebuffer 尺寸；Windows 显示缩放下两者不等（150% 缩放 → framebuffer 约为 1.5 倍）。
- 全仓**没有任何 framebuffer 尺寸来源**：`grep -rn "FramebufferSize" minEngine/minEngine/src` 为空，
  没有 `glfwGetFramebufferSize`、RHI 也不暴露 swapchain/backbuffer extent。**结论：本仓当前拿不到 framebuffer 像素。**
- 仓内确实一致的地方：`glViewport(0, 0, m_Width, m_Height)`（`GLFWWindowSystem.cpp:189/197`）也用窗口像素，
  GLFW 光标坐标（`InputSystem::GetMousePosition()`，`InputSystem.cpp:424-427`）同样是窗口像素，
  所以「鼠标 → Letterbox → 命中测试」这条链路内部自洽。
- 命名把这件事钉死：`GetViewportSizeWindowPx()` / `TryUnmapViewportPointToReferencePx(..., viewportPointWindowPx, ...)`，
  每个涉及像素的量都带 `WindowPx` 后缀；头注释与本节都写明「不是 framebuffer 像素」。
- 若将来接入 framebuffer 尺寸，正确的做法是在 `WindowSystem` 增加一个明确命名的来源并**另立 API**
  （如 `GetViewportSizeFramebufferPx()`），而不是让现有函数在不同 DPI 下悄悄改变含义。

## 6. 零新增导出

- 新增的全部是 **inline 成员函数** 与 `struct`（编译进消费者）以及 **`.cpp` 内的非 `.def` 私有成员函数**；
  导出集合由冻结的 `minEngine/cmake/minEngine_exports.def` 决定，未改动。
- 数据成员只做**尾部追加**（`m_PointerSample`、`m_bViewportContextFromEngine`），既有成员偏移不变。
- 实测：`NumberOfFunctions = 1283`（改动前后一致）；构建期预算断言 `PASS: NumberOfFunctions=1283 <= 3000,
  ordinal headroom=64252 >= 60000`；`--assert-no-removals-vs` 通过。

## 7. 用例（suite `screen-ui-game-viewport`，全部 `[smoke]`，headless）

suite 注册为 `InSmoke=true, InFull=true`，并在 `TestSuiteRegistry::GetSmokeSuites()` 的 `kSmokeOrder`
末尾追加 id —— 于是 `verify.ps1` → `minEngineTests.exe test smoke` 会真正跑这 3 条（门禁不是文档，是可执行断言）。
实测 `3 cases / 44 assertions` 全绿。

耗时（同进程内边际成本，热态 5 轮中位数）：本套件单跑 99 ms，进程启动地板（`test engine-version`）91 ms
→ **边际 ≈ 8 ms**；同为参考的 `asset-manager` 单跑 212 ms（边际 ≈ 121 ms）。smoke 总时长的
A/B 对照（8 套 vs 7 套）被 `material-ir` 的 GPU 着色器编译噪声淹没（基线自身 3.1–5.5 s 波动），
故以单跑差值作为边际成本依据。

| 用例 | 断言要点 |
| --- | --- |
| `screen-ui-game-viewport: viewport window px maps a canvas reference resolution` | 注入 1280×1000 视口 + 1920×1080 canvas → `Scale = 1280/1920`、`Offset.y = (1000 - 1080*Scale)/2`；`MapPoint` → `TryUnmapViewportPointToReferencePx` 往返回到原 reference px；上黑边的点被拒绝；`nullptr` canvas → `Scale = 0` |
| `screen-ui-game-viewport: missing viewport reports invalid instead of fake coords` | 无 context 且无窗口 → 尺寸 `(0,0)`、样本 `bValid == false` 且向量为 0、`Scale == 0`、`outReferencePx` 不被写；有 context 的帧有效，**无法解算的帧立刻转无效并清掉旧坐标**；注入 0×0 视口后又回到 `Scale = 0` |
| `screen-ui-game-viewport: pointer sample matches UIPointerEvent reference px` | 逐帧注入 (20,20)/(30,25)：样本 reference px 与 `OnPointerDown` 收到的 `UIPointerEvent::ReferencePx` **精确相等**；`ReferenceDeltaPx` = 帧间差 |

反向验证（两次，均已恢复现场）：

1. `RecordPointerSample` 故意写 `referencePx + (1,1)`（假坐标）→ 用例 3 红：
   `ScreenUIGameViewportTest.cpp:250: ERROR: CHECK( ui.GetPointerPosition().ReferencePx.x == doctest::Approx(firstRef.x) ) is NOT correct!` / `values: CHECK( 21 == Approx( 20 ) )`。
2. `GetReferencePxMapping` 故意返回 identity（假映射）→ 用例 1 红：
   `ScreenUIGameViewportTest.cpp:152: ERROR: CHECK( mapping.Scale == doctest::Approx(expectedScale) ) is NOT correct!` / `values: CHECK( 1 == Approx( 0.666667 ) )`。

## 8. 回退方案

改动集中在 5 个文件且相互独立可逆：

1. `UISystem.h`：删 inline 读取面 + `DerivePointerReferencePx/RecordPointerSample/UpdatePointerSample/
   UpdateEngineDefaultViewportContext/ResetPointerViewportContext` 声明 + 两个尾部成员 + friend 声明；
2. `UISystem.cpp`：`Tick` 恢复为「早退 → UpdatePointerFromInput」，`SetPointerViewportContext` 去掉所有权赋值，
   三处 `m_ViewportContext.Reset()` 复原；
3. `ScreenUIHitTypes.h`：删 `ScreenUIPointerViewportSample`；
4. `Tests/**`：删 suite 与注册（`TestSuiteRegistration.cpp` 的 traits/alias/Register 三处 + 两个新文件）；
5. `Runtime/Test/TestSuiteRegistry.cpp`：从 `kSmokeOrder` 删掉 `"screen-ui-game-viewport"`（回退后 smoke 回到 7 套）。

`.def` / baseline `minEngine/cmake/minEngine_export_baseline.json` 未动，回退不涉及导出契约刷新。
回退后编辑器路径与今日行为完全一致（编辑器的注入路径本就没改）。

## 9. 偏差与已知限制

- 指针样本只在**有指针帧**时更新：编辑器编辑态（外部拥有 context、路由关闭）不采样，保持今日开销
  （不留一条「编辑期每帧扫场景」的隐性成本）；独立宿主与 PIE 有样本。
- `ReferenceDeltaPx` 是**帧间差分**，不是「按下点以来的位移」；后者请继续用 `GetDragSession()`（语义不同，未合并）。
- 独立宿主里 ScreenUI 的**事件路由**仍未由引擎自动开启（`OnBeginPIE` 仍只被编辑器调用）——本项只补齐
  「视口尺寸 / 映射 / 指针坐标」读取面与 context 注入，未改路由开关策略（改它会改变 `ShouldBlockWorldPointer`
  与编辑器编辑态的拾取行为，属独立议题）。
- 窗口尺寸与注入 context 之间最多一帧延迟：context 在 `Tick` 刷新，`GetViewportSizeWindowPx()` 优先读 context。
- smoke 成员资格有两个开关，**必须同时满足**：`TestSuiteRegistry::kSmokeOrder` 里的 id **且** metadata
  `InSmoke == true`（`GetSmokeSuites()` 用 `kSmokeOrder` 遍历后仍检查 `InSmoke`）。只置 `InSmoke=true` 而不加进
  `kSmokeOrder` 会被**静默跳过**（本项第一版就踩了这个坑）。README 式表述「smoke = 所有 InSmoke 套件」是错的，
  已在 `TEST_UNIFIED_DESIGN.md §3.3` 过时项清单中提出。
- 未能测到「平台窗口存在」的那条分支（headless 测试没有 WindowSystem 实例）；该分支只有两行
  `WindowSystem::Get().GetWidth()/GetHeight()`，与 `UpdateEngineDefaultViewportContext` 共用同一表达式。
