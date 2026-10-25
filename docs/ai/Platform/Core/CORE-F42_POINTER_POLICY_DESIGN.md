# CORE-F42 指针 / 光标策略服务 — Design Spec

## Meta
- **ID:** `CORE-F42`
- **Type:** Feature
- **Complexity:** `L2`
- **Status:** `Done`（S01–S06 落地；引擎不自动 Confined，容器拥有者 RAII `Pointer::ShowUi` / `ConfineUiTo`）
- **Owner:** project maintainer
- **Last updated:** 2026-10-25（实现收口；S06 O7 绿 + 消费方迁移说明）
- **Related:**
  - 任务书原文：`minCraft-ui/docs/ai/INFRA/ENGINE_SESSION_PROMPT_UI_UV_AND_CURSOR_POLICY.md`（工作项 B，§4）
  - [ED-F23](../../Editor/ED-F23_VIEWPORT_POINTER_CAPTURE_DESIGN.md)（**现有** per-viewport 指针策略，Done；本 Feature 把它收进引擎级拥有者，并**改绑释放键**）
  - [CORE-F29](./CORE-F29_PLAY_GATED_GAMEPLAY_INPUT_DESIGN.md)（玩法 IMC 门 — **不由本 Feature 接管**）
  - [BUG-CORE-004](./BUG-CORE-004_INPUT_GATE_DEVICE_CLEANUP_AND_PIE_POINTER_FIX_DESIGN.md)（PIE 指针历史修复）
  - [UI-F08](../UI/UI-F08_SCREENUI_WIDGET_POINTER_INTERACTION_DESIGN.md)（`UISystem` 指针状态 / `ShouldBlockWorldPointer`）
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md)（`CORE-F42` 已登记）
- **Depends on:** `WindowSystem`（`SetCursorVisible` / `SetCursorClipRect` / `ConsumeRelativeMouseDelta`；**新增 focus 回调**）、`InputSystem::GetMousePosition`、`UISystem::GetPointerViewportContext`
- **Blocks:** 无
- **Does not block:** 工作项 A（UI 图像翻转）、工作项 C（材质编译缓存）

## TL;DR

窗口层只有"显示 / 隐藏+锁死"两档（`WindowSystem.h:40` → `GLFW_CURSOR_NORMAL/DISABLED`，`GLFWWindowSystem.cpp:240-243`），**没有拥有者、没有仲裁、没有游戏面 API**；编辑器三处各自直调，谁最后跑谁生效；`GLFW_CURSOR_DISABLED` 把"隐藏"和"锁死"绑死，缺"隐藏但自由"档；游戏侧读不到指针的实时参考像素坐标。

**更严重的是 OS 层泄漏**：`::ClipCursor` 是**系统级（全桌面）**约束，而本仓只有两处调用（`GLFWWindowSystem.cpp:253` 清、`:267` 设），**没有任何 focus / activate 事件驱动它撤销**（`glfwSetWindowFocusCallback` / `WM_ACTIVATE` / `WM_KILLFOCUS` 在 `minEngine/src` 内零命中；`SetupWindowEventCallbacks` 只注册了 5 个回调，`GLFWWindowSystem.cpp:408-415`）。于是 PIE 捕获后 Alt+Tab 到别的进程，**整个桌面的鼠标都被夹在视口矩形里**。

本 Feature 引入**唯一拥有者 `PointerPolicyService`**（tier 优先级栈 + 引用计数 + RAII 句柄），并把 **"约束只在窗口 active 期间存在"** 作为 P0 不变量（I6a/I6b/I6c）。**不接管输入路由**（那是 `CORE-F29` 的门与 `UISystem` 的 hover 门）。

## Scope

### In
- **focus 作用域约束（P0）**：窗口失焦即刻撤销 clip / raw capture / 隐藏 / 捕获；回焦按栈重放。
- 引擎级单一指针策略拥有者（模式集合 / 优先级栈 / 引用计数 / RAII 句柄）。
- 模式到窗口层原语的映射（可见性 × 约束 × 移动源），含"隐藏但自由"档。
- 游戏面公开 API（非作者化）：请求 show/hide、free/confined/captured、查询有效模式。
- 实时指针位置契约（窗口像素 / 视口像素 / 参考像素 / 相对位移 / 有效性）。
- PIE 生命周期状态机：进 Play 默认 Captured、**Shift+F1 释放**、Esc 停止、失焦挂起、Play 结束与进程关闭归零、崩溃不残留锚定。
- 与 `UISystem::ShouldBlockWorldPointer()` 的职责分工（容器打开 → UI tier 请求"可见 + 约束在视口"）。
- 可观察 oracle 与最小自动化验证路径。

### Out（非目标，见 §Non-Goals）
- 输入路由 / IMC 门 / 焦点系统重建 / 通用键位重映射框架。
- 多窗口、多视口并发 pointer lock；软件光标绘制。
- 作者化面（不进反射、不进 `medata`、不给蓝图/资产配"默认鼠标模式"）。
- 自研 raw input 管线、自研 ClipCursor 替代品（沿用现有 Win32 路径）。

## Reader quick start
1. 先读 **Problem**（尤其 **P7 系统级约束泄漏**，这是本轮唯一 P0 级缺陷）与 **复核与任务书差异**。
2. 再读 **模式与三方对照**、**Proposed Architecture**、**State & Invariants**（I6a/I6b/I6c 与 PIE 状态机是核心）。
3. 契约看 **API / Interface Semantics**；验收看 **Verification Strategy**；实现顺序看 **Slices**（S01 = focus 作用域约束）。

## Pre-flight

| 项 | 结论 |
|----|------|
| **Complexity** | **L2**（跨 Platform / Editor / UI 三处调用面；状态机 + 生命周期；已过审批门） |
| **Prerequisites** | `SetCursorClipRect` + Raw Input（`GLFWWindowSystem.cpp:245-352`）、`InputSystem::GetMousePosition`（`InputSystem.h:82`）、`ScreenUI` 参考像素映射（`UISystem.cpp:155-182`）、GLFW 回调注册点（`GLFWWindowSystem.cpp:408-415`）**sound** |
| **Tech-debt risk** | **medium**（收口一个跨层服务；但 S01 只做 focus 作用域，风险极低） |
| **WIP** | 与工作项 A / C 无文件重叠；本 Design 只落文档 |
| **Philosophy** | 指针**物理状态**是引擎机制 → 进 Core；"什么场景该锁鼠标"是产品决策 → 由请求方表达；**"约束不许泄漏到进程外"是引擎责任，不是请求方的礼貌** |
| **Recommendation** | **S01 先落地 focus 作用域约束**（UX 伤害最大、blast radius 最小、不依赖新 API 面），再 S02 服务骨架 → S03 映射与迁移 → S04 状态机 → S05 游戏面 → S06 UI 集成 |

---

## Problem

### P1 — 没有拥有者：三处直调 + 每帧重放，谁最后跑谁赢

| 调用点 | 证据 | 做的事 |
|--------|------|--------|
| 编辑器启动 | `Editor/src/Editor.cpp:412` | `SetCursorVisible(true)`（抵消窗口创建时的 `GLFW_CURSOR_DISABLED`） |
| PIE 停止 | `Editor/src/PlayMode/PlayInEditorSession.cpp:268-274` | 清 clip + `SetCursorVisible(true)` |
| 视口策略 | `Editor/src/SubEditor/Scene/SceneEditingViewportClient.cpp:392-429` | 每帧 `ApplyPointerPolicy(mode)`：hide+clip / 清 clip+show |

三者互不知情，且视口的 apply 是**每帧无条件重放**（`SceneEditingViewportClient.cpp:173-183` 在 `EndFrame` 末尾调用）。PIE `Stop()`（`PlayInEditorSession.cpp:268-274`）与视口的每帧重放**先后取决于调用顺序**：同帧内如果视口先跑、Stop 后跑，结果与反序相反。没有任何一处能回答"现在有效模式是什么"。

### P2 — 只有两档：`DISABLED` 把"隐藏"和"锁死"绑死

`WindowSystem.h:40` 是 `SetCursorVisible(bool)`，实现把 `false` 映射到 `GLFW_CURSOR_DISABLED`（`GLFWWindowSystem.cpp:240-243`）。`GLFW_CURSOR_DISABLED` 同时隐藏光标并提供虚拟无限位移（`Third-Party/glfw/include/GLFW/glfw3.h:1158-1161` 定义了 `NORMAL / HIDDEN / DISABLED / CAPTURED` 四档，但仓库只用了两档）。因此**"隐藏但自由移动"（隐藏 + 绝对坐标）与"隐藏且约束在视口内"两个档位无法表达**，而这正是消费方光标栈跟手、以及容器打开后要求"可见 + 约束"所需的两档。

### P3 — 缺游戏面 API

游戏代码没有任何办法说"捕获鼠标 / 显示鼠标 / 限制在视口内 / 现在是什么模式"。`ED-F23` 的 Non-Goals 明确写着 `Game-side Pointer Lock API`（`docs/ai/Editor/ED-F23_VIEWPORT_POINTER_CAPTURE_DESIGN.md:37`）—— 这是当时有意的裁剪，现在成为缺口：独立 Game 进程（不经编辑器）**根本没有指针策略的入口**。

### P4 — 结构性缺口：窗口创建即锚定，独立 Game 无法自救

`GLFWWindowSystem::Initialize` 在建窗后直接 `glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED)`（`GLFWWindowSystem.cpp:132`、`:157`）。当前唯一补救是编辑器侧的 `Editor.cpp:412`。于是在**非编辑器进程**（Game / 其它 Host）里，进程一起来光标就是"隐藏 + 锁死"，而代码里没有任何由引擎自己拥有的复位点。

### P5 — 游戏侧读不到实时指针坐标

- `UISystem::GetPointerState()` 返回 `ScreenUIPointerState`（`UISystem.h:38`），其字段只有 hover/pressed/click/modifiers，**不含位置**（`ScreenUIHitTypes.h:61-92`）。
- 参考像素坐标只以三种形式存在：`UIPointerEvent::ReferencePx`（`ScreenUIPointerTypes.h:37`，只在事件里）、拖拽会话 `PressReferencePx / CurrentReferencePx`（`ScreenUIDragTypes.h:30-31`）、以及私有映射 `UISystem::TryUnmapReferencePx`（`UISystem.h:123-128`）。
- 窗口像素坐标**已有**：`InputSystem::GetMousePosition()`（`InputSystem.h:82`），`UISystem.cpp:175-179` 就是用它减 `ImageMin` 得到视口内坐标。缺的是"**把一次采样同时发布成四种坐标表示 + 有效性**"的公开读口。
- `InputKeys::Mouse2D` 的值在 clip preserve 打开时是**虚拟绝对量**而非物理量（`InputSystem.h:84-91`），Captured 下更只有相对位移 —— 调用方无法从现有 API 判断"这个坐标现在是否可信"。

