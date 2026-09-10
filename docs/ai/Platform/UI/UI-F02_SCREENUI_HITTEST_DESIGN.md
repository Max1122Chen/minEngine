# UI-F02 — ScreenUI Hit-test / Pointer

## Meta
- **ID:** UI-F02
- **Type:** Feature
- **Status:** Done（MVP：S0–S2；Button / Focus 后置）
- **Owner:** project maintainer
- **Last updated:** 2026-09-09
- **Branch:** feat/ui
- **Related:**
  - [UI-F01](./UI-F01_UI_SYSTEM_DESIGN.md)（Canvas / Layout / Image；本 Feature 前置）
  - [RND-F16](../../Render/RND-F16_2D_RENDERING_FOUNDATION_DESIGN.md)（ScreenUI Pass / StableOrder）
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
- **Depends on:** UI-F01 Done；InputSystem 鼠标位置/按键可用（实现前核实 OnMouseButton 接线）
- **Blocks:** Button / Focus / 「UI 挡住 3D pick」正式策略（本 Feature 可先做 Query + 薄状态）
- **UE 对照源码根：** D:/Dev/GitRepo/UnrealEngine/Engine/Source/Runtime/（Slate / SlateCore / UMG）

## TL;DR

在 UI-F01 几何之上补 **视口点 → 参考空间命中**，并提供可查询的 Hit Result + 极薄的 Hover/Press 状态。  
引擎侧用 **单一 UISystem** 收口 UI 交互生命周期（对齐 AudioSystem）；HitTester / 指针流程是其内部零件，**不**各自成子系统。  
**不**克隆 Slate FSlateApplication / FHittestGrid / 完整 bubble+FReply；学习其职责拆分，落到 minEngine 体量。

## Scope

### In（建议 MVP，可再切）
- 新增 **UISystem**（Engine 挂载；Init/Shutdown/Tick；目录 Runtime/Function/UI/）
- LetterboxMapping::UnmapPoint（viewport ↔ reference；黑边 miss）
- ScreenUIHitTester：多 Canvas 两阶段选举 + 树内 StableOrder（**非** System）
- Canvas **SortOrder**（跨 Canvas 叠层；与绘制序对齐；缺则本 Feature 补）
- ScreenUIHitResult + **永远可调的 Query API**（经 UISystem）
- UISystem 内 PointerState + **Click 判定**（机制见 §3.3；无 Button 委托）
- **Play / PIE**：指针 Tick +（可选）挡住 3D pick / 玩法

### Out（后置）
- ButtonComponent / 正式 Click 委托体系（Click **状态/边沿**在本 Feature；订阅另开）
- Focus / Tab / 键盘导航
- **Editor 非 Play：不把指针路由进 ScreenUI Hit**（Query 仍可供单测/Agent）
- 透明像素精确命中、World Canvas、多指、Drag-Drop
- 把 UI 点进 Enhanced Input IMC 的完整策略
- Slate 式 bubble path、Preview tunnel、完整 Capture 栈、FReply 语义全集
- HitTester / Router / PointerState **各自**升级为引擎子系统（禁止）

## Reader quick start
1. §3.0 **UISystem 落位与边界**（子系统怎么挂）
2. §3.1–3.5 三层职责（触发 / 算 Hit / 用结果）
3. §4 **UE 类对照**
4. §5 切片与验收

---

## Pre-flight（摘要）

| 项 | 结论 |
|----|------|
| 前置 | UI-F01 几何 **sound**；Mouse→InputSystem 接线需实现前 **核实**（partial） |
| 债风险 | **low–medium**：若把 Router 做成迷你 EventSystem、或拆成多个 System 会膨胀 |
| WIP | CORE-F14 Done；本分支适合开 UI-F02 |
| Philosophy | **机制**（Query + 命中）优先；政策（Button/Focus）后置；**一个** UI 伞系统 |
| 建议 | **Go with scope cut**：S0 Hit+Query → S1 UISystem Tick/状态 → S2 挡 3D pick |

---

## 1) 背景与目标

**Pain：** ScreenUI 能画不能点；3D pick / flycam 不知道指针是否在 UI 上。

**成功（Draft MVP）：**
- 给定视口像素点，能稳定返回最前命中的 WidgetComponent（或未命中）
- Letterbox 黑边未命中；重叠取更高 StableOrder
- 可选：Hover/Press 状态可调试；Editor 点 UI 不误选 Mesh
- Engine 侧仅新增 **一个** UISystem，不散成多个 UI 子系统

