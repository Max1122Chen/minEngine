# ED-F02 — Implementation Plan

## Meta
- **ID:** `ED-F02`
- **Status:** Draft
- **Owner:** project maintainer
- **Last updated:** 2026-09-01
- **Branch:** `feat/editor`
- **Related:** [Design](./ED-F02_EDITOR_WORKFLOW_DESIGN.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)

## TL;DR

在既有 Scene/Material 文档能力上 **接线 + 编排**：S00 打通 Content Browser 双击；S01–S02 满足 merge 检查点；S03–S05 体验修复可并行或 merge 后收尾。每切片独立可验证、可提交。

## Scope
- **In:** Design §Scope In；切片 S00–S05。
- **Out:** Component 图标、全资产类型创建、Play Mode、资产重命名引用更新。

## Reader quick start
1. [Design](./ED-F02_EDITOR_WORKFLOW_DESIGN.md) — 模块边界与 dirty 策略。
2. 下表 — 推荐实施顺序。
3. 开干前：`git checkout feat/editor && git merge master`（若落后）。

---

## 1) 切片总览

| Slice ID | 内容 | 优先级 | 状态 | 验证 |
|----------|------|--------|------|------|
| `ED-F02-S00` | Content Browser 双击 → `TryOpenAsset` | 高 | Planned | 双击 `test.mescene` / `.mematerial` 进入对应模式 |
| `ED-F02-S01` | Scene `OpenAsset` + File Open + dirty 对话框 | 高 | Planned | 打开、切换、Cancel 保留 dirty |
| `ED-F02-S02` | `CreateAsset` Scene/Material + New/Create UI | 高 | Planned | 创建文件落盘、registry、可打开 |
| `ED-F02-S03` | Material Preview SkyBox | 中 | Planned | Material 视口可见天空 |
| `ED-F02-S04` | Scene 视口局部 RMB 鼠标捕获 | 中 | Planned | RMB 释放恢复；面板可点 |
| `ED-F02-S05` | Abstract Component 过滤 | 低 | Planned | 下拉无 `PrimitiveComponent` 等 |

**Merge gate（ACTIVE_WORK）：** S01 + S02 Done → 可与 `master` 内核一并 merge。

---

## 2) 切片详情

### ED-F02-S00 — Content Browser 打开接线

- **Goal:** 双击资产调用统一打开 API（Material 先可用；Scene 依赖 S01 的 `OpenAsset` 实现，可在 S00 合入路由、S01 完成 Scene 路径）。
- **Touch:**
  - `ContentBrowserWindow::ActivateAssetFromBrowser` → `m_Context.GetAssetWorkflow().TryOpenAsset(meta)`
  - `AssetWorkflowModule`：新增 `TryOpenAsset`（首版可直接转调 `OpenAsset`；S01 加入 dirty）
- **DoD:**
  - [ ] 双击 Material 打开 Material Editor 并加载资产
  - [ ] 双击 Scene 在 S01 完成后可加载；S00 至少不再仅 log
  - [ ] 打开失败有 `ME_CORE_WARN` 可读信息
- **Verify:** Editor GL `--project …`；Content Browser 双击 `test.mescene`、任一 `.mematerial`

---

### ED-F02-S01 — 打开 Scene 与 dirty 确认

- **Goal:** File Open Scene、Scene `OpenAsset`、切换前确认。
- **Touch:**
  - `SceneEditor::CanOpenAsset` / `OpenAsset` / `OpenSceneByPath`
  - `AssetWorkflowModule::TryOpenAsset`、`TryOpenSceneByPath`、`ConfirmDiscardIfDirty`
  - `EditorUnsavedChangesDialog`（或等价 ImGui modal）
  - `MainMenuWindow::DrawFileMenu` — Open Scene
  - `Editor::RequestExit` — exit 前确认（若 dirty）