### P6 — 失焦 / 崩溃 / 多请求叠加都没有定义（策略层）

- `WindowSystem` 的回调表只有 reset/key/mouse/cursorPos/scroll/windowSize（`WindowSystem.h:57-70`），**没有 focus 回调**：窗口失焦后引擎无从得知，Captured 态照旧。
- `ApplyPointerPolicy` 没有析构/关闭兜底；`GLFWWindowSystem::Shutdown` 里清 clip（`GLFWWindowSystem.cpp:177-180`）是唯一兜底，但它不知道"策略栈还挂着谁"。
- 多请求叠加无定义：例如"编辑导航（EditNavigate）+ PIE 转换（`SyncPointerModeFromPlayTransition`，`SceneEditingViewportClient.cpp:337-357`）"同帧发生，只能靠写入顺序碰巧正确。

### P7 — 系统级约束泄漏（P0 · 主人实测体验，比任务书原文更严重）

**现象（主人原话）**：「为了实现 PIE 时把鼠标指针约束在 viewport 内，甚至会导致聚焦在其他进程窗口时这个约束仍然存在，导致使用体验极其糟糕。」

**证据链（全部可在本仓核对）**：

| # | 事实 | 证据 |
|---|------|------|
| 1 | `::ClipCursor` 全仓只有两处调用：一处清（`nullptr`）、一处设 | `GLFWWindowSystem.cpp:253`、`:267`（对 `minEngine/minEngine/src` 全量 grep `ClipCursor` 仅此两处实现调用） |
| 2 | 清理只发生在**显式**调用 `SetCursorClipRect({bEnabled=false})` 或 `Shutdown` | `GLFWWindowSystem.cpp:249-259`（`wantClip==false` 分支）、`:177-180`（Shutdown 清） |
| 2b | 连这次清理都被**缓存自证**把着门：`ClipCursor(nullptr)` 只在 `m_bCursorClipActive == true` 时才真的调用 | `GLFWWindowSystem.cpp:251-255`（`if (m_bCursorClipActive) { ::ClipCursor(nullptr); ... }`）。一旦缓存标志与 OS 真值不同步，「撤销」会被静默跳过 —— 这就是本设计要求施加态**用 `::GetClipCursor` 回读**、不用缓存自证的原因（要求 5） |
| 3 | 引擎侧**没有任何** focus / activate 事件源 | `glfwSetWindowFocusCallback` / `WM_ACTIVATE` / `WM_KILLFOCUS` / `AppFocusLost` 在 `minEngine/minEngine/src` 内**零命中** |
| 4 | 注册点缺失是具体的：只注册了 5 个回调，没有 focus | `GLFWWindowSystem.cpp:408-415`（key / mouseButton / cursorPos / scroll / windowSize） |
| 5 | 自定义 WndProc 只特判 `WM_INPUT`，其余原样转发 —— 本可挂 `WM_ACTIVATE` 的位置是空的 | `GLFWWindowSystem.cpp:33-66`（`:35` 判 `WM_INPUT`，`:61-65` 转发） |
| 6 | 唯一会走到清理的两条编辑器路径都**与焦点无关** | `SceneEditingViewportClient.cpp:419-426`（模式不是 hide+clip 时）、`PlayInEditorSession.cpp:268-274`（Stop 时） |
| 7 | Win32 语义决定这是系统级泄漏：`ClipCursor` 的约束是**共享的桌面资源**，必须由调用方显式 `ClipCursor(nullptr)` 释放，**不会因为本窗口失焦自动解除**（Win32 `ClipCursor` 文档："The cursor is a shared resource… must release the cursor … before relinquishing control to another application"） | 平台语义（非本仓代码），与 #1/#2 结合即泄漏 |

**复现路径**：进 Play → 视口策略进入 hide+clip（`SceneEditingViewportClient.cpp:401-418`）→ **Alt+Tab 切到别的进程** → 本窗口不再接收任何输入，但 `ClipCursor` 仍生效 → 在整个桌面上移动鼠标被夹在旧视口矩形内，其他应用无法正常使用鼠标。**只有本进程退出或再次被切回并走 Stop 才恢复。**

**为什么"策略层失焦挂起"修不了它**：策略层解析出 `Free` 只意味着"我们打算不再约束"；OS 的 clip 是**已施加的副作用**，必须有人**显式撤销**。当前既没有人收到失焦事件（#3/#4），也没有"施加态 vs 意图态"的区分。这是本 Feature 把 **S01 前置**的唯一理由。

## 复核与任务书差异

任务书 §4 的现状事实**方向正确但有过时/遗漏**，按 file:line 修正如下（以本条为准）：

| 任务书说法 | 复核结果 |
|-----------|----------|
| "窗口层只有 `SetCursorVisible(bool)`" | **不准**。窗口层已有 `SetCursorClipRect(WindowCursorClipRect)`（`WindowSystem.h:12-20`、`:41-42`；实现 `GLFWWindowSystem.cpp:245-277`，Win32 `ClipCursor`）与 `ConsumeRelativeMouseDelta()`（`WindowSystem.h:47`；实现 `GLFWWindowSystem.cpp:279-290`），并自带 Raw Input 注册（`GLFWWindowSystem.cpp:292-352`）。**缺口不是"没有约束能力"，而是"没有统一策略与拥有者"** —— 且该约束能力**会泄漏**（P7） |
| "编辑器三处各自直调，无拥有者无仲裁" | **半对**。编辑器已有 per-viewport 策略枚举 `ViewportPointerMode{Off, EditNavigate, PieCaptured, PieReleased}`（`SceneEditingViewportClient.h:110-116`）与 `ED-F23`（Done）。准确说法是：**有编辑器级（更准确：单个视口级）策略，缺引擎级拥有者与仲裁** |
| "实时位置只在 `UIPointerEvent::ReferencePx` 与拖拽会话里" | **不准**。`InputSystem::GetMousePosition()/GetMouse2DAbsolute()` 提供窗口像素与虚拟绝对量（`InputSystem.h:82-91`）。缺的是**参考像素空间的、可查询的、带有效性的**实时采样（P5） |
| "Esc 释放/暂停 Play" | **不准**。现状 Esc = **Stop Play**（`SceneEditingViewportClient.cpp:368-372`），释放（保留 Play）是 **Left Alt**（`:374-382`）；`PlayState` 只有 `Editing / Playing`，**没有 Paused**（`PlayMode/PlayInEditorSession.h:23`、`:43`）。**主人裁决（R2）**：Esc 保持 Stop 不变；释放键改绑 **Shift+F1**（见 §键位变更） |
| （任务书未提） | **新增事实**：建窗即 `GLFW_CURSOR_DISABLED`（`GLFWWindowSystem.cpp:132/157`），独立 Game 进程无复位点（P4） |
| （任务书未提，主人 R2 追加） | **新增 P0 缺陷**：clip 泄漏到进程外（P7）。复核**成立**：无 focus 事件源（`GLFWWindowSystem.cpp:408-415` 只注册 5 个回调）+ `ClipCursor` 全仓仅 2 处（`:253/:267`）+ Win32 系统级语义。**当前实现确实会把光标夹在桌面范围** |

## Requirements

覆盖任务书 §4 七条 + 本轮 P0 追加要求（逐条落点见文末自检）。

### Must（S01 — focus 作用域约束，P0，**最早落地**）

1. **四类约束一律 focus-scoped。** clip（ClipCursor）、raw input capture、cursor hidden、captured（GLFW_CURSOR_DISABLED）都只在 **本窗口 active** 期间成立。
2. **失焦即刻撤销。** 收到失焦事件当帧（至迟下一帧 `BeginFrame` 前）必须撤销全部四类约束：`ClipCursor(nullptr)`、注销 Raw Input、光标恢复可见、退出 `DISABLED`。**不得**等 Stop、不得等下一帧的"策略解析"顺手处理。
3. **事件来源必须来自平台层。** `WindowSystem` 增加 focus 回调（GLFW `glfwSetWindowFocusCallback` 注册进 `SetupWindowEventCallbacks`，该函数 `GLFWWindowSystem.cpp:408-415` 现只有 5 个；Win32 侧可选 `WM_ACTIVATE` / `WM_KILLFOCUS` 兜底，挂点已在自定义 WndProc，`GLFWWindowSystem.cpp:33-66`）。**明令禁止**用 `ImGui::GetIO().AppFocusLost` 作为判据（UI 框架不判平台状态；且非编辑器进程里根本没有 ImGui）。
4. **clip 矩形必须与客户区求交。** Win32 `ClipCursor` 用屏幕坐标；请求方给的是视口矩形。施加前取 `rect ∩ client-screen-rect`，交集为空 → **不施加**并 warn（防止把指针夹到窗口外 / 别的显示器上）。
5. **OS 施加态可读（与意图态分离）。** 提供 `GetAppliedConstraint()`（真实施加态）与 `GetResolvedState()`（策略意图态）两个**独立**读口；Win32 下施加态以 `::GetClipCursor` 回读核对，不用缓存值自证。
6. **Watchdog。** 每帧核对"窗口未 active 却仍 `bClipActive` / raw 仍注册" → 立刻撤销 + 一条 warn（失焦事件万一漏了也不会长期泄漏）。

### Must（S02 — 服务骨架与仲裁）

7. **唯一拥有者。** 新增 `PointerPolicyService`，是**唯一**对 `WindowSystem` 施加光标可见性/约束/移动源指令的单位。编辑器与游戏**不得**再直接调 `WindowSystem::SetCursorVisible / SetCursorClipRect`（迁移清单见 **Migration**）。启动期的 `GLFW_CURSOR_DISABLED` 由服务在首帧解析时纠正（P4）。
8. **配置即栈，不是最后调用者。** 请求 = `(Tier, Mode, ClipSpec) × RefCount`。有效模式 = **最高 tier 中最近一次存活请求**；同 tier 内 **LIFO**；同 `(Tier, Mode, ClipSpec)` 的重复请求**合并计数**，任一持有者释放只减计，不减档。
9. **RAII 句柄。** 请求返回 move-only 句柄（`PointerModeRequest`），析构即 `Release`。**禁止**公开裸 `SetMode(bool)` 风格 API —— 这是"最后调用者赢"的根。
10. **帧边界施加一次。** 解析与施加在帧边界执行一次（`ResolveAndApply`），幂等；同帧内多次请求只产生**一次**窗口层写入（失焦撤销的立即路径除外）。
11. **无头可测。** 解析逻辑不依赖真实窗口：窗口层指令经 `IPointerModeSink` 下发，测试用 `RecordingPointerModeSink` 记录 `{NativeCursorMode, ClipRect, RelativeCapture}` 序列（当前仓库只有 `GLFWWindowSystem` 一个 `WindowSystem` 实现，没有 fake —— 这是必须补的缝）。

