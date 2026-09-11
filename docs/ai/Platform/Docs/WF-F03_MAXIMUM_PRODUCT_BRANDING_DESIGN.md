# WF-F03 — Maximum Product Branding (0.1.0 display name) — Design Spec

## Meta
- **ID:** `WF-F03`
- **Type:** Feature
- **Status:** Planned（Phase F3；地基短切片）
- **Owner:** project maintainer
- **Last updated:** 2026-09-11
- **Related:** [ENGINE_0_1_0_ROADMAP.md](../../ENGINE_0_1_0_ROADMAP.md) Phase F3
- **Depends on:** 无硬依赖；建议 F1/F2 之后或穿插，同一 Foundation 合入波
- **Blocks:** 无；服务 D8 / 对外称呼 Maximum 0.1.0

## TL;DR

编辑器**产品显示名**与版本展示改为 **Maximum**（及 0.1.0 版本字符串）；**不**要求重命名仓库或 `Editor.exe` 路径（可另议产物文件名）。

## Scope

### In
- 窗口标题 / About / 启动日志中的产品名
- 版本号常量或从 CMake/`ME_VERSION` 注入（若已有则接线）
- 文档与 Roadmap 用语对齐

### Out
- 强制重命名 git 仓库、目录 `minEngine` → `Maximum`
- 完整安装程序 / 商店 listing

## Reader quick start

1. 本文件  
2. 代码：Editor 窗口创建、About（待定位）  
3. CMake 目标名是否改显示名（可选）

## 6) 验收（预告）

- [ ] 运行 Editor 可见 Maximum（或 Maximum 0.1.0）展示  
- [ ] D8 相关说明写入 Progress  

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-11 | Planned stub |
