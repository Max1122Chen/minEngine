# BUG-CORE-005 — InputComponent 析构在 InputSystem Shutdown 之后断言

## Meta
- **ID:** BUG-CORE-005
- **Status:** Fixed
- **Severity:** S1
- **Owner:** project maintainer
- **Found:** 2026-10-02
- **Last updated:** 2026-10-02
- **Affects:** Editor / Runtime shutdown；`InputComponent`；含已 Register 的 Scene（minCraft default.mescene）
- **Related Feature/Slice:** Foundation lifecycle；minCraft feedback `ENGINE_FEEDBACK_PROMPT_INPUTCOMPONENT_DTOR_SHUTDOWN.md`

## TL;DR
关 Editor 时 `InputSystem Shutdown` 后 Scene/GO 仍可能晚析构，`InputComponent::~` 无条件 `Get()` → 断言。已加 `HasInstance` 守卫，并收紧 Editor/SceneManager 对 Scene 的强引用释放时机。

---

## 症状
关闭打开了含 `InputComponent` 的 Scene 的 MaximumEditor：

```text
[Core] InputSystem Shutdown
...
[Core] ObjectManager Shutdown.
[Core] Scene 'default' is being destroyed. Cleaning up N game objects.
ME_ASSERT: InputSystem is not initialized
```

## 期望
关进程时 Input/Scene 析构顺序无论先后，不因组件反注册断言崩溃。

## 复现
1. 打开含已 `RegisterInputComponent` 的 Scene（如 minCraft `default.mescene`）
2. （可选）进入过 PIE 再 Stop——`PlayInEditorSession` 曾保留 Scene `shared_ptr`
3. 关闭 Editor

## 环境
- Windows；Debug MaximumEditor；minCraft 反哺 2026-10-02

## 根因
1. **`InputComponent` 析构**无条件 `InputSystem::Get()`（Audio* 已有 `HasInstance` 先例）。
2. **ObjectManager 强引用表**已改为 `weak_ptr`（MEMORY_MANAGEMENT M1）——旧「OM 拖死 GO」叙述过时。
3. **晚于 Input 的 Scene 持有者：**
   - `PlayInEditorSession::m_EditorContext.Scene` 在 Stop 后仍强持有文档 Scene；`Editor::Shutdown` 原先未在 `Engine::Shutdown` 前清空。
   - `SceneManager::UnloadActiveScene` 只 `reset` `m_CurrentActiveScene`，同实例仍可能留在 `m_EditorSceneContext`。
   - Prefab Stage `StageScene` 在 `SceneEditor::Shutdown` 前未丢弃。

## 修复
- `InputComponent` dtor / `RegisterInputComponent`：`HasInstance` 守卫
- `Component::MarkForNeededEndOfFrameUpdate`：同模式守卫（同类风险）
- `UnloadActiveScene`：若 editor context 指向同一 Scene 则一并清空
- `PlayInEditorSession::Stop` 移交后 `m_EditorContext = {}`；新增 `Shutdown()`；`Editor::Shutdown` 在 Engine 前调用
- `PrefabStageController::Shutdown` + `SceneEditor::Shutdown` 释放 Stage Scene

## 回归验证
- [x] `minEngineTests` suite `gameplay-input-gate`：`RunInputComponentDtorAfterInputShutdown`
- [ ] 手动：打开 minCraft default.mescene → 关 Editor，无 InputSystem 断言

## 关联
- [BUG-CORE-004](./BUG-CORE-004.md)（Input gate，不同问题）
- `docs/ai/Platform/MemoryManagement/MEMORY_MANAGEMENT_DESIGN.md`（OM weak 已落地）

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-10-02 | 记录 + 修复落地 |