### Must（S03 — 模式集合与窗口映射）

12. **五档模式**（对齐 Godot 并覆盖 UE 语义，见 **模式与三方对照**）：
    `Free`（可见/不约束/绝对）、`Confined`（可见/约束/绝对）、`Hidden`（隐藏/不约束/绝对）、`ConfinedHidden`（隐藏/约束/绝对）、`Captured`（隐藏/锁死/相对）。
13. **映射集中在单点。** 纯函数 `MapModeToWindowCommand(mode, bWindowActive, clipRect) → {CursorMode NORMAL|HIDDEN|DISABLED, bClip, bRelativeCapture}`，不依赖 GLFW，可单测；`GLFWWindowSystem` 只实现"按命令写 GLFW/Win32"。
14. **`Captured` 与 `Confined` 的物理差异必须被 oracle 区分**：`Captured` 下绝对坐标无效、只有相对位移；`Confined` 下绝对坐标有效。

### Must（S04 — PIE 生命周期状态机与键位）

15. 进 Play：Play tier 推入 `Captured`（默认），**同帧生效**。
16. **Esc（= Stop Play，保持不变）与 Shift+F1（= PIE Released，本轮改绑）两条通道都必须在同一帧内解析到 `Free`（可见/不约束），且 OS 侧 clip 已撤销**，指针能移出视口点 Inspector。
17. 恢复：Shift+F1 释放后点击视口（`SceneEditingViewportClient.cpp:384-389` 现有行为）→ 回到**游戏请求的模式**（栈未弹出，自动还原）。
18. 失焦：任一非 `Free` 模式在窗口失焦时**挂起为 Free 且 OS 约束撤销**，重获焦点后**按当时的栈重放**（见 I6a/I6b/I6c/I6d）。失焦挂起不改栈。
19. Play 结束 / 进程关闭：`Stop()`、析构、`Engine` 关闭路径最终 resolved = `Free` **且 OS 施加态为空**；栈内残留（泄漏句柄）→ 按 owner 批量清扫并记一条 warn。
20. **崩溃不残留**：`Shutdown` 与崩溃诊断钩子（`Editor.cpp:398` `InstallEditorCrashDiagnostics` 同路径）都必须无条件 `Apply(Free)` + `ClipCursor(nullptr)` + 注销 raw input。**注意区分**：进程被强杀时 OS 会随进程消失释放 clip（不能把它当安全网），但**失焦不会**（P7）；捕获型诊断（进程活着）必须自救。

### Must（S05 — 游戏面 API 与位置）

21. **公开、非作者化**的游戏面 API：请求可见/隐藏、free/confined/captured、查询当前有效模式与是否被捕获。不进反射、不进 `medata`、不作为资产字段。
22. **实时指针位置**：一次采样同时给出 `WindowPx / ViewportPointPx / ReferencePx / RelativeDelta` 与两个有效性位（绝对有效、参考有效），每帧更新一次，更新点在 `UISystem::PollPointer` 之前（详见 **API** §位置契约）。
23. **参考像素映射只有一份实现**：复用 `ScreenUI` 现有 Letterbox 映射（`UISystem::TryUnmapReferencePx` / `HitTestAtWindowMouse` 的既有语义），不新建第二套换算。

### Must（S06 — 与 UI 的关系）

24. 容器打开 → 拥有者（游戏或 UI presenter）在 **UI tier** 请求 `Confined`；关闭即释句柄。UI tier 高于 Game tier，因此能在游戏 `Captured` 之上叠加"可见 + 约束"，且关闭后**自动回到 `Captured`**。**附加 UX 不变量**：任何持有 `Confined` 的容器必须提供键盘可达的关闭路径（否则用户被"关在视口里"动不了）。
25. `UISystem::ShouldBlockWorldPointer()` **保持原义**（hover/pressed/drag 的**拾取门**，`UISystem.cpp:113-122`），本 Feature **不得**改变它、不得让它读指针模式、不得让指针服务读它。分工见 **与 UISystem 的分工**。

### Should
26. 诊断：`GetResolvedState()` 返回 `{Mode, Tier, StackDepth, ClipRect, bSuspendedByFocus}`，`GetAppliedConstraint()` 返回 `{NativeCursorMode, bClipActive, ClipRect, bRawCaptureActive, bWindowActive}`，供状态栏 / 日志 / automation 读。
27. Automation op `editor.get_pointer_policy`（query）：返回上述两个状态 + 最近一条窗口指令，作为 Esc / Shift+F1 / 失焦的自动化 oracle。

### Non-Goals（明确"不做什么"）
- **不做 UE 式 `FInputMode` 输入路由合并。** 不接管"输入去 UI 还是去世界"——那是 `CORE-F29` 的 `EditorGameplayCaptureActive` 门（`InputSystem.h:58-59`）与 `UISystem` 的 hover 门。指针服务只回答"光标物理上长什么样"。
- **不做多窗口 / 多视口并发捕获**（单 GLFW 窗口，一帧一个有效模式；多视口争用由 tier 内 LIFO 收敛，不做真正的多指针）。
- **不做软件光标 / 准星绘制**（消费方自己画）。
- **不做作者化暴露**（无反射类、无 `UCLASS` 式注册、无资产字段）。
- **不替换 ImGui 的 `NoMouseCursorChange` 抑制手段**（`SceneEditingViewportClient.cpp:322-334` 保留为服务内部实现细节；但**不得**反过来用 ImGui 判焦点，见要求 3）。
- **不建通用键位重映射框架**（只落一个释放键设置项，见 §键位变更）。
- **不做 raw input 重写**（沿用 `GLFWWindowSystem.cpp:292-352`）。
- **不做"崩溃后重放会话"**（只保证不残留锚定态，不恢复现场）。

## 键位变更：释放指针 Left Alt → Shift+F1

| 项 | 决定 | 依据 |
|----|------|------|
| **默认释放键** | **`LeftShift + F1`**（UE 同款习惯；与引擎其它 F 键一致） | 主人裁决 R2 |
| **Esc 语义** | **不变**：Esc = Stop Play（`SceneEditingViewportClient.cpp:368-372`） | 主人裁决 R2（覆盖 D-2 原推荐） |
| **可配置性** | 单一设置项 `PointerPolicySettings::ReleaseKey`（默认 Shift+F1），落在编辑器/项目设置；**不建**通用键位映射系统（YAGNI；完整重映射仍属 `ED-F10` Deferred） | 需要给消费方与用户留退路 |
| **LeftAlt 兼容别名** | **默认开启**（一个过渡版本期），可由同一设置项关闭；别名走同一处理路径，不产生额外状态 | 减少既有用户与文档的断裂；消费方提示文案可能写的是 Alt |
| **可见面同步** | 编辑器提示文案 `Editor/src/UI/EditorWindows/EditorViewportWindow.cpp:40` 与 `:45`（当前写着 Captured / Esc=Stop / Alt=Release 与 Released / Click viewport=Capture）必须改为 Shift+F1；`ED-F23` 的 S03 hint（`ED-F23_VIEWPORT_POINTER_CAPTURE_DESIGN.md:75`）与消费方文档同步 | 键位变更是一次**可见的**行为变更 |
| **IMC 门** | 释放时仍置 `SetEditorGameplayCaptureActive(false)`（现状 `SceneEditingViewportClient.cpp:376-380` 的语义不变，只换触发键） | 路由不归本服务 |

> 本表是设计决定，不修改任何代码；落地见 S04。

## Constraints
- Windows 优先：`ClipCursor` 与 Raw Input 是现有实现（`GLFWWindowSystem.cpp:245-277`、`:292-352`）；非 Win32 允许降级为"只切 GLFW 模式，无约束"。
- **Win32 `ClipCursor` 是系统级共享资源**，必须显式释放；**失焦不会自动解除**（P7）。这条决定了 S01 的形状。
- 撤销路径不得以缓存标志为前提（现状 `GLFWWindowSystem.cpp:251-255` 有此前提）—— 施加态一律以 OS 真值（`::GetClipCursor`）为准。
- 打包的 GLFW 是**预编译库**（`minEngine/Third-Party/glfw/lib-mingw-w64/libglfw3.a`，仓库内无 `src/`），头文件声明了 `GLFW_CURSOR_HIDDEN/CAPTURED`（`glfw3.h:1158-1161`）但**不能假定链接进来的 3.4 后端实现了 `CAPTURED`** → 必须运行期探针或走现有 ClipCursor 路径（D-4）。
- focus 判定**只能**来自平台层（`WindowSystem` 回调 / Win32 消息）；不得用 UI 框架状态兜底。
- 服务在渲染帧内解析，不得占用渲染线程；单线程（主线程）即可。
- 不得新增构建目录、不得改动 `FEATURE_REGISTRY.md`、不得改 `.meta`。
- 既存 `ED-F23` 的 Esc / 点击重捕获 UX 不在本 Feature 内被静默改掉（只有释放键按 §键位变更 调整）。

## Preferences
- 句柄 + 栈，而不是布尔 flag + "谁后写谁赢"。
- "意图态"与"施加态"分开存、分开读；**永远用 OS 真值校验缓存**。
- 一个 `ResolveAndApply`，一个纯映射函数，一个 sink 接口 —— 三个小部件，不建平台框架。
- 错误显式化：越序 release、owner 缺失、栈遗留、未 active 却仍 clip → 打 warn 并暴露计数，不静默自愈。

## Assumptions

| # | 假设 | 若不成立 |
|---|------|----------|
| A1 | 单窗口（`GLFWWindowSystem` 单实例，`WindowSystem.h:55` `Get()`），因此"一帧一个有效模式"够用 | 多窗口需求出现时引入 per-window 服务实例，tier 语义不变 |
| A2 | 主线程是唯一调用者（编辑器与游戏都在主线程请求） | 若要多线程，句柄表加锁；**先不做**，在契约里写明"主线程 only" |
| A3 | `UISystem` 的 `ImageMin/ImageSize`（`ScreenUIHitTypes.h:40-41` 的 viewport context）每帧由编辑器注入，服务读它不会读到脏值 | 读不到时 `bReferenceValid=false`，游戏面得到"无效"而不是假坐标 |
| A4 | `GLFW_CURSOR_DISABLED` 足够表达 `Captured`（虚拟无限位移 + 隐藏） | 若需要真 pointer-lock 精度，改用 Raw Input 位移（已在做） |
| A5 | 引擎的 focus 回调在编辑器 `ImGui_ImplGlfw_InitForOpenGL(window, true)`（`Editor/src/.../ImGuiBackend.cpp:52`）之前注册，ImGui 会链式保留前置回调（`imgui_impl_glfw.cpp:662` 保存 prev、`:567` 转发） | 若顺序颠倒导致回调被覆盖 → 用 Win32 `WM_ACTIVATE` 兜底（挂点 `GLFWWindowSystem.cpp:33-66`） |

