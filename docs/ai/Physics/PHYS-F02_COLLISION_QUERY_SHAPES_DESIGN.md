# Collision + Query Shapes — Design Spec

## Meta
- **ID:** `PHYS-F02`
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-08-01
- **Related:** [Implementation](./PHYS-F02_COLLISION_QUERY_SHAPES_IMPLEMENTATION.md), [PHYS-F01](./PHYS-F01_JOLT_INTEGRATION_DESIGN.md), [FEATURE_REGISTRY.md](../FEATURE_REGISTRY.md)

## TL;DR
在 F01 的 Box + LineTrace 之上，补 **Sphere / Capsule 碰撞体** 与 **Scene SphereTrace / CapsuleTrace**（同一 Channel 矩阵）。公开入口仍是 `Scene`；`PhysicsWorld` 为实现层。

## Scope
- **In:** `SphereColliderComponent` / `CapsuleColliderComponent`；`RegisterRigidBody` 按 Collider 类型建 Jolt Shape；`Scene::SphereTrace` / `CapsuleTrace`；headless 测试
- **Out:** Mesh collider；非均匀 scale；Convex hull；玩法回调（PHYS-F03）；Editor 调试绘制；BoxSweep（可后补）

## Reader quick start
1. 本文件：形状语义与 API
2. Implementation：S01/S02 任务与验收
3. 代码：`Runtime/Function/Physics/*`、`Scene::{Sphere,Capsule}Trace`

---

## 1) 背景与目标
F01 仅 Box。角色 / 拾取物需要球与胶囊；查询需要有厚度的 sweep（Sphere/Capsule trace）。

## 2) 现状
- `ColliderComponent` + `BoxColliderComponent`；`RigidBody` 只认 Box
- `Scene::LineTrace` + 矩阵过滤已落地

## 3) 方案

### 3.1 碰撞体
| 组件 | 字段（引擎单位 m） | Jolt |
|------|-------------------|------|
| `SphereColliderComponent` | `Radius`（默认 0.5） | `SphereShape` |
| `CapsuleColliderComponent` | `Radius` + `HalfHeight`（圆柱半高，默认 0.5） | `CapsuleShape`（局部 **Y** 轴；与引擎 Up 一致） |

胶囊总高 ≈ `2 * (HalfHeight + Radius)`。Trigger + Sensor 语义同 F01。

`RigidBodyComponent::FindColliderComponent()` 取第一个 `ColliderComponent`；`RefreshPhysicsBody(ColliderComponent*)`。

### 3.2 查询 API（公开在 Scene）
```cpp
bool SphereTrace(start, end, radius, traceChannel, params, outHit);
bool CapsuleTrace(start, end, radius, halfHeight, traceChannel, params, outHit);
```
- 过滤：与 LineTrace 相同（Response≠Ignore；`IgnoreGameObject`）
- 实现：Jolt `CastShape` + `ClosestHitCollisionCollector`
- 胶囊查询取向：局部 Y = 引擎 Up（identity 旋转）

### 3.3 拍板
| ID | 决定 |
|----|------|
| P1 | 一 GO 一主 Collider（第一个）；多 Collider 以后再做 |
| P2 | Capsule 轴 = 引擎 Y |
| P3 | 查询挂 `Scene`，不挂 System |

## 4) 备选
| 选项 | 结论 |
|------|------|
| 统一 `CollisionShape` 袋 + 单一 Sweep | 延后；本期显式 Sphere/CapsuleTrace |
| Capsule 用 Z 轴（UE 习惯） | 否；引擎 Y-up |

## 5) 风险
| 风险 | 缓解 |
|------|------|
| 轴映射搞错胶囊朝向 | 落体/贴地测 + 文档写明 Y |
| CastShape 起点穿透 | 测试用分离布置；`mFraction`/`Location` 可测即可 |

## 6) 验收
- [x] Sphere/Capsule 动态体可注册并参与仿真（smoke 级）
- [x] `SphereTrace` / `CapsuleTrace` hit/miss + ignore-self
- [x] 既有 physics suites 无回归

## 7) Status note
Done — S01/S02 landed；验证见 Implementation / PROGRESS_LOG。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-01 | 初稿并开写 |
| 2026-08-01 | S01/S02 实现完成；Meta → Done |
