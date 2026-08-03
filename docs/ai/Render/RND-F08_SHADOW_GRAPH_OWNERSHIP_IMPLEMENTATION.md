# Shadow Map Graph Ownership — Implementation Plan

## Meta
- **ID:** `RND-F08`
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-08-02
- **Related:** [Design Spec](./RND-F08_SHADOW_GRAPH_OWNERSHIP_DESIGN.md)

## TL;DR

S01–S03 已落地：Execute 重排；Dir/Spot/Point Absolute 进图；Manager 无纹理 Create。待用户目视阴影。

## Scope
- **In:** 见 Design
- **Out:** 算法/VK transient

## Reader quick start
1. Design
2. 下表
3. `PROGRESS_LOG.md`

---

## 1) 切片总览

| Slice ID | 内容 | 状态 | 验证 |
|----------|------|------|------|
| RND-F08-S01 | Execute 重排；Directional Absolute 2DArray 进图；Bind 后 Set1 | Done | tests + 目视 |
| RND-F08-S02 | Spot 2D / Point Cube 进图 | Done | tests |
| RND-F08-S03 | 删除 Ensure*/Manager 纹理成员；TD-020 Done | Done | 无 RHICreate in Manager |

## 2) 切片详情

### RND-F08-S01 — Directional + Execute 相位
- **Goal:** SetupAttachments → BindDirAtlas → BuildSceneSet1 → Enqueue；Dir atlas 图拥有。
- **DoD:**
  - [x] `AcquireDirectional` 不再 Ensure
  - [x] `ShadowGraphPass` 声明 `kRDGDirShadowAtlas`
  - [x] Set1 前 Texture 已绑定
- **Verify:** `test smoke`；Editor CSM 目视

### RND-F08-S02 — Spot / Point
- **DoD:**
  - [x] Spot/Point 无 Ensure 创建
  - [x] Cube/Array dims 正确（`GraphDepthResourceName` 共享）
- **Verify:** smoke

### RND-F08-S03 — 删除 Manager 纹理所有权
- **DoD:**
  - [x] 无 Ensure*
  - [x] TD-020 Done
- **Verify:** Manager 无 `RHICreateTexture`

## 3) 依赖顺序

```text
S01 → S02 → S03（同会话落地）
```

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-02 | 初稿；与 Design 同开 |
| 2026-08-02 | S01–S03 Done |
