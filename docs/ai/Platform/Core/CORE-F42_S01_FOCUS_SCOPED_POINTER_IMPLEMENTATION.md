# CORE-F42 · S01 —— focus 作用域指针约束（实现文档）

| 项 | 值 |
|----|----|
| 切片 | `S01`（P0 · 最早落地）：focus 作用域约束 |
| 设计输入 | `docs/ai/Platform/Core/CORE-F42_POINTER_POLICY_DESIGN.md`（**已批准，本次未改动**） |
| 任务书 | `minCraft-ui/docs/ai/INFRA/ENGINE_SESSION_PROMPT_UI_UV_AND_CURSOR_POLICY.md` §4 |
| 落地日期 | 2026-10-20 |
| 验收 | `pointer-policy` 套件 5 用例 / 72 断言全绿；`screen-ui-pointer` / `screen-ui-drag` / `gameplay-input-gate` / `begin-play` / `play-main-camera` / `input-key-catalog` 无回归；`Editor/**` 零改动 |

## 1. 一句话结论

光标约束（ClipCursor / raw input / 隐藏 / captured）现在**只在"本窗口 active"期间存在**：

- 失焦 → **在 focus 回调里当帧撤销**（不是下一帧、不等 Stop），撤销**不经过任何缓存标志**；
- 回焦 → 按**当时在用的**约束请求重放（不是"重置成 Free"，也不是回放历史）；
- 施加态读口用 `::GetClipCursor` **回读 OS**，非 Win32 明确返回"不可观测"；
- focus 回调万一漏掉，**Watchdog 每帧用平台真值兜底撤销 + warn**。

主人原话里的 P0 痛点在结构上消失：`Alt+Tab` 出去后指针立刻可自由移动到桌面任意位置/其它应用。

## 2. 根因（三条，逐条对应到修复点）

| # | 根因 | 证据（修复前） | 修复点 |
|---|------|----------------|--------|
| 1 | 引擎侧**没有** focus/activate 事件源 | `glfwSetWindowFocusCallback` / `WM_ACTIVATE` 在 `minEngine/src` 零命中；`SetupWindowEventCallbacks` 只注册 5 个回调 | 注册 focus 回调（`GLFWWindowSystem.cpp:570`）+ 自定义 WndProc 的 `WM_ACTIVATE` 兜底（`:63-72`） |
| 2 | 撤销被**缓存标志把门**：OS 已施加但 `m_bCursorClipActive==false` 时，"撤销"被静默跳过 | 旧 `SetCursorClipRect`：`if (m_bCursorClipActive) { ::ClipCursor(nullptr); ... }` | `PointerConstraintState::RevokeAll()`（`PointerConstraintState.cpp:240-252`）**无条件**调 `::ClipCursor(nullptr)`；`m_bCursorClipActive` 已删除，施加态改为 OS 回读 |
| 3 | 施加态与意图态不分 | 只有缓存标志自证 | 新增 `WindowCursorConstraintState`（`WindowSystem.h:35-46`）+ `PointerConstraintOs::ReadAppliedClipState()`（`PointerConstraintState.cpp:82`，内部 `::GetClipCursor`） |

## 3. 结构（三个落点，都是平台层）

```text
[平台事件] GLFW focus 回调 (GLFWWindowSystem.cpp:85) ──┐
            WM_ACTIVATE/WA_INACTIVE (WndProc :65)     ──┼──► GLFWWindowSystem::OnWindowFocusChanged (:338)
                                                        │            │
                                                        │            └──► PointerConstraintState::OnWindowFocusChanged
                                                        │                   失焦 → RevokeAll()（立即、无条件）
                                                        │                   回焦 → ApplyCurrentRequest()（重放当前请求）
[每帧] GLFWWindowSystem::PollEvents (:278) ──► glfwGetWindowAttrib(GLFW_FOCUSED) ──► SyncPlatformFocus
                                                        （平台真值；与内部状态不一致 → 强撤 + warn）
编辑器既有调用点（未改）：SetCursorVisible / SetCursorClipRect ──► 记录请求 + （若 active）施加
```