---

## 2) 现状（代码事实，2026-09-06）

| 能力 | 状态 |
|------|------|
| Canvas / Layout / ComputedRect / Letterbox **forward** MapPoint | **有**（UI-F01） |
| StableOrder 绘制序 | **有** |
| InputSystem：GetMousePosition / key·mouse 边沿 | **有**（Enhanced Input IMC 另轨） |
| Editor：EditorInputHub + 视口 ImageMin/ImageSize；3D TrySelectAtMousePosition | **有** |
| Runtime/Function/UI/（UITypes + UILayoutPass） | **有**；尚无 UISystem |
| UnmapPoint / Widget hit-test / UI pointer 状态 | **无** |

坐标链（目标）：

```text
窗口鼠标 → 视口局部（− ImageMin / RT 缩放）
        → Letterbox Unmap → Canvas 参考像素
        → ComputedRect 包含测试 → 按 StableOrder 取最前
```

---

## 3) 方案（临时拍板草案）

### 3.0 UISystem 落位与边界（拍板）

**决策：** 引擎挂载 **一个** UISystem，作为 ScreenUI 交互与相关机制的唯一子系统入口（对齐 AudioSystem：一个伞，下面是零件）。

```text
Engine.cpp
 ├── InputSystem          // 设备 / IMC；不懂 Canvas 几何
 ├── SceneManager          // Scene / GO；不懂 Hover/Click
 ├── AudioSystem / Physics / Render …
 └── UISystem              // ← UI 域唯一 System
        ├── Tick：读 Input + 视口上下文 → 更新 PointerState
        ├── HitTest / Query（可转发到无状态 HitTester）
        └── （以后）Focus 等仍进此伞，不新开 System
```

**目录（Runtime/Function/UI/）：**

| 符号 | 层级 | 说明 |
|------|------|------|
| UISystem | Engine 子系统 | Init/Shutdown/Tick；持有 PointerState；对外 Query |
| ScreenUIHitTester | 域内工具（无状态） | 几何 + StableOrder → HitResult；**不是** System |
| ScreenUIPointerState | 数据 | Hovered / Pressed / Click 边沿 |
| 指针更新流程 | UISystem 内部 | 原草案名 Router；实现上可为私有方法，不必单独 System 类 |
| UITypes / UILayoutPass | 已有 | 继续留在同目录 |
| Canvas / Widget / Image | Component | 暂留 Framework/Components；中期可迁 UI/Components（非 F02 必须） |
| Render/ScreenUI | 渲染边界 | Coords / Material / Pass；不负责交互 System |

**与 Scene / Input / Render 的关系：**

| 谁 | 关系 |
|----|------|
| Scene | UI 树 = GO 父子；Hit/Layout **查询**活跃 Scene，不把逻辑写进 SceneManager |
| InputSystem | UISystem **读**鼠标位置/按键；IMC **不**绑 UI |
| Render/ScreenUI | 只负责画与 Letterbox 坐标；Hit 读 Layout 的 ComputedRect，不塞进 Pass |
| Editor 视口 | 提供 ImageMin/Size 等视口上下文给 UISystem；挡 3D pick 在消费侧问 UISystem |

**禁止：** ScreenUIHitTester / PointerRouter / PointerState 各自注册为引擎子系统。

### 3.1 三层职责（均在 UI 域内）

```text
① 触发     UISystem::Tick / 指针流程   读 Input + 视口，在 move/down/up 时查询
② 算 Hit   ScreenUIHitTester           无业务；几何 + 序 → ScreenUIHitResult
③ 用结果   UISystem::HitTestAt（Query，永远）
           + PointerState（薄：Hovered / Pressed / Click）
           + 以后 Button / 挡 3D pick（消费侧问 UISystem）
```

### 3.2 HitTester（机制核心）

输入：viewportPixelPoint + viewportSize + 场景（找 Canvas）。  
输出建议：

```text
ScreenUIHitResult {
  WidgetComponent* widget;     // 最前；可空
  GameObject*      gameObject;
  CanvasComponent* canvas;
  Vector2          pointInReferenceSpace;
  bool             bHit;
}
```