## Current Architecture

```text
[Editor.cpp:412]  SetCursorVisible(true)            ┐
[PIE Stop:268-274]  clear clip + visible            ├─ 三处直调，互不感知
[SEVC ApplyPointerPolicy:392-429] 每帧重放 hide+clip ┘
                │
                ▼
        WindowSystem (抽象, WindowSystem.h:40/42/47)
                │
                ▼
        GLFWWindowSystem :: glfwSetInputMode + ClipCursor + RegisterRawInputDevices
                │                       ▲
                │                       └─ 只有显式调用才清；★ 无 focus 事件驱动（P7）
                ▼
        InputSystem (物理鼠标 / 虚拟绝对 / IMC 门: EditorGameplayCaptureActive)
                │
                ▼
        UISystem (hover 门 ShouldBlockWorldPointer; ReferencePx 只在事件/拖拽里)
```

编辑侧现有的模式枚举（`SceneEditingViewportClient.h:110-116`）：

| 现有值 | 实际含义 | 新体系归属 |
|--------|----------|-----------|
| `Off` | 不干预 | 无请求（栈空）→ `Free` |
| `EditNavigate` | 隐藏 + clip + raw（`SceneEditingViewportClient.cpp:401-418`） | Editor tier，`Captured` |
| `PieCaptured` | 同上 | Play tier，`Captured` |
| `PieReleased` | 可见 + 无 clip（`:419-426`） | Play tier，**无请求** → 落到 `Free` |

## 模式与三方对照

### 三方对照表（本仓 ↔ UE ↔ Godot）

| 本仓 `EPointerMode` | 可见 | 约束 | 移动源 | UE 对应组合 | Godot `Input.MOUSE_MODE_*` |
|---|---|---|---|---|---|
| `Free` | 是 | 无 | 绝对 | `FInputModeUIOnly` / `FInputModeGameAndUI` + `bShowMouseCursor=true` + Viewport Mouse Capture Mode=`NoCapture`；或 `GameOnly` + `bShowMouseCursor=true` + Default Viewport Mouse Lock Mode=不锁 | `VISIBLE` |
| `Confined` | 是 | 视口（ClipCursor） | 绝对 | `GameAndUI` + `bShowMouseCursor=true` + Capture Mode=`CapturePermanently`（视口内行为接近） | `CONFINED` |
| `Hidden` | 否 | 无 | 绝对 | **无直接等价**（UE 的 `bShowMouseCursor=false` 在 GameOnly 下会同时夺锁；只能靠自定义 CursorWidget 或隐藏系统光标近似） | `HIDDEN` |
| `ConfinedHidden` | 否 | 视口 | 绝对 | **无直接等价**（最接近 `GameAndUI` + 隐藏 + Capture Mode=`CapturePermanently`） | `CONFINED_HIDDEN` |
| `Captured` | 否 | 锁死（虚拟无限） | 相对 | `FInputModeGameOnly` + `bShowMouseCursor=false` + Capture Mode=`CapturePermanently` | `CAPTURED` |

UE 侧的概念（`FInputModeGameOnly / FInputModeGameAndUI / FInputModeUIOnly`、`bShowMouseCursor`、`APlayerController::SetInputMode`、Viewport 的 Mouse Capture Mode 与 Default Viewport Mouse Lock Mode）**把两件事压在一个开关里**：输入去向（游戏 / UI）+ 光标物理形态。本仓这两件事分属两个子系统（`CORE-F29` 门 + `UISystem` 拾取门 vs 本服务），因此上表给出**语义等价而非 API 等价**的映射；`Hidden / ConfinedHidden` 两档是 UE 侧拿不到、Godot 侧显式给出的，本设计**取 Godot 的档位粒度**、取 UE 的"请求即声明意图"（谁请求谁负责，引擎不猜）。

> **两者都不提供、本仓必须自己保证的一条**：UE 与 Godot 的"捕获"都隐含**只在应用处于前台时**成立（失焦由各自平台层释放）。本仓当前没有任何 focus 处理（P7），因此这一条必须显式实现，不能指望"照搬现成设计就自动对"。

### 取舍
- **取 Godot 的 5 档**：因为"隐藏但自由"是消费方光标栈跟手的硬需求（任务书 §4.4），而 UE 的 `bShowMouseCursor=false` 与"夺锁"绑死，表达不出这一档。
- **取 UE 的"请求方声明意图"**：`FInputMode*` 是**显式声明**而不是"改 flag"，天然可栈化 —— 这与本设计的句柄 + tier 栈同构。
- **不取 UE 的输入路由合并**：本仓路由已分成 `CORE-F29`（玩法 IMC 门）+ `UISystem`（hover 门）两层，合并会把一个清晰的两层门变成一个 God object，且会与 `ED-F23`/`BUG-CORE-004` 已稳定的行为冲突。
- **取 UE 的键位习惯**：释放用 `Shift+F1`，不沿用原来的 LeftAlt。
- **取两者共同的隐含前提并显式化**：约束只在应用前台生效（P7 → I6a–I6d）。

### 明确"不做什么"（与上表配套）
1. 不引入 `FInputMode` 概念的等价类（不做 `InputMode` 枚举），因为路由不在这里。
2. 不把 `bShowMouseCursor` 做成 bool（那正是当前 bug 的形状）；可见性是**模式的派生量**（`mode != Free && mode != Confined`）。
3. 不提供 `Hidden` 的"UE 式近似"分支；`Hidden` 直接映射 `GLFW_CURSOR_HIDDEN`。
4. 不做 `CAPTURED` 的"`DISABLED` + 自己 warp 回中心"补丁 —— 那是把两个模式塞进一个模式。
5. **不把"窗口 active"做成一个模式**（不新增 `InactiveFree` 模式值）；它是**约束的作用域条件**，不是策略选择。

## Proposed Architecture

```text
   窗口/平台层                                  策略层                        消费方
┌────────────────────────┐        ┌──────────────────────────────┐
│ GLFW focus callback    │        │ PointerPolicyService         │   Editor tier ◄─┐
│ (SetupWindowEvent—     │───────►│  request stack(tier,mode,clip)│   Play   tier ◄─┤
│  Callbacks :408-415)   │ focus  │  + RefCount + RAII handles   │   UI     tier ◄─┤
│ + WM_ACTIVATE 兜底     │ 事件   │  BeginFrame(): ResolveAndApply│   Game   tier ◄─┘
│ (WndProc :33-66)       │        │  ★ bWindowActive 闸门         │
└────────────────────────┘        │  GetResolvedState()   意图态 │
                                  │  GetAppliedConstraint() 施加态│
                                  └──────────────┬───────────────┘
                                                 │ PointerWindowCommand{mode,clip,relCapture}
                                                 ▼
                                  IPointerModeSink (WindowSystem 适配 / 测试 Recording)
                                                 │
                                                 ▼
                     GLFWWindowSystem: glfwSetInputMode + ClipCursor(∩ client) + RawInput
                                                 │
        UISystem::GetPointerViewportContext() ──► 参考像素映射（唯一实现）
                                                 │
                                                 ▼
                     PointerPolicyService::GetPosition() → PointerPositionSample
```

## Responsibilities

| Unit | 负责 | 不负责 |
|------|------|--------|
| `PointerPolicyService` | 请求栈、tier/LIFO 仲裁、引用计数、帧边界施加、位置缓存、**focus 作用域闸门** | 输入路由、拾取判定、视口布局、画光标 |
| `PointerModeRequest`（句柄） | 生命周期（析构释放）、move-only | 任何策略判断 |
| `MapModeToWindowCommand`（纯函数） | 模式 + `bWindowActive` → `{CursorMode, bClip, bRelativeCapture}` | 平台细节、GLFW 枚举 |
| `GLFWWindowSystem` | 按命令写 GLFW/Win32；**上报 focus 事件**；clip 与客户区求交；回读真实 `GetClipCursor` | 谁该被捕获 |
| `SceneEditingViewportClient` | 把"编辑导航 / PIE 捕获 / PIE 释放"翻译成 Editor tier / Play tier 请求；clip 矩形来源（`MakeViewportImageClipRect`，`SceneEditingViewportClient.cpp:310-320`）；**Shift+F1 / Esc 输入** | 直接调 `WindowSystem` |
| `PlayInEditorSession` | Play 生命周期事件（Enter/Stop）通知策略层 | 光标细节 |
| 游戏面 API（`Pointer::*` facade） | 提供请求 + 查询 + 位置 | 编辑器特定语义 |
| `UISystem` | 拾取门 `ShouldBlockWorldPointer`、参考像素映射、指针事件 | 光标物理形态 |

## Boundaries

- **入向**：编辑器视口、PIE 会话、UI presenter、游戏代码 —— **只允许**请求与查询。
- **出向**：唯一出口是 `IPointerModeSink`（生产实现是 `WindowSystem` 适配）。除它之外**任何**模块不得写光标状态。
- **事件入向**：focus 事件只能由 `WindowSystem` 平台上抛（禁止 UI 框架、禁止消费方自报）。
- **横向**：不 include 编辑器头；不 include `UISystem.h`（位置参考坐标通过一个薄接口取，见 A3/决策 D-5），避免 Platform → UI 的编译期依赖反转。

## Ownership & Lifetime

| 对象 | 拥有者 | 生命周期挂载点 |
|------|--------|----------------|
| `PointerPolicyService` | `Engine`（引擎子系统，单例 `HasInstance/Get` 风格与 `UISystem`/`InputSystem` 一致） | `Engine::Initialize` 末尾创建；在 `Engine::ShutdownSystems()` 里 **RenderSystem 之后、WindowSystem 之前**销毁（现状 `Engine.cpp:540-545` 与 `:547-552` 之间）—— 此刻 `WindowSystem` 仍存活，可下发最后一条 Free |
| `PointerModeRequest` | 请求方（栈帧 / 成员变量） | 析构释放；也可 `Release()` 提前释放 |
| Clip 矩形来源 | 请求方（编辑器给视口图像矩形；游戏给视口矩形） | 每帧可重算，服务只保存最近一次生效值 |

