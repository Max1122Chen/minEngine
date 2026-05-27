# Editor 任务推进安排（2026-05-27）

Last updated: 2026-05-27  
Status: **rolling**（S1 收口；Reveal/Rename 待代码清理；**S2 待开工**）  
父文档：[`PLATFORM_ROADMAP.md`](../Platform/PLATFORM_ROADMAP.md) §10

---

## 0) 定位

推进安排文档；设计细节见 [`EDITOR_CONTEXT_MENU_DESIGN.md`](./EDITOR_CONTEXT_MENU_DESIGN.md) **§6.1、§15.4**。

---

## 1) 今日主线（可增量）

### A. P7 / E1 右键菜单系统（优先）

- 交付切片：**M0 ✅ → M1 CB 收口 ✅ → M2 Hierarchy/Inspector → M3 Tools FileDialog**
- **M1 CB 菜单项（收口）：** Delete、Import…、Refresh  
- **暂缓（不注册 / 代码待删）：** RevealInExplorer、Rename — 见设计 §15.4

#### A 子任务勾选

| 子项 | 状态 |
|------|------|
| M0 骨架 + `GetContextMenu()` | [x] |
| M1 Context + CB 右键接入 | [x] |
| M1 Delete / Import / Refresh | [x] |
| M1 代码清理：移除 Reveal + Rename 临时实现 | [x] |
| M2 Hierarchy + Inspector | [ ] |
| M3 移除 Tools FileDialog | [ ] |

### B. CB Tile Icon | ### C. Material Editor

（未变，见原 rollout §1）

---

## 2) 执行切片

| Slice | 内容 | 状态 |
|-------|------|------|
| S0 | M0 骨架 | **Done** |
| S1 | M1 CB（Delete/Import/Refresh） | **Done**（收口） |
| S1b | 删除 Reveal/Rename 临时代码 | **Done** |
| S2 | M2 Hierarchy / Inspector | Pending |
| S3 | M3 Tools FileDialog | Pending |

---

## 4) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-05-27 | 初版；S0；方案 A；S1 |
| 2026-05-27 | **M1 收口**：Reveal/Rename 暂缓；S1b 代码清理；对齐设计 §15.4 |