规则：
1. 对每个活跃 ScreenSpace Canvas：**各自** MakeLetterboxMapping → Unmap；黑边 → 本 Canvas miss  
2. 该 Canvas 子树可见、可命中的 WidgetComponent；参考空间 UIRect::Contains  
3. Canvas **内**选举：StableOrder 最大  
4. Canvas **间**决胜：SortOrder 最大（§3.2.1）  
5. 可选 bHitTestVisible（默认 true）；inactive / invisible 跳过  

命中目标先是 **WidgetComponent（几何节点）**；以后 Button 与同 GO Widget 协作。  
HitTester **无状态**，可单测；UISystem 持有状态并调用它。

### 3.2.1 多 Canvas（两阶段选举，拍板）

现状：`CanvasComponent` **尚无** SortOrder；绘制队列目前主要按 Widget `StableOrder` 混排。多 Canvas 且参考分辨率不同时，**必须按 Canvas 各自 Letterbox Unmap**，不能全局解一次。

与先前讨论一致的算法：

```text
candidates = []
for each active ScreenSpace Canvas（按 Canvas.SortOrder 升序遍历即可）:
    mapping = MakeLetterbox(canvas.ref, viewport)
    refPoint = Unmap(viewportPoint)     // 黑边 → 本 Canvas 无候选
    if miss: continue
    elect = 该 Canvas 子树内 Contains 且可命中的 Widget 中 StableOrder 最大者
    if elect: candidates.push({ canvas, elect, Canvas.SortOrder, elect.StableOrder })

return candidates 中 Canvas.SortOrder 最大者
      （同 SortOrder 时再比 StableOrder；再平则稳定次序，如 GO 创建序）
```

| 层 | 比较键 | 含义 |
|----|--------|------|
| Canvas 间 | `Canvas.SortOrder`（高者在上） | 对齐 Unity `Canvas.sortOrder` / UE 上屏 Widget 叠层直觉 |
| Canvas 内 | `Widget.StableOrder`（高者在上） | 与现有 ScreenUI 绘制序一致 |

**为什么两阶段而不是一次扁平扫：**  
每个 Canvas 有独立参考空间与 Letterbox；先「树内选举」再「Canvas 间决胜」边界清晰，也避免把不同 ref 空间的 Rect 误比。

本 Feature **In：** 为 `CanvasComponent` 增加 `SortOrder`（默认 0）；绘制侧后续应同键排序（若 S0 只改 Hit，绘制对齐可紧随或记债）。

### 3.3 Click 机制 + 指针状态（政策很薄，属 UISystem）

Click **机制本身很简单**，本 Feature **实现**（状态/边沿），**不**做 Button 外观与委托列表。

```text
Down：若 Hit.widget → Pressed = that Widget（Capture 萌芽）
Move：更新 Hovered；不清除 Pressed
Up：
  hitNow = HitTest(...)
  if Pressed != null && hitNow.widget == Pressed → 产生 Click（边沿/计数/日志）
  清空 Pressed
```

| 事件 | 行为 |
|------|------|
| Move | 更新 Hovered |
| Down | 若命中 → Pressed = Hit.widget |
| Up | 同 Widget → Click；清空 Pressed |

Capture MVP：Up 时也可约定「只要 Pressed 非空且指针未改键」仍把 Click 判给 Pressed（移出矩形也算）——与常见按钮一致；上表用「Up 仍在同一 Widget」更严。**拍板用宽松 Capture 版**（Up 交给 Pressed，不要求仍 Contains）。  
**不**绑 IMC。不必暴露公开 Router 类型。

### 3.4 Query 永远可用

```text
UISystem::HitTestAt(...) → ScreenUIHitResult
// 内部走 HitTester；单测可不经 Tick
```

符合 Agent-Friendly / 机制优于政策：不必经 Button 也能查。

### 3.5 Editor / Play 策略（拍板）

| 模式 | ScreenUI 指针 Hit / Click | 说明 |
|------|---------------------------|------|
| **Play / PIE** | **做** | UISystem Tick；可挡 3D pick / 玩法 |
| **Editor 非 Play** | **不做路由** | 视口点击仍走编辑拾取/操作；不驱动 Hover/Click |
| **Query API** | 始终可调 | 单测 / Agent / 调试，不依赖 Editor 路由 |

**别的引擎（对照，非抄）：**

