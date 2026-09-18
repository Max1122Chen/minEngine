# MCP-F04 — Implementation Plan

## Meta
- **ID:** `MCP-F04`
- **Status:** Review
- **Last updated:** 2026-09-18
- **Design:** [MCP-F04_AUTOMATION_DOMAIN_EXPANSION_DESIGN.md](./MCP-F04_AUTOMATION_DOMAIN_EXPANSION_DESIGN.md)

## Slices

| Slice | 内容 | 验收 |
|-------|------|------|
| S01 | schema kind 增量；Host 注册表能拒绝未知 kind；MCP `editor_control` / `editor_asset` 转发（op 可先返回未实现） | 信封往返；`KindOpMismatch` |
| S02 | `TypeQueryServices` + `reflect.list_classes` / `list_properties`；Add Component 改调；删 `m_AllComponentTypeNames` | 过滤与现 picker 一致；JSON 无指针 |
| S03 | `AssetEditorServices`：`asset.list` / `create` / `delete` | 建一个 Material 或 Scene 后 list 能看见；delete 失败有消息 |
| S04 | `control`：`asset.open`、`activate_session`、`reveal_asset`、`scene.select_object` | 打开后当前 Tab 是该文档；select 不隐含聚焦 |
| S05 | `scene.move_component`、`scene.instantiate_prefab` | 与 GUI 同一撤销栈 |

未获 Design 审阅前不开工。

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-18 | 五切片草案 |
