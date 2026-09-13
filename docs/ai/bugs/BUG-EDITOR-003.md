# BUG-EDITOR-003 — Maximized window: Viewport navigate stutter / high load

## Meta
- **ID:** BUG-EDITOR-003
- **Status:** Verified
- **Severity:** S2
- **Owner:** project maintainer
- **Found:** 2026-09-13
- **Last updated:** 2026-09-13
- **Affects:** 本机短名 `Maximum.exe`；OpenGL + 最大化 Viewport
- **Related Feature/Slice:** WF-F03 `OUTPUT_NAME` → `MaximumEditor`

## TL;DR
最大化卡顿 + 风扇升高，与 **`Maximum.exe` 映像名**耦合（非 About/Title/Log）。  
**修复：** shipping **`MaximumEditor.exe`**（显示名仍 Maximum）。用户验证通过。

## 根因
本机按 `Maximum.exe` 施加的 GPU/OS 侧配置倾向；引擎品牌逻辑非主因。

## 修复
- `OUTPUT_NAME MaximumEditor` + Launcher/脚本/文档对齐
- `ProductBranding::kProductEditorExecutableStem`
- 旁路保留：`UpdateWindowTitle` 未变跳过 SetTitle

## 回归验证
- [x] `Editor.exe` 探针不卡
- [x] **`MaximumEditor.exe` 最大化 + navigate 不卡**（用户 2026-09-13）
- [x] 显示名仍为 Maximum Editor

## 关联
- WF-F03 · [TD-032](../TECH_DEBT.md) · [ED-F09](../Editor/ED-F09_LOG_CONSOLE_RECORD_UI_DESIGN.md)

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-13 | 排查钉 exe 名；shipping → MaximumEditor；Verified |
| 2026-09-13 | 移除临时 worktree `minEngine-bisect-fs` |
