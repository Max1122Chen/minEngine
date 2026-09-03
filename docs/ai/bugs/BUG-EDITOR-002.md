# BUG-EDITOR-002 — Editor 冷启动偶发崩溃（DebugDrawService / Physics debug 入队路径）

## Meta
- **ID:** BUG-EDITOR-002
- **Status:** Open（**Blocked / 根因未闭合**；Editor 已默认关 DebugDraw 绕过）
- **Severity:** S1
- **Owner:** project maintainer
- **Found:** 2026-09-02
- **Resolved:** —
- **Last updated:** 2026-09-03
- **Affects:** Editor 冷启动首帧～数秒；OpenGL / Vulkan 均曾高概率（`0xC0000005`）
- **Related Feature/Slice:** `PhysicsDebugDraw`, `DebugDrawService`, `DebugDrawPass`, `SceneEditingViewportClient`

## TL;DR
冷启动偶发 `0xC0000005`，故障点稳定在 `DebugDrawService::EnqueueBox`，读地址常为 `0xFFFFFFFFFFFFFFFF`（`m_Boxes` / vector 控制块已坏）。

| 实验 | 结果 | 结论 |
|------|------|------|
| **二分 A** 跳过 UI font atlas rebuild | 仍崩 | **atlas 不是主因**（日志末行 atlas rebuilt 为时序巧合） |
| **二分 B** 关闭 Physics debug 入队 | 20 次 0 崩 | **触发路径 = DebugDraw 入队** |
| **S05** 入队移至 `ForwardRenderer::Execute` | 栈变为渲染 Tick，**仍崩** | 生命周期对齐必要但不充分 |
| **ASan**（LLVM-MinGW clang++） | 多次冷启动过 atlas **不崩、无 ASan 报告** | 分配器/布局可能**掩盖**踩内存；**不能**据此判 Resolved |

**现状：** 根因仍是 DebugDrawService 入队时内存已损坏；真正写坏者未找到。**Editor 已默认关闭 `EnableDebugDraw`**（等同二分 B）；正式修复前保持 Open。

---

## 症状
- Editor 冷启动，场景加载成功后数秒内偶发无提示崩溃。
- 退出码：`-1073741819`（`0xC0000005`）。
- 日志末行常为 `EditorAppearance: UI font atlas rebuilt`（**误导**——二分 A 已排除）。

## 期望
- 冷启动稳定；Physics collider debug 可开且不 AV。

## 复现（GCC Debug，`minEngine/bin`）
```powershell
cd minEngine\bin
.\Editor.exe --rhi opengl --project <MyMEProject.meproject>
# 多次冷启动；历史约 30%（2026-09-02）
```

## 环境
- OS: Windows 10/11
- **易复现：** MinGW-Builds GCC 15 Debug（`minEngine/build` → `bin/`）
- **难复现：** ASan / clang++（`minEngine/build-asan` → `bin-asan/`）
- 项目：`MyMEProject`，默认场景 `test`

---

## 根因调查（截至 2026-09-03）

### 崩溃点（符号化）
`DebugDraw::Box` → `DebugDrawService::EnqueueBox`（`DebugDraw.cpp`）

### 调用栈演变

**S04 / 二分期（ImGui EndFrame 入队）：**
```
EditorViewportWindow::OnDraw → SceneEditingViewportClient::EndFrame
  → PhysicsDebugDraw::SubmitScene → DebugDraw::Box → EnqueueBox
```

**S05 落地后（仍崩，栈证明入队已挪到渲染）：**
```
Editor::Run → TickRendererFrame → RenderSystem::Tick
  → ForwardRenderer::Execute
    → PhysicsDebugDraw::SubmitScene → … → EnqueueBox
```

### 机制判断（更新）

1. **已确认：** 只要走 Physics collider → `EnqueueBox`，GCC Debug 冷启动可 AV；关掉入队则稳定。
2. **已确认：** 损坏模式是 vector/`this` 读 `0xFFFFFFFFFFFFFFFF`，不是 collider 数值算错。
3. **S04 不足：** 仅同条件门控 + 帧头 `ClearFrameQueues` 无法根治。
4. **S05 不足：** 入队与 `DebugDrawPass` 同 tick 后仍崩 → 更像 **入队前单例/堆已被踩**，而非单纯「入队/消费跨阶段」。
5. **ASan 未抓到：** 多次跑过 atlas 且无 `ERROR: AddressSanitizer`；高度怀疑 ASan quarantine / 布局改变导致原踩踏不再命中 `DebugDrawService`（**假阴性可能**）。

