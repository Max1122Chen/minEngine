# ED-F19 — Implementation Plan

## Meta
- **ID:** `ED-F19`
- **Status:** Done
- **Last updated:** 2026-09-19
- **Design:** [ED-F19_MATERIAL_ANIM_EDITOR_SERVICES_DESIGN.md](./ED-F19_MATERIAL_ANIM_EDITOR_SERVICES_DESIGN.md)

## Slices

| Slice | 内容 | 验收 |
|-------|------|------|
| S01 | `MaterialEditorServices` + Surface `material.*`；`get_graph` 只在 Surface | Done。服务无 GetGraph |
| S02 | 材质 GUI `Submit*` 改调服务 | Done。同一 Command |
| S03 | 动画状态/过渡 Command（删状态含过渡快照） | Done（Command 内快照）。无独立单测 |
| S04 | `AnimationGraphEditorServices` + `animgraph.*`；GUI / Debug 改调服务 | Done。save 不走 Scene |
| S05 | 文档收口 | Done |

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-19 | 五切片草案 |
| 2026-09-19 | S01–S05 Done。Editor 编译 PASS |
