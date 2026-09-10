# ED-F06 — Anim SM Canvas Polish — Implementation Plan

## Meta
- **ID:** ED-F06
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-10
- **Related:** [Design Spec](./ED-F06_ANIM_SM_CANVAS_POLISH_DESIGN.md)

## TL;DR
S00 清理 → S01 边右键 → S02 Entry → S03 AnyState 画布 → S04 文档/回归。

## Scope
- **In:** 见 Design
- **Out:** BlendTree；Preview；改 DefaultState 字段名

## 1) 切片总览

| Slice | 内容 | 状态 | 验证 |
|-------|------|------|------|
| S00 | 删 AnimGraphIds；文档交叉链接 | **Done** | 编译 |
| S01 | SmGraph 边 ContextMenu 事件 + Bridge Delete/Reverse | **Done** | 手测 |
| S02 | Entry 节点 Pull/Apply；设 DefaultState | **Done** | Save 往返 |
| S03 | AnyState 节点+边；创建/选中/删边 | **Done** | Inspector |
| S04 | 回归 Material；勾验收；Progress | **Done**（文档）；手测待维护者 | smoke |
| S05 | State 右键 Rename + Delete | **Done** | 手测 |

## 2) Engineering notes
- **Build:** `cmake --build build --target Editor`（源目录 `minEngine/`）
- **ID 约定：** State=`index+1`；Entry=`900001`；AnyState=`900002`；普通边=`index+1`；Entry 边=`900101`；AnyState 边=`800000+index`
- **AnyState / Entry 位置：** `AnimGraphSpecialNodeLayout` 会话级，不写资产
- **关联 Runtime：** 空 Clip State 允许 Validate；Instance hold-last Pose（ANIM-F03）

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-10 | S05 节点菜单；配合 F03 空 Clip |
| 2026-09-10 | S00–S04 代码 + 文档落地 |
| 2026-09-10 | 初版 S00–S04 |