- 新增文件：`PointerConstraintState.h` / `.cpp`（Render 目录，CMake `GLOB_RECURSE src/*.cpp` 自动纳入，无需登记）。
- `GLFWWindowSystem` 通过私有继承 `ICursorConstraintSink` 把四个动作落到底层：`ApplyOsClip`（`GLFWWindowSystem.cpp:385`）、`ApplyCursorMode`、`ApplyRelativeCapture`、`GetClientScreenRect`（客户区屏幕矩形，供 I11 求交）。

## 4. 关键实现

### 4.1 撤销路径（无条件，重点）

```cpp
// PointerConstraintState.cpp:240
void PointerConstraintState::RevokeAll()
{
    WindowCursorClipRect released;
    // Unconditional: the release must never be gated by any cached "we applied it" flag (CORE-F42 P7).
    m_Sink.ApplyOsClip(released);
    m_Sink.ApplyRelativeCapture(false);
    m_Sink.ApplyCursorMode(EWindowCursorMode::Normal);
}

// PointerConstraintState.cpp:40  -> 真的调 OS，且释放分支不读任何成员
bool PointerConstraintOs::ApplyOsClipRect(const WindowCursorClipRect& rect)
{
#if defined(_WIN32)
    if (!rect.bEnabled)
    {
        // Unconditional: a cached "already released" flag must never suppress the release (CORE-F42 P7).
        return ::ClipCursor(nullptr) != FALSE;
    }
    RECT osRect{ rect.Left, rect.Top, rect.Right, rect.Bottom };
    return ::ClipCursor(&osRect) != FALSE;
#endif
}
```

失焦分支（`PointerConstraintState.cpp:152-166`）**先撤后记**，全部在回调内完成，因此天然满足 I6c（当帧收敛）：

```cpp
if (!focused)
{
    m_bWindowActive = false;
    RevokeAll();          // 立即路径：当帧撤销四因子
    ++m_FocusRevokeCount;
    return;
}
m_bWindowActive = true;
ApplyCurrentRequest();    // I6d：按"当时在用的请求"重放
```

### 4.2 施加态读口（回读 OS，不自证）

```cpp
// PointerConstraintState.cpp:82
bool PointerConstraintOs::ReadAppliedClipState(bool& bOutActive, WindowCursorClipRect& outRect)
{
    bOutActive = false;
    WindowCursorClipRect osClipRect;
    if (!ReadOsClipRect(osClipRect))        // -> ::GetClipCursor（:65）
    {
        return false;                       // 平台不可观测
    }

    const WindowCursorClipRect desktop = GetDesktopScreenRect();   // SM_*VIRTUALSCREEN
    bOutActive = IsValidRect(desktop) && !RectContains(osClipRect, desktop);
    outRect = osClipRect;
    outRect.bEnabled = bOutActive;
    return true;
}
```

`GLFWWindowSystem::GetCursorConstraintState()`（`GLFWWindowSystem.cpp:300-336`）拼出四因子：

| 字段 | 来源 |
|------|------|
| `bClipActive` / `ClipRect` | `::GetClipCursor` 回读（与整块桌面比较，能覆盖桌面 == 无约束） |
| `bObservable` | 回读是否成功；非 Win32 保持 `false`（**明确"不可观测"，不假装 Free**） |
| `NativeCursorMode` | `glfwGetInputMode(GLFW_CURSOR)`（平台真值，非我们写的缓存） |
| `bRawCaptureActive` | `m_bRawMouseRegistered`（Raw Input 注册是否真的成功过） |
| `bWindowActive` | 状态机记录的 focus 真值 |

### 4.3 focus 事件源与安装顺序（为什么不会吞掉 ImGui 的 focus 回调）

安装顺序（实际执行序）：

1. `GLFWWindowSystem::Initialize()` → `SetupWindowEventCallbacks()`（`GLFWWindowSystem.cpp:561-575`）：先装自己的 5 个回调，再
   `s_PreviousWindowFocusCallback = glfwSetWindowFocusCallback(m_Window, WindowFocusCallback);`
   —— `glfwSetWindowFocusCallback` 的返回值就是**上一个**回调指针，我们保存它。