- 请求方**不得**在热路径上每帧 `Request/Release` 抖动（会造成模式翻页）；需要"每帧确认"的场景用 `Update(handle, ...)`（Should）。
- 句柄持有者先于服务销毁：服务析构时强制清栈，不回调释放方。

## Data Flow

1. 帧开始：`WindowSystem::PollEvents()` → 焦点事件已在上一步回调里投递给服务 → `InputSystem` 更新物理鼠标。
2. `PointerPolicyService::BeginFrame()`：更新 `bWindowActive` → Watchdog（未 active 却仍约束则强撤）→ 读 `UISystem::GetPointerViewportContext()` → 采样位置 → `ResolveAndApply()`。
3. 时钟顺序中 `UISystem::PollPointer()` 在服务之后：UI 与本服务看到**同一帧同一像素**（oracle O4）。
4. 本帧内任何请求/释放只修改栈，不立即写窗口；下一条 `BeginFrame` 统一施加（"同帧多次请求只写一次"）。**唯一例外**是失焦撤销：走"立即路径"，当帧内立刻撤销（要求 2）。
5. 编辑器 `EndFrame` 末尾（`SceneEditingViewportClient.cpp:173-183`）**只更新请求内容**（clip 矩形/目标模式），不再直接调 `WindowSystem`。

## Control Flow（帧序，硬约束）

```text
[平台事件回调] FocusLost  ──► PointerPolicy::OnWindowFocusLost() ──► 立即 Apply(Free + clip=off + raw=off)
PollEvents → InputSystem 采样 → PointerPolicy.BeginFrame(Watchdog+Resolve+Apply+Sample)
           → UISystem.PollPointer → 玩法 tick → 编辑器 GL 绘制(ImGui) → 视口 EndFrame(更新请求)
[平台事件回调] FocusGained ──► bWindowActive=true（下一次 BeginFrame 按当时栈重放）
```
ImGui 的 `NoMouseCursorChange` 抑制（`SceneEditingViewportClient.cpp:322-334`）由服务在"解析到非 Free"时统一设置，**不在**请求方各自设；反过来，**禁止**用 ImGui 的焦点状态驱动本服务的任何判定。

## State & Invariants

### 两个可分别观测的量（P7 的根治结构）

| 量 | 含义 | 读口 | 谁写 | 失焦后必须变成 |
|----|------|------|------|----------------|
| **意图态** `PolicyResolvedState` | 栈解析出的模式（上层"打算"怎样） | `GetResolvedState()` | 服务解析 | `Free`（挂起） |
| **施加态** `AppliedConstraint` | OS / 窗口层**真实**状态：`{NativeCursorMode, bClipActive, ClipRect, bRawCaptureActive, bWindowActive}` | `GetAppliedConstraint()`（Win32 以 `::GetClipCursor` 回读） | sink 执行后回写 | `bClipActive=false`、`bRawCaptureActive=false`、`NativeCursorMode=NORMAL` |

**核心判据**：任何时刻施加态必须与"意图态 + `bWindowActive`"一致；不一致即缺陷（Watchdog 修复并 warn）。

### 模式解析（与 PIE 无关的一般规则）

| 状态 | 事件 | 转移 | 不变量 |
|------|------|------|--------|
| 任意 | `Request(tier, mode)` | 栈内插入/计数 +1；下一帧解析 | 不立即写窗口 |
| 任意 | 句柄析构 | 计数 -1，为 0 则出栈；下一帧解析 | 非顶层释放**不改变**当前有效模式 |
| 栈空 | 解析 | → `Free` | I2 |
| 非栈空 | 解析 | 取最高 tier 的栈顶（同 tier LIFO） | I3 |
| 任意 | `OnWindowFocusLost` | 立即：施加态 → Free/未约束；意图态挂起；`bWindowActive=false` | **I6a/I6b/I6c** |
| 未 active | 解析 | 一律解析为 `Free`（不施加任何约束），栈保持 | I6a/I10 |
| 未 active | `Request/Release` | 只改栈，**不施加** | I10 |
| 重获焦点 | `OnWindowFocusGained` | `bWindowActive=true` → 下一次 `BeginFrame` 按**当时**栈重放（不回放历史） | I6d |

### PIE 生命周期状态机

| 状态 | 事件 | 转移 | OS 施加态 | 不变量 / 备注 |
|------|------|------|-----------|---------------|
| `EditorFree` | `BeginPlay` | `PlayCaptured` | `DISABLED + clip + raw` | Play tier 入栈 `Captured`，同帧生效 |
| `PlayCaptured` | **`Shift+F1`**（`LeftAlt` 兼容别名） | `PlayReleased` | `NORMAL + clip off + raw off` | Play tier 请求弹出 → 解析到 `Free`；**IMC 门同时置 false**（现状 `SceneEditingViewportClient.cpp:376-380` 语义不变，只换键） |
| `PlayReleased` | LMB on viewport | `PlayCaptured` | 同上反向 | 现有行为 `:384-389` |
| `PlayReleased` | `BeginPlay`(重入) | `PlayCaptured` | 同上反向 | 幂等 |
| `PlayCaptured` / `PlayReleased` | `Esc`（Stop） | `EditorFree` | `NORMAL + clip off + raw off` | **同帧**撤销（等价 `PlayInEditorSession.cpp:268-274` 的意图，改由服务统一施加） |
| 任意非 Free | **失焦** | `SuspendedFree` | `NORMAL + clip off + raw off` | **立即**撤销（要求 2）；栈不改 |
| `SuspendedFree` | 焦点回来 | 按当时栈重放 | 与栈一致 | 若失焦期间栈被清空 → 停在 `Free` |
| 任意 | `Stop` / `Shutdown` | `EditorFree` + 栈清空 | `NORMAL + clip off + raw off` | I5 |
| 任意 | 崩溃诊断钩子 | 强制 `Free` | 同上 | I5；进程被强杀由 OS 收拾（**不是安全网**） |
| 任意 | 容器打开（UI tier） | 叠加 `Confined`（栈顶 tier=UI） | `NORMAL + clip` | 关闭容器 → 自动回落到下层（通常 `Captured`） |

### 不变量（Invariants）

- **I1 单次施加**：每帧最多一次窗口层写入（失焦撤销的立即路径除外）；`ResolveAndApply` 幂等。
- **I2 空栈即 Free**：不存在"栈空但光标被隐藏"。
- **I3 层级 + LIFO**：高 tier 恒覆盖低 tier；同 tier 最近存活请求生效；释放非顶层不改结果。
- **I4 引用计数**：同 `(tier, mode, clipSpec)` 合并；N 个持有者需要 N 次释放才出栈。
- **I5 归零**：`Stop` / `Engine::Shutdown` 之后 `GetResolvedState().Mode == Free` **且** `GetAppliedConstraint()` 满足 `bClipActive == false && bRawCaptureActive == false && NativeCursorMode == NORMAL`。
- **I6a（focus 双条件 · 意图侧）**：窗口失焦后，`GetResolvedState().Mode == Free` 且 `GetResolvedState().bSuspendedByFocus == true`。
- **I6b（focus 双条件 · OS 侧）**：窗口失焦后，`GetAppliedConstraint().bClipActive == false` 且 `bRawCaptureActive == false` 且 `NativeCursorMode == NORMAL`，且 Win32 下 `::GetClipCursor` 回读不窄于屏幕矩形。**"意图态已 Free 但 OS 仍 clip" 属于硬违例，不是可接受的中间态。**
- **I6c（收敛时限）**：I6a 与 I6b 必须在**失焦当帧结束前**同时成立（至迟 `Engine` 下一次 `BeginFrame` 之前）；"先 Free、下一帧才 clip-off"视为违例。
- **I6d（重放）**：重获焦点后施加态必须按**当时**的栈重放，不得"重置为 Free 就完事"，也不得回放失焦前的历史。
- **I7 Captured 无绝对坐标**：`bAbsoluteValid == false`。
- **I8 不消费**：服务**不得**调用 `WindowSystem::ConsumeRelativeMouseDelta()`（消费权属于 `InputSystem` 的 look intent，`WindowSystem.h:44-47` 语义为"consume once"）。
- **I9 无反向依赖**：`UISystem` 不读指针服务；`ShouldBlockWorldPointer` 语义不变。
- **I10 作用域**：任何约束（clip / raw / hidden / captured）**只在 `bWindowActive == true` 时存在**；`bWindowActive == false` 时栈可非空但施加态必须为 Free。
- **I11 矩形合法性**：施加的 clip 矩形必须是"请求矩形 ∩ 客户区屏幕矩形"；交集为空则不施加（并 warn）。

## API / Interface Semantics

> 签名是**契约**，本 Design 不落地任何代码。

### 模式与层级

```cpp
enum class EPointerMode : uint8_t
{
    Free = 0,          // visible, unconfined, absolute
    Confined,          // visible, clipped, absolute
    Hidden,            // hidden, unconfined, absolute  ("hidden but free")
    ConfinedHidden,    // hidden, clipped, absolute
    Captured,          // hidden, grabbed, relative
};

enum class EPointerTier : int
{
    Default = 0,
    Game    = 40,
    UI      = 60,
    Play    = 80,
    Editor  = 100,
};

struct PointerClipSpec
{
    bool bEnabled = false;
    WindowCursorClipRect Rect{};   // 复用 WindowSystem.h:12-20 的既有类型（屏幕坐标）
};
```

### 平台侧新增（S01 的必要条件）

```cpp
// WindowSystem：focus 事件源（唯一合法来源；禁止 ImGui 兜底）
typedef std::function<void(bool focused)> OnWindowFocusFunc;
void RegisterOnWindowFocusCallback(const OnWindowFocusFunc& callback);

// WindowSystem：真实施加态回读（Win32 用 ::GetClipCursor 核对，不用缓存自证）
struct WindowCursorConstraintState
{
    int  NativeCursorMode = 0;   // 对应 NORMAL / HIDDEN / DISABLED
    bool bClipActive = false;    // 真实 ClipCursor 状态
    WindowCursorClipRect ClipRect{};
    bool bRawCaptureActive = false;
};
virtual WindowCursorConstraintState GetCursorConstraintState() const = 0;
```

### 请求（编辑器 / 游戏同一入口，靠 tier 区分）