- **DoD:**
  - [ ] File → Open Scene 选择 `.mescene` 并加载
  - [ ] 有 dirty 时切换：Save / Don't Save / Cancel 行为符合 Design §3.3
  - [ ] 加载成功：CommandStack 清空、dirty 清除、标题更新
  - [ ] `SceneEditor::OpenAsset` 不再返回 false（合法 Scene meta）
- **Verify:** 修改 Transform → dirty → Open 另一 Scene → 三选项；`verify.ps1`

---

### ED-F02-S02 — 创建 Scene / Material

- **Goal:** Runtime 创建 API + Editor 入口。
- **Touch:**
  - `AssetManager::CreateAsset<Scene>`、`CreateAsset<Material>` 特化（`.cpp`）
  - `AssetWorkflowModule::TryCreateScene`、`TryCreateMaterial`
  - `MainMenuWindow` — New Scene
  - `ContentBrowserBuiltInActions` 或新 Provider — Create Scene / Material
  - Scene **Save As** 最小路径（New Scene 后落盘）
- **DoD:**
  - [ ] 创建后磁盘有 `.mescene`/`.mematerial` + `.meta`
  - [ ] Scene 在 `SceneManager` 已注册，Save 成功
  - [ ] Material 创建后可打开编辑
  - [ ] Content Browser 刷新可见新资产
- **Verify:** 新建 Scene → 加 GameObject → Save → 重开；新建 Material → 保存 → 重开；`verify.ps1`

---

### ED-F02-S03 — Material Preview SkyBox

- **Goal:** Material 编辑器预览与 Scene 视口一样有天空。
- **Touch:**
  - `PreviewScene::BuildDefaultSphereScene` — 添加 `SkyBoxComponent`（资源 GUID 与 EngineDefault 对齐）
  - `MaterialEditorViewportClient::EndFrame` — `SceneDrawFlags::EnableSkyBox | EnablePostProcess`
- **DoD:**
  - [ ] Material Editor 视口可见 SkyBox（非纯黑/灰背景）
  - [ ] Scene Editor 视口无回归
- **Verify:** 人工目视 Material Editor GL；`verify.ps1`

---

### ED-F02-S04 — Viewport 局部鼠标捕获

- **Goal:** RMB 导航仅作用于聚焦中的 Scene 视口。
- **Touch:**
  - `SceneEditingViewportClient::InputKeys`、`SetNavigating`、`ExecuteInputCommands`
  - 视口 `ViewportFrameState`（`IsFocused` / `IsHovered`）与 ImGui `WantCaptureMouse`
- **DoD:**
  - [ ] RMB 在视口内按下：进入导航、光标隐藏
  - [ ] RMB 释放或指针离开视口：退出导航、光标恢复
  - [ ] 在 Inspector / Content Browser 上 RMB 不隐藏全局光标
- **Verify:** 人工：RMB drag 视口 → 释放在面板上 → 光标正常

---

### ED-F02-S05 — Abstract Component 过滤

- **Goal:** 仅列出可实例化 Component。
- **Touch:**
  - 相关 Component 头文件 — `ME_CLASS(Abstract)` 标注
  - `SceneEditor::InitializeComponentTypeNames`
  - `SceneContextMenuProviders` Add Component 列表
- **DoD:**
  - [ ] Inspector Add Component 无抽象基类
  - [ ] 右键 Add Component 同步
  - [ ] 具体组件（`StaticMeshComponent` 等）仍可添加
- **Verify:** 人工 Inspector 下拉；`verify.ps1`

---

## 3) 建议 PR / 提交顺序

```text
1. S00（可单独小 PR）
2. S01 + S01 所需 dialog 基础
3. S02（含 Save As 最小版）
--- merge checkpoint 可在此 ---
4. S03 / S04 / S05（各可独立）
```

---

## 4) 测试与记录

| 检查 | 命令 / 步骤 |
|------|-------------|
| Smoke | `.\scripts\verify.ps1` |
| Editor | `Editor.exe --rhi opengl --project <path.meproject>` |
| 记录 | 每切片完成后 `PROGRESS_LOG.md` 一条 |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-01 | 初版 Implementation Plan（S00–S05） |