2. 若 ImGui 的 GLFW 后端更晚初始化（编辑器就是这种顺序），它会再次调用 `glfwSetWindowFocusCallback`，并把它拿到的"上一个"（=我们的 `WindowFocusCallback`）存为自己的 `prev`，转发给我们。
3. 两种顺序都成立，因为**两边都做了链式**：
   - 我们晚装：我们保存 ImGui 的回调，处理完再调用它（`GLFWWindowSystem.cpp:96-101`）；
   - 我们早装：ImGui 保存我们的回调，处理完再调我们。
4. 退出时（`Shutdown`, `:216-220`）把先前回调装回去，并把指针清空 —— 不留悬挂指针。

因此 "ImGui 的 focus 处理被静默吞掉" 不会发生；同时本功能**不读** `ImGui::GetIO().AppFocusLost`（要求 3），平台层是唯一来源。

Win32 兜底：自定义 WndProc 只在 Raw Input 注册期间存在，而 Raw Input 恰好只在"有约束施加"时注册，所以 `WM_ACTIVATE`/`WM_ACTIVATEAPP` 的失活分支正好覆盖"泄漏风险窗口"（`GLFWWindowSystem.cpp:63-72`），且处理完仍原样转发给 `s_PreviousWndProc`（GLFW 的 WndProc → 它自己的 focus 回调 → 再回到我们的 `WindowFocusCallback`；幂等，不重复撤销）。

### 4.4 Watchdog（帧序）

`GLFWWindowSystem::PollEvents()`（`GLFWWindowSystem.cpp:278-286`）在 `glfwPollEvents()` 之后：

```cpp
const bool bPlatformActive = m_Window != nullptr && glfwGetWindowAttrib(m_Window, GLFW_FOCUSED) == GLFW_TRUE;
m_PointerConstraint.SyncPlatformFocus(bPlatformActive);
```

`SyncPlatformFocus`（`PointerConstraintState.cpp:168-188`）：平台说"未 active"而内部状态说"active" → 立即 `RevokeAll()` + `++WatchdogRevokeCount` + 一条 `LogPlatform/Warn`。这保证"focus 事件漏了/没注册"也不会长期泄漏（要求 6 / 断言 E）。

### 4.5 与不变量的对照

| 不变量 | 落点 |
|--------|------|
| I6a（意图侧挂起） | `OnWindowFocusChanged(false)` 置 `bWindowActive=false`（诊断口 `GetDiagnostics().bWindowActive`），请求保留待重放 |
| I6b（OS 侧清空） | `RevokeAll()` 三连 + `::ClipCursor(nullptr)`；`GetClipCursor` 回读为"覆盖桌面" |
| I6c（当帧收敛） | 撤销在回调内完成，测试在**不推进任何帧**的情况下断言 |
| I6d（重放） | `OnWindowFocusChanged(true)` → `ApplyCurrentRequest()`；测试含"失焦期间改请求，回焦按新请求重放" |
| I10（作用域） | 所有施加点都先过 `if (!m_bWindowActive) { RevokeAll(); return; }` |
| I11（矩形合法性） | `ApplyCurrentRequest` 先与 `GetClientScreenRect()` 求交；交集为空 → 不施加 + `++EmptyIntersectionCount` + warn |
| I5（Stop/Shutdown 归零） | `Shutdown` 第一句 `m_PointerConstraint.ReleaseAllAndForgetRequests()`（`GLFWWindowSystem.cpp:214`），且忘掉请求（回焦不会把 Stop 前的约束带回来） |

## 5. 测试：`pointer-policy`（5 用例 / 72 断言）

位置：`minEngine/Tests/Suites/PointerPolicyTest.cpp`；注册：`TestSuiteRegistration.cpp`（`pointer-policy` / InSmoke=true / InFull=true）。