```cpp
class PointerModeRequest   // move-only RAII handle
{
public:
    PointerModeRequest() = default;
    PointerModeRequest(PointerModeRequest&&) noexcept;
    PointerModeRequest& operator=(PointerModeRequest&&) noexcept;
    ~PointerModeRequest();                 // 析构 = Release(1)
    PointerModeRequest(const PointerModeRequest&) = delete;

    void Release();                        // 幂等提前释放
    bool IsValid() const;
};

class PointerPolicyService
{
public:
    static bool HasInstance();
    static PointerPolicyService& Get();

    // 主线程 only。tier 由请求方给定（见 Ownership 规则）。
    PointerModeRequest Request(EPointerTier tier, EPointerMode mode, const PointerClipSpec& clip = {});

    // 意图态
    EPointerMode GetResolvedMode() const;
    PointerResolvedState GetResolvedState() const;   // {mode, tier, stackDepth, clip, bSuspendedByFocus}
    bool IsPointerVisible() const;                    // derived: Free || Confined
    bool IsPointerCaptured() const;                   // derived: Captured

    // 施加态（P7 oracle）
    AppliedConstraint GetAppliedConstraint() const;   // {nativeCursorMode, bClipActive, clipRect, bRawCaptureActive, bWindowActive}

    // 位置（每帧一次采样）
    const PointerPositionSample& GetPosition() const;

    // 平台事件入口（由 WindowSystem 回调驱动）
    void OnWindowFocusLost();
    void OnWindowFocusGained();

private:
    void BeginFrame();                     // Watchdog + ResolveAndApply + Sample（Engine 帧驱动）
    void ResolveAndApply();
    void ApplyImmediateRelease();          // 失焦立即撤销（不走栈解析）
};
```

### 游戏面 facade（公开、非作者化）

```cpp
namespace Pointer
{
    // 请求：返回值必须被持有（RAII）；丢弃句柄 = 立即释放
    [[nodiscard]] PointerModeRequest Show();                      // Free
    [[nodiscard]] PointerModeRequest Hide();                      // Hidden（隐藏但自由）
    [[nodiscard]] PointerModeRequest ConfineTo(const WindowCursorClipRect& rect);       // Confined
    [[nodiscard]] PointerModeRequest ConfineHiddenTo(const WindowCursorClipRect& rect); // ConfinedHidden
    [[nodiscard]] PointerModeRequest Capture();                   // Captured

    EPointerMode GetMode();
    bool IsCaptured();
    bool IsVisible();

    const PointerPositionSample& GetPosition();
}
```
规则：以上都进 Game tier（facade 内部固定 tier，不允许调用方抬层级；抬层级只能由编辑器侧调用带 tier 的 `Request`）。**失焦期间 facade 的请求只入栈不生效**（I10），重获焦点后自动生效。

### 位置契约

```cpp
struct PointerPositionSample
{
    Vector2 WindowPx{0.0f, 0.0f};         // 物理窗口像素，左上原点
    Vector2 ViewportPointPx{0.0f, 0.0f};  // 视口图像内像素（windowPx - ImageMin）
    Vector2 ReferencePx{0.0f, 0.0f};      // 参考像素空间（ScreenUI 设计分辨率）
    Vector2 RelativeDelta{0.0f, 0.0f};    // 本帧相对位移（不消费 InputSystem 的量）
    bool    bAbsoluteValid = false;       // Captured 时 false（I7）
    bool    bReferenceValid = false;      // 无 viewport context / 未启用指针路由时 false
    uint64_t FrameStamp = 0;
};
```

| 项 | 契约 |
|----|------|
| 更新时机 | 每帧一次，`PointerPolicyService::BeginFrame`，在 `UISystem::PollPointer()` **之前**（同一帧同一像素） |
| 空间定义 | `WindowPx` = `InputSystem::GetMousePosition()`（物理，`InputSystem.h:82`）；`ViewportPointPx` = `WindowPx - ScreenUIPointerViewportContext::ImageMin`（与 `UISystem.cpp:175-179` 现有算式一致）；`ReferencePx` = 经 `ScreenUI` Letterbox 反映射（唯一实现，复用 `UISystem::TryUnmapReferencePx` 语义） |
| 有效性 | 无 viewport context / 映射失败 → `bReferenceValid=false`，**不返回假坐标** |
| Captured 下 | `bAbsoluteValid=false`，只有 `RelativeDelta` 可信；`RelativeDelta` 为累加量，**不得**取自 `ConsumeRelativeMouseDelta`（I8） |
| 归属 | 位置缓存由服务持有；映射实现留在 `UISystem`（决策 D-5） |
| 线程 | 主线程 only |

### 键位设置契约

```cpp
struct PointerPolicySettings
{
    // 默认 LeftShift+F1；LeftAlt 作为兼容别名默认保持开启一个过渡版本期。
    InputKey ReleaseKeyPrimary = InputKeys::Key_F1;   // 配合 Shift 修饰
    bool     bKeepLeftAltAlias = true;
};
```

### 编辑器侧新增（迁移后）

```cpp
// SceneEditingViewportClient：不再直接调 WindowSystem
void SceneEditingViewportClient::UpdatePointerRequests();                 // 每帧刷新 Request 内容
void SceneEditingViewportClient::HandlePlayPointerReleaseInput();         // Shift+F1 / (LeftAlt alias) / Esc
PointerModeRequest m_NavigateRequest;                                     // Editor tier, Captured
PointerModeRequest m_PlayPointerRequest;                                  // Play tier, Captured
```

## 与 UISystem 的分工

| 问题 | 谁回答 | 依据 |
|------|--------|------|
| 光标现在长什么样（可见/约束/锁死） | `PointerPolicyService` | 本设计 |
| 指针现在是否压在 UI 上（拾取门） | `UISystem::ShouldBlockWorldPointer()` | `UISystem.cpp:113-122`（`Hovered/Pressed/DragSession` 三者之一非空） |
| 指针在世界 / UI 的哪个像素 | `PointerPolicyService::GetPosition()`（坐标） + `UISystem::HitTestAt`（命中） | 位置与命中分离 |
| 输入事件去玩法还是去 UI | `CORE-F29` 门（`InputSystem::EditorGameplayCaptureActive`）+ UI 路由 | 明确**不归**本服务 |
| 窗口是否 active | **平台层**（`WindowSystem` focus 回调） | P7；**不**用 ImGui `AppFocusLost` |
| 容器打开时鼠标该怎样 | 容器拥有者请求 **UI tier `Confined`** | 本设计 §Requirements 24 |

三条硬边界：
1. `ShouldBlockWorldPointer()` **不读**指针模式（否则容器打开 → 阻挡 → 反馈环）。
2. 指针服务 **不读** `ShouldBlockWorldPointer()`（hover 变化不得改变光标物理状态；否则"鼠标滑过 UI 就掉捕获"）。
3. 容器关闭必须释放它自己的句柄（RAII），**不得**"重置为 Free"——重置会把下层游戏的 `Captured` 一起清掉。

## Failure Model

| 场景 | 期望行为 | 落点 |
|------|----------|------|
| **★ 约束泄漏（P0 独立失效模式）** | 触发条件：任一约束生效中窗口失焦（Alt+Tab / 点其他进程 / UAC 弹窗抢焦点）。期望：同帧撤销 clip + raw + hidden + captured；`GetAppliedConstraint()` 立即反映。观测：`GetAppliedConstraint().bClipActive`、Win32 `::GetClipCursor` 回读、手动到其他窗口移动鼠标。恢复路径：focus 回调的立即路径；回调丢失时由 Watchdog 在下一帧强撤 + warn。 | I6a–I6d / 要求 1–3、6 |
| **窗口失焦**（策略层） | 意图态挂起为 `Free`（`bSuspendedByFocus=true`），栈保留 | I6a |
| **窗口重获焦点** | 按**当时**栈重放（不回放历史）；若失焦期间栈已空则停在 `Free` | I6d |
| **本窗口 active 但指针移出窗口矩形** | clip 生效时**物理上不可能**（这正是 clip 的定义）→ 属于"约束正常工作"，不是异常。反之"窗口非 active 而 clip 仍在"才是泄漏（见第一行）。`Confined` 期间用户无法把指针移出容器 → 要求 24 的键盘关闭路径是硬要求 | I10 / 要求 24 |
| **clip 矩形部分落在客户区外** | 施加前与客户区求交（I11）；交集为空则不施加 + warn（避免夹到别的显示器） | I11 |
| **Play 结束** | 同帧 `Free` 且 OS 施加态清空；Play / UI 请求全部出栈但**不**影响编辑器请求 | I5；对照 `PlayInEditorSession.cpp:268-274` |
| **进程正常退出** | `Engine::ShutdownSystems()` 在 `WindowSystem` 销毁前下发最后一次 Free（位置见 Ownership）；`GLFWWindowSystem::Shutdown` 的清 clip（`GLFWWindowSystem.cpp:177-180`）为第二道保险 | I5 |
| **崩溃（进程活着的诊断态）** | 崩溃诊断钩子（与 `Editor.cpp:398` 同一注册路径）强制 `Apply(Free)` + `ClipCursor(nullptr)` + 注销 raw input；**不**试图恢复会话 | I5 |
| **崩溃（进程被杀）** | OS 随进程消失释放 clip 与窗口状态。**文档明确：这不是安全网** —— 失焦**不会**自动释放（P7），所以不能靠"反正进程会死"省掉 focus 处理 | — |
| **多请求叠加 / 越序释放** | 计数保护：非顶层释放不改变模式；越序/未知句柄 → 忽略并 warn（不抛） | I3/I4 |
| **句柄泄漏（owner 提前销毁）** | `SweepOwner` 或 `Stop`/`Shutdown` 时按 tier 清扫；栈深度非 0 时 warn 并记录最后一次 owner 标签 | I5 |
| **请求抖动**（每帧 Request/Release） | 服务统计"同帧入栈/出栈超过阈值"，warn 一次 | Preferences |
| **编辑器热重载 / 重新打开项目** | 以 owner generation 计：重新注册的 owner 是新句柄；旧 owner 的残留由 `SweepOwner(generation)` 清理。策略栈**不跨项目**保留 | R7/热重载 |
| **viewport 切换（submodule 变）** | clip 矩形来源失效 → 请求方当帧无 `bEnabled`；服务解析为 `Captured` 无 clip（不是"用上帧脏矩形"） | A3 |
| **ImGui 抢光标** | 服务统一设 `ImGuiConfigFlags_NoMouseCursorChange`（非 Free 时），消除 `ED-F23` 列的 flicker；但**焦点判定绝不用 ImGui** | Control Flow / 要求 3 |
| **GLFW `CAPTURED` 缺失** | 探针失败 → `Confined` 退化为 `NORMAL + ClipCursor`（现有路径），不阻塞 | 决策 D-4 |

## Alternatives & Trade-offs

