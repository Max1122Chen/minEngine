# Collision + Query Shapes — Implementation Plan

## Meta
- **ID:** `PHYS-F02`
- **Type:** Implementation Plan
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-08-01
- **Related:** [Design](./PHYS-F02_COLLISION_QUERY_SHAPES_DESIGN.md)

## TL;DR
S01：Sphere/Capsule collider + Register 多态建 Shape。S02：`Scene` Sphere/CapsuleTrace + 测试。**已完成。**

## 1) 切片

| Slice | 标题 | 验证 |
|-------|------|------|
| PHYS-F02-S01 | Sphere/Capsule collider | 编译；球/胶囊落体或静止注册 |
| PHYS-F02-S02 | SphereTrace / CapsuleTrace | `minEngineTests.exe test physics-shapes` |

### PHYS-F02-S01
- [x] `SphereColliderComponent` / `CapsuleColliderComponent`
- [x] `FindColliderComponent`；`RegisterRigidBody(..., ColliderComponent*)`
- [x] `RebuildWorldBodies` / Editor side effects 认基类 Collider
- [x] LineTrace hit 填 `FindColliderComponent`

### PHYS-F02-S02
- [x] `PhysicsWorld::{Sphere,Capsule}Trace` + `Scene` 转发
- [x] `physics-shapes` suite：落体 + trace hit/miss/ignore
- [x] 回归 smoke/contact/linetrace

## 2) 刻意不做
Mesh；BoxSweep；PHYS-F03 回调；enum TD-013

## 3) 验证记录
- `cmake --build minEngine/build --target minEngineTests`
- `minEngineTests.exe test physics-shapes` PASS
- 回归：`physics-linetrace` / `physics-contact` / `physics-smoke` PASS

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-01 | 初稿 |
| 2026-08-01 | S01/S02 Done；Meta → Done |