### 已排除
- UI font atlas rebuild 主因（二分 A）
- efsw 直接写 `DebugDrawService`
- 显卡驱动主因（栈在引擎）

### 未闭合
- 谁在首帧前写坏 `DebugDrawService` 单例 / `m_Boxes` 控制块
- 为何 clang ASan 布局下不触发、GCC 下高概率触发

---

## 已尝试的修复 / 基础设施

| Slice / 项 | 内容 | 效果 |
|------------|------|------|
| **S01/S02** | atlas invalidate、`ed_crash.log` | 诊断用，保留 |
| **S04/S04b** | 门控 + 帧头 Clear + eager-init；撤销过度 RT 守卫 | 曾误标 Resolved；**复发** |
| **S05** | `GameplayScene`；`Execute` 内入队；去掉 Editor 帧头 Clear | 栈正确，**仍崩** |
| **ASan 工程** | `MINENGINE_ENABLE_ASAN`；`scripts/debug/configure_asan.ps1` / `build_asan.ps1` / `run_editor_asan.ps1`；LLVM-MinGW；`--export-all-symbols`；efsw `char_traits` shim | 可构建运行；**未复现 / 无报告** |

### S05 目标生命周期（保留，正确方向）
```
EndFrame: SubmitSceneDraw(drawDesc)  // GameplayScene 已填

ForwardRenderer::Execute:
  ClearFrameQueues()
  PhysicsDebugDraw::SubmitScene(GameplayScene)
  … scene graph …
  DebugDrawPass::Prepare → BuildFrameGeometry → ClearFrameQueues
```

---

## 临时绕过（已合入，开发优先）

与二分 B 等价，**不修根因**，仅避免触发：

- **已落地：** `SceneEditingViewportClient::EndFrame` 不再置 `SceneDrawFlags::EnableDebugDraw`（注释指向本 bug）。
- 因此 `ForwardRenderer::Execute` 不会调用 `PhysicsDebugDraw::SubmitScene`。

预期：冷启动稳定；viewport **无** collider 线框。  
正式修复后应恢复 `| SceneDrawFlags::EnableDebugDraw`，并做冷启动压测。

---

## 实施状态
- [x] 符号化确认 `EnqueueBox`
- [x] 二分 A / B
- [x] S05 生命周期对齐（代码在树；**未过回归**）
- [x] ASan 构建链路（`bin-asan`）；多次冷启动无报告
- [x] 临时关 DebugDraw 合入（workaround；根因仍 Open）
- [ ] 找到真正写坏内存的代码
- [ ] GCC Debug 冷启动 20+ 次无 AV（含 DebugDraw 开启）

## 下一步建议（按优先级）
1. **根因：** GCC Debug 下对 `DebugDrawService` 单例 / `m_Boxes` 设 GDB watchpoint；或 MSVC ASan（若安装组件）；对比 clang vs GCC 堆布局差异。
2. **勿仅依赖** 当前 LLVM-MinGW ASan 结果判定已修复。
3. **修复后：** 恢复 `EnableDebugDraw`，冷启动压测 20+ 次后再标 Resolved。

## 关联
- [BUG-EDITOR-001](./BUG-EDITOR-001.md)
- [手动调试指南](./DEBUG_EDITOR_MANUAL.md)
- ASan 脚本：`scripts/debug/configure_asan.ps1`、`build_asan.ps1`、`run_editor_asan.ps1`

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-02 | 登记；初判 ImGui atlas |
| 2026-09-02 | 符号化 `EnqueueBox`；S04（曾标 Resolved，后复发） |
| 2026-09-03 | 二分 A 否定 atlas；二分 B 确认入队路径；S05 落地仍崩 |
| 2026-09-03 | ASan（LLVM-MinGW）可跑、多次不复现无报告；**保持 Open**；允许临时关 DebugDraw 推进开发 |
| 2026-09-03 | 合入 workaround：Editor viewport 默认不置 `EnableDebugDraw` |