### Option A（推荐）：引擎级服务 + tier 栈 + RAII 句柄 + focus 作用域闸门
- 优点：单一拥有者；"谁赢"可解释、可测；失焦泄漏从结构上消除（`bWindowActive` 是解析的输入，不是补丁）；编辑器/游戏/UI 同一入口。
- 代价：一个跨 Platform/Editor 的新服务；迁移三处调用点；新增一个 focus 回调。

### Option B：只在现有 `ApplyPointerPolicy` 里加"失焦就不 clip"
- 优点：本行修复最小。
- 否决理由：**失焦时没有人调用 `ApplyPointerPolicy`** —— 它由 `EndFrame` 驱动（`SceneEditingViewportClient.cpp:143-183`），而失焦窗口通常不再产生帧内的输入路径；即便还有帧，也仍是"三处直调"的老结构，独立 Game 进程无入口，且无法修正"OS 已施加但没人撤"的时序。**只作为 S01 的兜底手段（Watchdog）存在，不是终态。**

### Option C：保留 `ED-F23` 的 per-viewport 策略，只补"隐藏但自由"档
- 否决理由：仍无引擎级拥有者，独立 Game 无入口（P3），PIE/UI/编辑器争用仍在；`Stop()` 与视口每帧重放的顺序依赖（P1）不消失；P7 也不会被结构性解决。

### Option D：照搬 UE `FInputMode`（把输入路由一起收进来）
- 否决理由：本仓路由已分两层且已稳定（`CORE-F29` + `UISystem`），合并会产生一个同时管输入、拾取、光标的 God object。**明确不做**。

### Option E：只做游戏面 API（`Capture/Show`），不加栈
- 否决理由：问题不是"没有 API"而是"三处直调互相覆盖"（P1）+ "约束会泄漏"（P7）；无栈的 API 只是把"最后调用者赢"包装得更体面。

## Integration Impact

| 受影响 | 影响 | 兼容性 |
|--------|------|--------|
| `WindowSystem` | 新增 `RegisterOnWindowFocusCallback`、`GetCursorConstraintState()`；`SetCursorClipRect` 语义收紧为"与客户区求交"；不作删除 | 纯增量 + 一处语义收紧（空交集不再施加） |
| `GLFWWindowSystem` | `SetupWindowEventCallbacks`（`:408-415`）增加 focus 注册；自定义 WndProc（`:33-66`）可选加 `WM_ACTIVATE` 兜底；`SetCursorClipRect` 求交 + 回读 | 行为修正（P7） |
| `SceneEditingViewportClient` | `ApplyPointerPolicy` 改为更新请求；`ViewportPointerMode` 删除（D-9）；**释放键改 Shift+F1** | 行为等价 + **一处可见键位变更** |
| `PlayInEditorSession` | `Stop()` 里的直接清理改为通知服务 | 行为等价 |
| `Editor.cpp:412` | 启动期的 `SetCursorVisible(true)` 删除（服务首帧即 `Free`） | 修正 P4 |
| `UISystem` | 新增公开 `TryMapWindowPxToReferencePx`（或等价）供服务采样 | 纯增量 |
| 独立 Game 进程 | 首次获得指针策略能力；后台运行时不再被锁 | 新增 |
| **用户可见文案** | `Editor/src/UI/EditorWindows/EditorViewportWindow.cpp:40` 与 `:45` 的提示（现为 Captured / Esc=Stop / Alt=Release 与 Released / Click viewport=Capture）改为 Shift+F1；`ED-F23` S03 hint 同步 | **可见变更，需同步帮助文档** |
| **消费方（minCraft）** | 若其文档/提示写"Alt 释放"，需同步为 Shift+F1（LeftAlt 别名默认仍可用，短期不炸） | 需通知 |
| 测试 | 新增 `RecordingPointerModeSink` + `pointer-policy` suite | 新增 |

## Migration / Compatibility
- 迁移**不保留**旧的直调路径（前向开发：不留双轨）；`SetCursorVisible/SetCursorClipRect` 仍在 `WindowSystem` 上（内部实现细节），但引擎内除 sink 适配外无调用者，由一条 grep 断言守住（S03 验收）。
- **键位**：LeftAlt → Shift+F1 是**可见变更**；LeftAlt 作为兼容别名默认保留一个过渡版本期（§键位变更），并在编辑器提示与 `ED-F23` S03 hint 同步；消费方文档需通知。
- 不改任何 `.medata` / `.meta`；不改场景资产；不引入序列化字段（`PointerPolicySettings` 只有两个标量，落在编辑器/项目设置里）。
- 不新增构建目录；改 `CMakeLists` 只加新源文件（S02 落地时）。

## Verification Strategy

### 可观察 oracle

| # | Oracle | 观测点 | 断言 |
|---|--------|--------|------|
| **O1** | 解析状态（意图态） | `GetResolvedState()` | `{mode, tier, stackDepth, clip, suspended}` 与期望矩阵逐条相等 |
| **O2** | 窗口层指令序列 | `RecordingPointerModeSink` 的 `{NativeCursorMode, ClipRect, RelativeCapture}` | 等于 `MapModeToWindowCommand` 的期望输出（"GLFW 输入模式值"的可测替身；真实值再用 `GetAppliedConstraint().NativeCursorMode` 对一次） |
| **O3** | Shift+F1 / Esc 后可点视口外 | `IsPointerCaptured()==false` 且 `IsVisible()==true` 且 `GetAppliedConstraint().bClipActive==false`；编辑器自动断言用 `editor.get_pointer_policy` | 三个条件同时成立；手工再点一次 Inspector 树行验证真能选中 |
| **O4** | 位置一致性 | `PointerPolicyService::GetPosition().ReferencePx` vs 同帧 `UIPointerEvent::ReferencePx` / `UISystem::GetDragPointerReferencePx()` | 同一帧同一像素，分量差 `== 0`（或 ≤ 0.5px 浮点容差，实现时钉死一种） |
| **O5** | 不残留（Stop / Shutdown） | `Shutdown` 前保持 `Captured`，断言 `GetAppliedConstraint()` 与 sink 最后一条记录 | `NativeCursorMode == NORMAL`、`bClipActive == false`、`bRawCaptureActive == false` |
| **O6** | 无绝对坐标 | `Captured` 帧 | `bAbsoluteValid == false`，`RelativeDelta` 非零（移动鼠标时） |
| **O7** | 门不回归 | `test screen-ui-pointer` + `test gameplay-input-gate` | 全绿（证明 `ShouldBlockWorldPointer` 与 IMC 门未被牵连） |
| **O8（新1）** | **PIE 中切到其他进程窗口 → 指针可自由移到桌面任意位置** | `GetAppliedConstraint()` + Win32 `::GetClipCursor` 回读 + 手动 | 失焦后**同一帧**：`bClipActive==false`、`bRawCaptureActive==false`、`NativeCursorMode==NORMAL`、`bWindowActive==false`；`::GetClipCursor` 回读不窄于屏幕矩形；手动：指针可移到任务栏 / 另一个应用窗口内任意位置 |
| **O9（新2）** | **切回本窗口 → 按栈恢复到进入前的模式（不是被重置成 Free）** | `GetResolvedState()` 前后对比 | 失焦前 `mode=M`（非 Free）→ 失焦期间 `Free + bSuspendedByFocus` → 回焦后 `mode==M`、`bSuspendedByFocus==false`、施加态与 M 对应（M=Captured → `DISABLED+clip+raw`）。**反例断言**：回焦后若得到 `Free` 且栈非空 → FAIL |
| **O10（新3）** | **`Stop` / `Shutdown` 后 clip 状态读口必为空** | `GetAppliedConstraint()` + `::GetClipCursor` | `bClipActive==false` 且回读不窄于屏幕矩形；`NativeCursorMode==NORMAL`；`bRawCaptureActive==false`。`Stop` 与 `Shutdown` 两条路径**分别**断言 |

### 最小自动化验证路径

```powershell
# 1) 构建（目标化）
cmake --build D:\Dev\GitRepo\minEngine\minEngine\build --target minEngineTests

# 2) 跑新 suite
cd D:\Dev\GitRepo\minEngine\minEngine\bin; .\minEngineTests.exe test pointer-policy

# 3) 回归两条相关门
cd D:\Dev\GitRepo\minEngine\minEngine\bin; .\minEngineTests.exe test screen-ui-pointer
cd D:\Dev\GitRepo\minEngine\minEngine\bin; .\minEngineTests.exe test gameplay-input-gate

# 4) 全量（协调者收尾统一跑，本设计不跑）
powershell -NoProfile -ExecutionPolicy Bypass -File D:\Dev\GitRepo\minEngine\scripts\verify.ps1
```

`pointer-policy` suite（doctest `TEST_CASE`，suite id 走 `Tests/TestSuiteRegistration.cpp` 的既有注册方式，参照 `screen-ui-pointer` 的注册形状 `TestSuiteRegistration.cpp:778`）：

| 用例 | 断言（必须逐条可读） |
|------|----------------------|
| **`pointer-policy - focus loss releases os clip`（O8）** | 经 `RecordingPointerModeSink` 进入 `Captured` → 投递 `OnWindowFocusLost()` → **立即**（不等 BeginFrame）sink 最后一条为 `{NORMAL, clip=off, raw=off}`；`GetAppliedConstraint()` 同上；`GetResolvedState().bSuspendedByFocus==true` |
| **`pointer-policy - focus regain restores stack`（O9）** | 失焦前 `mode=Captured` → 失焦 → `OnWindowFocusGained()` + `BeginFrame` → `mode==Captured`、施加态回到 `DISABLED+clip+raw`；再断言"失焦期间新增的 UI 请求"在回焦后成为栈顶（按当时栈重放，不回放历史） |
| **`pointer-policy - stop and shutdown clear clip`（O10）** | `Captured` 中 `Stop` → 施加态清空；再建 `Captured` 调 `Shutdown` → 施加态清空（两条路径分别断言） |
| **`pointer-policy - watchdog kills clip when inactive`** | 人为屏蔽 focus 回调、构造"`bWindowActive==false` 但 sink 侧仍 clip"→ 下一次 `BeginFrame` 后施加态清空 + 一条 warn |
| **`pointer-policy - clip intersects client rect`（I11）** | 请求矩形部分越界 → 施加值 == 交集；完全越界 → 不施加 + warn |
| `pointer-policy - resolve prefers higher tier` | Game 请求 `Captured` 后 UI 请求 `Confined` → `GetResolvedMode()==Confined`，`tier==UI`，`stackDepth==2` |
| `pointer-policy - refcount coalesces` | 两个同 tier 同模式句柄，释放一个 → 模式不变；释放两个 → 回 `Free` |
| `pointer-policy - out-of-order release is inert` | Editor tier + Game tier，释放 Editor → 模式落到 Game 的请求（不是 `Free`），无 warn；释放未知句柄 → 忽略并 warn 一次 |
| `pointer-policy - pie captured then shift-f1 release` | `BeginPlay` → `Captured`+clip；`Shift+F1` → `Free`+clip off（同帧），`IsPointerCaptured()==false`；`LeftAlt` 别名路径同样生效 |
| `pointer-policy - pie stop returns free` | `Captured` 中 `Esc`（Stop）→ 立即 `Free`；断言 sink 最后一条 `NORMAL` + clip on=false |
| `pointer-policy - position reference matches ui event` | 构造 viewport context，喂 `InputSystem` 鼠标位，`GetPosition().ReferencePx` 与 `HitTestAtWindowMouse` 的参考点相等（O4） |