| 引擎 | 非 Play 编辑视口 | Play / PIE |
|------|------------------|------------|
| **Unity** | Scene 视图点的是编辑器操作；Game 视图在 Edit Mode 下 EventSystem 通常不跑完整玩法交互；**Play** 后 UGUI 才正常收指针 | EventSystem + Raycaster |
| **UE** | 编辑器输入进 **Editor Slate**；关卡视口点 Actor 是编辑器拾取。关卡里的 UMG 一般不在非 PIE 下当「游戏 UI」收点击 | PIE / 游戏视口走游戏侧输入 + Slate/UMG |
| **minEngine** | Editor 壳是 ImGui；非 Play 视口优先 3D/编辑逻辑 | 与上表对齐：**仅 Play/PIE 路由 ScreenUI** |

结论：你的判断成立——**Editor 非 Play 不做 ScreenUI 命中路由**；避免点 UI 与选物体抢输入，也减少两套坐标策略纠缠。

### 3.6 刻意不做

- 每 Widget 挂 Raycaster  
- 完整 EventSystem + 十余种 Handler  
- 把 Click 做成唯一入口（必须保留 Query）  
- 多个 UI 相关 System 并列挂 Engine  
- Editor 非 Play 驱动 ScreenUI Hover/Click  

---

## 4) 与 Unreal Engine 的对照（学习，不抄全量）

UE 的游戏 UI（UMG）底层是 **Slate**：输入进 FSlateApplication，绘制时填 **FHittestGrid**，命中得到 **FWidgetPath**，再 **Route** 到 SWidget 虚函数，控件用 **FReply** 告诉系统是否 Handled / Capture / Focus。

minEngine 没有独立 Slate 树；ScreenUI 挂在 **GO + Component** 上。对照的是 **职责**，不是类一一复制。  
对标「一个应用层入口」时，用 **UISystem**（而非多个小 System）近似 FSlateApplication 的收口角色——职责窄很多。

### 4.1 端到端数据流（UE）

```text
平台鼠标/触控
  → FSlateApplication::ProcessMouseButtonDownEvent / ProcessMouseMoveEvent …
  → LocateWindowUnderMouse（窗口级）
  → FHittestGrid::GetBubblePath（桌面坐标 → 控件路径）
  → RoutePointerDownEvent / RoutePointerMoveEvent …
  → SWidget::OnMouseButtonDown / OnMouseEnter / …
  → 返回 FReply（Handled、CaptureMouse、SetUserFocus…）
```

绘制与命中的耦合（重要）：SWidget::OnPaint 经 **FPaintArgs** 拿到当前 **FHittestGrid**，把可命中的控件登记进网格——**命中空间与布局/绘制同源**，而不是另建一套「Raycaster 组件」。

源码锚点（本地 UE）：
- Slate/Public/Framework/Application/SlateApplication.h — Process* / LocateWindowUnderMouse / RoutePointer*
- SlateCore/Public/Input/HittestGrid.h — FHittestGrid::GetBubblePath / AddWidget
- SlateCore/Public/Types/PaintArgs.h — FPaintArgs 持有 FHittestGrid
- SlateCore/Public/Layout/WidgetPath.h — FWidgetPath
- SlateCore/Public/Input/Reply.h — FReply
- SlateCore/Public/Layout/Visibility.h — EVisibility（Visible / HitTestInvisible / …）
- SlateCore/Public/Widgets/SWidget.h — OnMouseButtonDown / OnMouseEnter / …
- UMG/Public/Components/Button.h — UButton + OnClicked（作者层）

### 4.2 类职责表（UE → 本草案）

