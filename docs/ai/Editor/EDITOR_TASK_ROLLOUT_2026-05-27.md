# Editor 任务推进安排（2026-05-27）

Last updated: 2026-05-27  
Status: **rolling**（S3 M3 Done；S4 M4 Done；新增 **S4.1 稳定性修订**）  
父文档：[`PLATFORM_ROADMAP.md`](../Platform/PLATFORM_ROADMAP.md) §10

---

## 0) 定位

推进安排文档；设计细节见 [`EDITOR_CONTEXT_MENU_DESIGN.md`](./EDITOR_CONTEXT_MENU_DESIGN.md) **§6.1、§15.4**。

---

## 1) 今日主线（可增量）

### A. P7 / E1 右键菜单系统（优先）

- 交付切片：**M0 ✅ → M1 ✅ → M2 ✅ → M3 ✅ → M4 ✅ → M4.1 稳定性修订 → M5 快捷键**
- **M1 CB 菜单项（收口）：** Delete、Import…、Refresh  
- **暂缓（不注册 / 代码待删）：** RevealInExplorer、Rename — 见设计 §15.4

#### A 子任务勾选

| 子项 | 状态 |
|------|------|
| M0 骨架 + `GetContextMenu()` | [x] |
| M1 Context + CB 右键接入 | [x] |
| M1 Delete / Import / Refresh | [x] |
| M1 代码清理：移除 Reveal + Rename 临时实现 | [x] |
| M2 Hierarchy + Inspector | [x] |
| M3 移除 Tools FileDialog | [x] |
| M4 Provider + Section 子菜单 | [x] |
| M4.1 稳定性修订（崩溃/范围收口） | [ ] |

### B. CB Tile Icon | ### C. Material Editor

（未变，见原 rollout §1）

---

## 2) 执行切片

| Slice | 内容 | 状态 |
|-------|------|------|
| S0 | M0 骨架 | **Done** |
| S1 | M1 CB（Delete/Import/Refresh） | **Done**（收口） |
| S1b | 删除 Reveal/Rename 临时代码 | **Done** |
| S2 | M2 Hierarchy / Inspector | **Done** |
| S3 | M3 Tools FileDialog | **Done** |
| S4 | M4 Provider + Section 子菜单 | **Done** |
| S4.1 | M4 稳定性修订（Section 策略 + 作用域收口） | **Planned** |

---

## 4) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-05-27 | 初版；S0；方案 A；S1 |
| 2026-05-27 | **M1 收口**：Reveal/Rename 暂缓；S1b 代码清理；对齐设计 §15.4 |
| 2026-05-27 | **M2**：Hierarchy/Inspector 右键；`EditorEditActions` 统一 Delete；Duplicate/Focus 占位 |
| 2026-05-27 | **M4**：`SceneAddComponentMenuProvider`；Section `BeginMenu`；Create Empty / Remove Component |
| 2026-05-27 | **S4.1 计划**：修复 Edit 悬停崩溃；仅 Create 使用折叠子菜单；Hierarchy GO 菜单移除 Add Component；Inspector 右键临时下线 |