**手工点验（编辑器）**

1. 【P7 / O8】Play → 光标隐藏锁定 → **Alt+Tab 到别的进程**（或点任务栏、点另一个应用窗口）→ 鼠标在整个桌面**可自由移动**，其他应用可正常操作。
2. 【O9】切回本窗口 → 光标重新隐藏锁定（回到 Captured），无需再点视口。
3. 【O3】`Shift+F1` → 光标出现且能点 Hierarchy/Inspector（**不是在视口内**）→ 点回视口 → 重新锁定。
4. 【O3】`Esc` → Play 结束且光标可用。
5. 【别名】`LeftAlt` 仍能释放（别名默认开）；在设置里关掉别名后 `LeftAlt` 不再释放。
6. 【O10】Play 中点关闭窗口 / 退出编辑器 → 退出后光标在整个桌面自由。

## Slices

| Slice | 目标 | 独立验收 |
|-------|------|----------|
| `S00` | 登记 + 本 Design | `FEATURE_REGISTRY` 一行 + 本文（已具备） |
| **`S01`（P0 · 最早）** | **focus 作用域约束**：focus 回调 + 立即撤销 + 施加态读口 + Watchdog。**不动策略面、不加新 API 面、不迁编辑器调用点** | 见下 **§S01 独立验收** |
| `S02` | 服务骨架 + sink 缝 + `MapModeToWindowCommand` | `pointer-policy` 的 tier / refcount / 越序 3 用例绿 |
| `S03` | 窗口映射接线 + 编辑器三处迁移（删直调） | `EditNavigate/PieCaptured/PieReleased` 行为等价；grep 断言无直调 |
| `S04` | PIE 状态机 + **Shift+F1 改绑** + 崩溃归零 | `pie captured then shift-f1 release` / `pie stop returns free` / `stop and shutdown clear clip` 三用例绿 + 手工点验 3–6 |
| `S05` | 游戏面 facade + 位置契约 + `editor.get_pointer_policy` | O4/O6 绿；独立 Game 可捕获 |
| `S06` | UI 容器集成（UI tier `Confined`/`ShowUi`）+ 文档与提示文案收口 | **Done** — O7 绿；Shift+F1 文案已改；引擎不自动请求，消费方 RAII |

### §S01 独立验收（不依赖新 API 面）

**为什么它能独立**：S01 只需要三样东西 —— ①一个 focus 事件源；②一个"立即撤销"的动作；③一个真实施加态读口。它不需要模式栈、不需要 tier、不需要游戏面 API，也不改任何调用点；编辑器现有的 hide+clip 路径（`SceneEditingViewportClient.cpp:401-418`）**保持不变**，只是失焦时机被平台层打断并撤销。

| 断言 | 手段 |
|------|------|
| A. 失焦当帧 clip 撤销 | Win32 `::GetClipCursor` 回读（新读口 `GetCursorConstraintState()` 内部就用它）不窄于屏幕矩形 |
| B. 失焦当帧 raw input 注销 | `bRawCaptureActive==false`；侧证：`ConsumeRelativeMouseDelta()` 不再随"在别的应用里移动鼠标"增长 |
| C. 失焦当帧光标可见 | `NativeCursorMode==NORMAL`；手动：其他应用能看到系统光标 |
| D. 回焦重放 | 回到失焦前的请求模式，手动再进 PIE 验证一次 |
| E. 不漏 | `watchdog kills clip when inactive` 用例在人为屏蔽 focus 回调时仍能撤销 + warn |
| F. 无回归 | `test screen-ui-pointer` / `test gameplay-input-gate` 全绿；`EditNavigate`（拖拽中失焦）不再把指针夹在视口里 |

手工复现脚本（P0 缺陷验收，逐条可执行）：
1. 编辑器里进 Play（默认 Captured，`SceneEditingViewportClient.cpp:401-418` 路径生效）。
2. Alt+Tab 到记事本：预期**能**把鼠标移到记事本任意位置（**修复前：鼠标被夹在原视口矩形内**）。
3. 在记事本窗口内按住左键拖选一段文字 → 能选中（修复前不可能）。
4. 点回编辑器窗口（不要点视口）：预期光标立刻回到 Captured（隐藏+约束），移动只影响游戏视角。
5. 再 Alt+Tab 出去 → 再次自由。重复 10 次不出现"残留夹住"。

## Deferred
- 多窗口 / 多视口并发捕获（A1）。
- 完整键位重映射框架（当前只落一个 `ReleaseKey` 设置项；通用重映射仍属 `ED-F10`）。
- 暂停态（`PlayState` 无 `Paused`，若未来引入则由状态机加两个转移即可）。
- 光标 warp-on-release 的高级策略（见决策 D-6，先做"warp 到视口中心"）。
- `GLFW_CURSOR_CAPTURED` 若探针可用则替换 `ClipCursor` 路径（决策 D-4 的后续）。
- 非 Win32 的 focus 作用域实现（当前平台层降级为"只切 GLFW 模式"，但 focus 撤销仍应生效）。
- 指针位置的历史轨迹 / 平滑（消费方需求明确后再谈）。
- 手柄/触屏的"虚拟指针"（不属于光标策略）。

## 待主人审批的决策清单（R2 后状态）

| # | 决策 | 推荐 | 状态 | 推翻条件 |
|---|------|------|------|----------|
| **D-1** | 模式档位粒度：5 档（Godot 全集）/ 3 档 | **(a) 5 档** | **已批准** | 消费方确认光标栈走 Confined 即可 → 退 3 档 |
| **D-2** | Esc 语义与释放键 | ~~Esc=Stop、Alt=Released~~ → **主人裁定：Esc=Stop 不变；释放改绑 Shift+F1（LeftAlt 兼容别名默认开）** | **已裁定，覆盖原推荐** | 若 Shift+F1 与既有快捷键冲突 → 换键（同设置项） |
| **D-3** | tier 序 Editor100 > Play80 > UI60 > Game40 > Default0 | **(a)** | **已批准** | 出现过场强制锁定 → 加 `GameOverride=90` |
| **D-4** | `Confined` 底层：Win32 `ClipCursor` / `GLFW_CURSOR_CAPTURED` | **(a) ClipCursor**，(b) 作探针通过后的优化 | **已批准** | 探针证明 `CAPTURED` 生效且行为等价 → 切 (b) |
| **D-5** | 参考像素映射归属：留 `UISystem` / 下沉 `Platform` | **(a) 留 UISystem** | **已批准** | 出现非 UI 的第二套参考空间 → 引入中间层 |
| **D-6** | 退出 `Captured` 是否 warp 到视口中心 | **(a) warp** | **已批准** | 与 ImGui 拖拽 / 多显示器 DPI 冲突 → 不 warp |
| **D-7** | `WindowSystem` 加 focus 回调 + 诊断读口 | **(a) 加** | **已批准，并升级为 P0 必需品**（P7 的唯一正确挂点；禁止 ImGui 兜底） | 无（不加则 P7 无法根治） |
| **D-8** | 新增 `editor.get_pointer_policy` automation op | **(a) 加** | **已批准** | 审批收紧 automation 面 → 退 (b)，O3/O8/O9/O10 降为手工 |
| **D-9** | `ViewportPointerMode` 去留：删除 / 保留 | **(a) 删除** | **已批准** | 视口需要"策略之外"的提示态 → 保留但只作 UI 提示 |

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-10-19 | 初版 Design（工作项 B，S00）；复核任务书 §4 现状事实并列出 5 处差异 |
| 2026-10-20 | **R2 修订**（主人裁决）：①新增 **P7 系统级约束泄漏（P0）** 与 file:line 证据链；②不变量新增 **I6a/I6b/I6c/I6d**（意图态 vs 施加态双条件 + 收敛时限 + 重放）与 I10/I11；③新增并**前置** S01 切片"focus 作用域约束"，给出独立验收与手工复现脚本；④focus 事件来源落到 `SetupWindowEventCallbacks`（`GLFWWindowSystem.cpp:408-415`）与自定义 WndProc（`:33-66`），明确禁止 ImGui 兜底；⑤释放键 **LeftAlt → Shift+F1**（Esc 保持 Stop），补 §键位变更与可见面同步清单；⑥Failure Model 增"约束泄漏"独立失效模式并区分"进程退出"与"失焦"；⑦Verification 增 **O8/O9/O10** 与 3 条焦点用例；⑧Slices 重排（S01 focus → S02 骨架 → …）；⑨决策清单标记 R2 状态（D-2 被裁定覆盖） |

## 自检：覆盖核对

- [x] ① 对标现成设计 → **模式与三方对照**（UE/Godot 三方表 + 取舍 + 明确"不做什么"5 条 + "两者都不提供的本仓必须自己保证"一条）
- [x] ② 所有权与仲裁 → **Proposed Architecture** / **Ownership & Lifetime**（销毁点精确到 `Engine.cpp:540-552` 之间）/ **State & Invariants**
- [x] ③ 游戏面 API → **API · 游戏面 facade**（签名契约 + 非作者化规则 + 失焦期间只入栈）
- [x] ④ 实时指针位置 → **API · 位置契约**（四种坐标 + 两个有效性位 + 更新时机 + I7/I8）
- [x] ⑤ PIE 生命周期 → **PIE 生命周期状态机**（Esc=Stop / Shift+F1=Released / 失焦立即撤销 / Stop / Shutdown / 崩溃 / 重放）
- [x] ⑥ 与 UI 的关系 → **与 UISystem 的分工**（三条硬边界 + 焦点不归 ImGui + 容器 → UI tier `Confined`）
- [x] ⑦ 可测试 → **Verification**（O1–O10 + 11 条用例 + 可执行命令 + 6 步手工点验）
- [x] ⑧ **R2 P0 追加：系统级约束泄漏** → **P7** + 要求 1–6 + **I6a/I6b/I6c/I6d** + Failure Model 首行 + **O8/O9/O10** + **S01 独立验收**