| 用例 | 断言要点 |
|------|----------|
| `pointer-policy - focus loss releases os clip` | 已施加 → 失焦（同帧，无帧推进）→ `LastAppliedClip.bEnabled==false`、`GetClipCursor` 回读 `bClipActive==false`、`NativeCursorMode==Normal`、`bRawCaptureActive==false`、`FocusRevokeCount==1`；**另加"缓存失同步"腿**：绕过状态机直接在 OS 上施加 clip，再驱动失焦 → 仍被撤销（证明撤销不以缓存为前提） |
| `pointer-policy - watchdog kills clip when inactive` | 完全跳过 focus 回调，只调 `SyncPlatformFocus(false)` → 撤销 + `WatchdogRevokeCount==1`、`FocusRevokeCount==0` + warn |
| `pointer-policy - focus regain replays last request` | 回焦后 `bClipActive==true`（**反例：若为无约束即 FAIL**）、`Disabled`、`bRawCaptureActive==true`、施加矩形与失焦前逐分量相等；子场景：失焦期间改请求 → 回焦按**新**请求重放（不回放历史） |
| `pointer-policy - stop and shutdown leave no clip` | 两条路径分别断言：① 编辑器 Stop 的原始调用序列（`SetCursorClipRect(clear)`+`SetCursorVisible(true)`）；② `ReleaseAllAndForgetRequests()`（= `Shutdown` 路径）且回焦后不复现 |
| `pointer-policy - clip rect intersects client area` | 部分越界 → 施加值 == 请求 ∩ 客户区（同时核对 `GetClipCursor` 回读）；完全越界 → 不施加 + `EmptyIntersectionCount==1` + warn |

通过尾行：

```text
[doctest] test cases:  5 |  5 passed | 0 failed | 228 skipped
[doctest] assertions: 72 | 72 passed | 0 failed |
[doctest] Status: SUCCESS!
[Test] TestRunner: suite 'pointer-policy' PASSED.
```

### 5.1 反向验证（两处临时 mutation，均已还原）

**Mutation A —— 失焦不再撤销**（复现 P0 缺陷本身：`OnWindowFocusChanged(false)` 只置标志不调用 `RevokeAll()`）：

```text
D:\...\Tests\Suites\PointerPolicyTest.cpp:168: ERROR: CHECK( sink.LastAppliedClip.bEnabled == false ) is NOT correct!
  values: CHECK( true == false )
[doctest] test cases:  1 | 0 passed | 1 failed | 232 skipped
[Test] TestRunner: suite 'pointer-policy' FAILED.
```

**Mutation B —— 撤销以 `m_bClipAppliedCache` 为前提**（复现"缓存把门"缺陷）：

```text
D:\...\Tests\Suites\PointerPolicyTest.cpp:179: ERROR: CHECK( sink.ReadState().bClipActive == false ) is NOT correct!
  values: CHECK( true == false )
[doctest] test cases:  1 | 0 passed | 1 failed | 232 skipped
[Test] TestRunner: suite 'pointer-policy' FAILED.
```

还原后重新构建 + 重跑：5/5 通过、72/72 断言通过（上表尾行）。

## 6. 手工点验脚本（5 步，逐条可执行）

1. 启动编辑器（`bin/MaximumEditor.exe`）→ 打开/进入任意场景 → 点视口，按 Play 进入 PIE（默认进入 hide+clip：光标隐藏且被夹在视口内）。
2. **Alt+Tab 切到记事本**（或点任务栏/其它应用窗口）：预期指针**能在桌面任意位置自由移动**，并且**能在记事本正文里按住左键拖选一段文字**（选中蓝底高亮）。
   *修复前*：指针被夹在原视口矩形内，离开编辑器窗口就动不了、记事本拖选不可能。
3. 在记事本里再多拖几次、把指针移到第二屏/任务栏角落 → 全程不被夹住（说明 `::ClipCursor(nullptr)` 已真的执行，而不是"我们以为释放了"）。
4. 点回编辑器窗口标题栏（**不要点视口**）：预期光标立刻回到 Captured（隐藏 + 视觉被约束，移动只影响游戏视角），无需再点视口。
5. 重复 2↔4 共 10 次；每次退出到别的应用都应自由、每次回到编辑器都恢复约束，不出现"残留被夹住"。随后按 `Esc` 结束 Play → 光标可用；关闭编辑器 → 光标在整个桌面自由。

补充（可选回归）：Play 中拖动场景视图（EditNavigate）时 Alt+Tab 出去 → 同样立即自由；回来后再点视口恢复。