| 职责 | UE | minEngine UI-F02 草案 | 学什么 / 不学什么 |
|------|----|------------------------|-------------------|
| **① 谁触发、谁收平台输入** | FSlateApplication::ProcessMouse* | **UISystem** Tick/指针流程读 InputSystem + 视口 | 学「一个应用层入口」；不抄窗口栈；不拆多 System |
| **窗口/视口下找谁** | LocateWindowUnderMouse | 视口 ImageMin/ImageSize（已有 Editor/3D pick 习惯） | 学坐标先落到正确表面；不抄多 SWindow |
| **② 谁算 Hit** | FHittestGrid::GetBubblePath | ScreenUIHitTester（UI 域工具，非 System） | 学「空间索引 + 序」；MVP 线性扫即可 |
| **绘制时登记命中** | FPaintArgs 持有 FHittestGrid | Layout 已写 ComputedRect；Hit 读同一几何 | 学「命中与布局同源」 |
| **几何** | FGeometry / FArrangedWidget | UIRect + Letterbox Map/Unmap | 学局部/绝对分离 |
| **命中路径** | FWidgetPath | MVP 只返回最前 Widget | 学 bubble 需要路径；暂不 bubble |
| **③ 事件怎么派发** | RoutePointer* | UISystem 更新 PointerState | 学先 path 再 route；不抄 Preview |
| **控件怎么响应** | SWidget::OnMouse* | 后置；MVP 无虚事件表 | 学接口形状 |
| **Handled / Capture / Focus** | FReply | Pressed ≈ 极简 Capture；Focus Out（仍进 UISystem 伞） | 学 Reply 是政策回传 |
| **可见 vs 可点** | EVisibility | bHitTestVisible + 可见/active | 学能画 ≠ 能点 |
| **UMG 作者层** | UWidget → SWidget | WidgetComponent / 以后 Image/Button | 学作者类型 ≠ 命中实现 |
| **按钮语义** | UButton + OnClicked | UI-F03+；本 Feature 最多 Click 状态 | Click 不在 HitTester |
| **玩法输入** | Enhanced Input 与 Slate 分轨 | IMC 不绑 UI；UISystem 先吃指针 | 学分轨 |

### 4.3 三层问题 ↔ UE

| 你的问题 | UE 答案 | 我们的答案 |
|----------|---------|------------|
| 谁触发查询？ | FSlateApplication::Process* | **UISystem**（或调用方直接 Query） |
| 谁算 Hit？ | FHittestGrid::GetBubblePath | ScreenUIHitTester（读 Layout；属 UI 域） |
| 结果怎么用？ | Route* → SWidget → FReply | Query + PointerState → 以后 Button；挡 3D pick 问 UISystem |

### 4.4 刻意简化相对 UE 的理由

1. **无独立 UI 树运行时：** GO 父子 + Component 已是树；再引入 SWidget 双树成本过高。  
2. **控件数量级小：** 网格加速可后置；正确性先靠 StableOrder + Contains。  
3. **无 bubble 需求：** MVP 只要「点到谁」；路径与 Preview 留给真交互控件。  
4. **Philosophy：** 机制（Query/Hit）进 UI 表面；政策（Button/Focus）保持薄；**一个** UISystem 避免子系统爆炸。

### 4.5 仍值得从 UE 借的「概念清单」（实现时核对）

- [x] 能画与能点分离（HitTestInvisible 思路） — `WidgetComponent::m_bHitTestVisible`  
- [x] Capture：按下后即使移出仍把 Up 交给 Pressed（宽松版）  
- [x] Handled：Play/PIE 下 UI 吃掉后，3D pick / 玩法不再处理（S2） — `ShouldBlockWorldPointer` + pick 守卫；Play 本就停 Editor pick  
- [x] 命中序与绘制序一致（Canvas.SortOrder + Widget.StableOrder；Draw 队列同键）  
- [ ] 自定义命中（ICustomHitTestPath）— **远期**，透明图/圆形按钮用  

---

## 5) 建议切片

| 切片 | 内容 | 验收 | 状态 |
|------|------|------|------|
| **S0** | Unmap + HitTester（含多 Canvas 两阶段）+ Canvas.SortOrder + Query + 单测 | 单/多 Canvas、黑边、树内序、Canvas 间序正确 | **Done**（`screen-ui-hit`） |
| **S1** | UISystem 挂 Engine；Play/PIE Tick；PointerState + **Click** | Hover/Press/Click 可观察；非 Play 不路由 | **Done**（PIE OnBegin/End + Viewport 注入） |
| **S2** | 消费侧 `ShouldBlockWorldPointer`；`TrySelectAtMousePosition` 守卫 | 路由开启且 Hover/Pressed 时不 3D pick | **Done** |
| **后置** | Button 委托、Focus、IMC Consume | — | — |

### 无 Button 时如何目视验收（S1 验收辅助）

Play/PIE 下 Viewport 左上角 **ScreenUI debug HUD**：
- `Hover` / `Pressed` / `Click` 文本
- Hover 控件绿色描边（按下时橙色）
- `Ctx: ok` 表示 ImageMin/Size 已注入

