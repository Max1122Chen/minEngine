# Shadow Map Slot Semantics — Slim-down Design

## Meta
- **ID:** `RND-F08` follow-up（不新开 Feature）
- **Type:** Refactor
- **Status:** Done
- **Last updated:** 2026-08-03
- **Related:** [RND-F08 Design](./RND-F08_SHADOW_GRAPH_OWNERSHIP_DESIGN.md), TD-020 Done

## TL;DR

`ShadowResourceManager` 已删除；`SlotIndex` 显式对齐 Set1 / LightUBO；纹理仍由 RDG 拥有。

## 现状问题

| 角色 | 实际依赖 |
|------|----------|
| Set1 | `ctx.*Handles[i].Texture` — **不**读 Manager |
| LightUBO | ~~`TextureUnit - BASE_UNIT`~~ → **`SlotIndex`** |
| RDG | `GraphDepthResourceName` + Absolute |

## 目标语义

```text
ShadowResourceHandle（采样槽描述符，非资源所有者）
  SlotIndex      // 0..MAX-1；与 Set1 数组下标一致
  ResourceType / Resolution / LayerCount
  Texture        // BindGraphShadowTextures 后非空
  GraphDepthResourceName（在 DrawCommand 上）
```

- Set1：`SpotShadowHandles[i]` → layout `SpotShadow{i}`
- LightUBO：`Params.w = SlotIndex`
- **已删除** `ShadowResourceManager`

## Out

- 改 shader 布局槽位数；atlas 打包；VK。

## 验收

- [x] 无 `ShadowResourceManager` 源文件引用
- [x] LightUBO / Bind 不使用 `TextureUnit` 选图
- [x] `test smoke` + `test render-graph` PASS（黄金场景阴影待用户确认）

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-03 | 短设计；落地删除 Manager + SlotIndex |