## 7. 偏差 / 限制 / 未做

| 项 | 说明 |
|----|------|
| **PE 导出序号上限（重要）** | 引擎 DLL 用 `-Wl,--export-all-symbols`（`minEngine/CMakeLists.txt:143-157`），PE 上限 65535 个导出序号。本切片落地前的构建产物导出数已达 **65537**（**已超限，链接直接报 `export ordinal too large: 65537`**，与本切片无关，但任何重新链接都会撞上）。为让构建转绿，S01 把自身导出面压到 20 个（几何helper 改 TU 内部链接、诊断计数器合成 1 个 `GetDiagnostics()`、focus 回调改文件作用域 static、`WindowSystem::GetCursorConstraintState` 改纯虚），最终 **65533 / 65535**（仅剩 2 个余量）。**建议后续独立任务**改为显式 `.def` 导出表或 `--exclude-symbols`，否则引擎再加任何符号都会再次链接失败。 |
| 独立 Game 的建窗态不参与重放 | `Initialize` 仍 `GLFW_CURSOR_DISABLED`（P4 属于 S03/S05）。若独立 Game 从未发起请求，失焦→回焦后会停在 Free（NORMAL）。这是 S01 "没有请求就没有约束"的一致结果，S05 用游戏面 API 修正。 |
| 非 Win32 | clip 无等价实现 → `GetCursorConstraintState().bObservable == false`（明确"不可观测"）；光标模式与 raw 的 focus 作用域仍生效。 |
| 多显示器下的"无约束"判据 | 以**整个虚拟桌面矩形**为参照：OS 回读矩形覆盖全桌面 == 无约束。单显示器与"Windows 回读主屏"两种情况等价；若未来出现"回读只有主屏"的平台差异，判据需连同读口一起复核（已在读口集中一处，便于改）。 |
| 不做（按任务边界） | 无 tier 栈/引用计数（S02）、无游戏面 `Pointer::*` API（S05）、不迁移编辑器调用点（S03）、不改模式集合与 GLFW 映射（S03）、不加 `editor.get_pointer_policy`（S05）。 |
| 编辑器零改动 | `git status --porcelain | grep Editor` → 空。S01 纯平台层。 |

## 8. 复现命令

```powershell
cmake --build D:\Dev\GitRepo\minEngine\minEngine\build --target minEngineTests -j 8

cd D:\Dev\GitRepo\minEngine\minEngine\bin
.\minEngineTests.exe test pointer-policy
.\minEngineTests.exe test screen-ui-pointer
.\minEngineTests.exe test screen-ui-drag
.\minEngineTests.exe test gameplay-input-gate
```

## 9. 改动清单

| 文件 | 变更 |
|------|------|
| `minEngine/src/Runtime/Function/Render/WindowSystem.h` | 新增 `EWindowCursorMode`、`WindowCursorConstraintState`、纯虚 `GetCursorConstraintState()`、`OnWindowFocusFunc` + `RegisterOnWindowFocusCallback` |
| `minEngine/src/Runtime/Function/Render/PointerConstraintState.h/.cpp`（新） | focus 作用域状态机 + `ICursorConstraintSink` + `PointerConstraintOs`（`ClipCursor` / `GetClipCursor` / 桌面矩形 / 施加态回读） |
| `minEngine/src/Runtime/Function/Render/GLFWWindowSystem.h/.cpp` | focus 回调注册与链式转发、`WM_ACTIVATE` 兜底、`PollEvents` watchdog、`GetCursorConstraintState()` 回读、`SetCursorVisible/SetCursorClipRect` 改为请求记录、`Shutdown` 无条件释放；删除 `m_bCursorClipActive` |
| `Tests/Suites/PointerPolicyTest.h/.cpp`（新） | `pointer-policy` 5 用例（含 RecordingCursorSink：clip 真打到 OS，断言读回读） |
| `Tests/TestSuiteRegistration.cpp` | 注册 `pointer-policy` 套件（3 处：include / traits / register） |
| `minEngine/src/Runtime/Engine.cpp` | **未改**（`ShutdownSystems` 已有 `WindowSystem::Shutdown()` 挂点，足够） |