场景准备：Canvas + 带 `WidgetComponent`+`ImageComponent` 的子 GO → Play → 鼠标划过 Image。

实现前：**核实** MouseButton → InputSystem 是否可靠。

---

## 6) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. **单一 UISystem** + 域内 HitTester/PointerState | 不散、可测、对齐 Audio | 需在 Engine 多挂一个 System | **选用** |
| B. 无 UISystem；仅工具 + Editor 里手搓 Tick | 更薄 | 多处重复挂接；缺统一入口 | 不用（已拍板要伞） |
| C. Router/HitTester 各成 System | 名义清晰 | 子系统爆炸 | **禁止** |
| D. 克隆 EventSystem + Raycaster | 接近 Unity 教材 | 与 Canvas/Layout 重复 | 不用 |
| E. 直接上 Slate 式 Path+Reply+Grid | 与 UE 同构 | 体量过大、双树 | 不用 |
| F. 多 Canvas 一次扁平扫（忽略每 Canvas Letterbox） | 实现短 | 多 ref 分辨率错误 | **禁止**；用两阶段 |

---

## 7) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| Mouse 未进 InputSystem | S1 假阴性 | S0 前后核实接线 |
| UISystem 膨胀成迷你 Framework | 难维护 | Scope Out 写死；Button/Focus 另切仍进同一伞 |
| 多个 UI System 并列 | 散、重复 Tick | §3.0 禁止；Code review 卡 |
| Canvas 无 SortOrder / 绘制与 Hit 序不一致 | 点到的与看到的不符 | F02 补 SortOrder；绘制同键排序或记债 |
| 无 bubble 导致以后改 API | 破坏性 | HitResult 先留扩展位；路径后置 |

---

## 8) 验收标准（Draft → Done 时勾）

- [x] Design 升 Planned/In Progress 后实现 S0–S1（S2 按需）  
- [x] Engine 仅新增 **一个** UISystem；无并列 UI 子系统  
- [x] 单测：Unmap、树内序、**多 Canvas 决胜**、黑边（`screen-ui-hit` / `screen-ui-coords`）  
- [x] Play/PIE：Hover/Press/Click 状态机 + 非 Play 不路由（`OnBeginPIE`/`OnEndPIE`）  
- [x] S2：`ShouldBlockWorldPointer` + pick 守卫（Play 下 Editor pick 本已关闭；API 供消费侧）  

---

## 9) 岔路口决议（2026-09-07）

| # | 问题 | 决议 |
|---|------|------|
| 1 | Click？ | **做机制**：Pressed+Up→Click 边沿/状态；Button 委托后置。Capture 用宽松版（Up 交给 Pressed） |
| 2 | Editor 非 Play 命中？ | **不做路由**（对齐 Unity/UE「编辑视口 ≠ 游戏 UI 输入」）；Play/PIE 做；Query 仍可用 |
| 3 | 命中挂谁？ | **WidgetComponent** |
| — | 多 Canvas？ | **两阶段**：每 Canvas 树内选举 → 再比 `Canvas.SortOrder`（本 Feature 补该字段） |

**此前已拍板：** 单一 UISystem；零件不成 System；UI 树 = Scene GO。

---

## 10) Status note

| 字段 | 内容 |
|------|------|
| Status | **Done**（MVP） |
| Blocked by | — |
| Next | 用户 PIE 目视验收（debug HUD）；后置 Button / Focus |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-06 | Draft：三层方案 + UE Slate/UMG 类职责对照（源码锚点） |
| 2026-09-06 | 拍板 **单一 UISystem**：§3.0 落位/边界；修订 Scope/切片/备选/验收；禁止零件各自成 System |
| 2026-09-07 | 岔路决议：Click 机制入 MVP；Editor 非 Play 不路由；命中挂 Widget；**多 Canvas 两阶段 + SortOrder** |
| 2026-09-07 | **Planned**：用户认可；开始骨架（System/类型/接口；算法空实现） |
| 2026-09-09 | **In Progress**：S0 Hit + S1 Pointer/Click/PIE；`screen-ui-hit` 绿；Draw 按 Canvas.SortOrder |
| 2026-09-09 | **Done**：Viewport 注入 + PIE debug HUD；S2 `ShouldBlockWorldPointer`；无 Button 目视路径 |