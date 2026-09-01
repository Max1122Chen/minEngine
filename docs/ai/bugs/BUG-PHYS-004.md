# BUG-PHYS-004 — Collider disable/remove does not unregister physics shape

## Meta
- **ID:** BUG-PHYS-004
- **Status:** Fixed *(Editor 目视已确认)*
- **Severity:** S2
- **Owner:**
- **Found:** 2026-09-02
- **Last updated:** 2026-09-02
- **Affects:** Physics — `ColliderComponent` hierarchy; Editor Inspector remove/disable; `RigidBodyComponent` + Jolt world
- **Related Feature/Slice:** `CORE-F06` (Component Activate); `PHYS-F04`

## TL;DR
Disabling or deleting a collider component does not remove its shape from the physics world; the owning rigid body still collides with the previous geometry until rebuild.

---

## 症状
- Inspector: disable **Active** on a collider, or **Remove Component** on the collider.
- GameObject with `RigidBodyComponent` continues to collide as if the collider were still present.

## 期望
- Disable / remove collider → sibling `RigidBodyComponent` tears down Jolt body (no stale shape).
- Re-enable / re-add collider → body rebuilt from current collider data.
- RigidBody remains on GO but with **no collider** → no physics body registered.

## 复现
1. Open `test.mescene` (GO with `RigidBodyComponent` + collider).
2. Simulate in Editor viewport.
3. Disable collider **Active** or remove collider component.
4. Collisions unchanged until workaround (e.g. toggle RigidBody Active).

## 环境
- Branch: `master`
- Post `CORE-F06` Component Activate

## 根因
1. **无生命周期挂钩：** `ColliderComponent` 未覆盖 `ApplyActivationToSystems` / `RemoveActivationFromSystems`；析构函数 `= default`，删除组件时不通知 RigidBody。
2. **查询未过滤 Active：** `FindColliderComponent` / `RebuildWorldBodies` 取第一个 collider，不检查 `IsActive()`；即便触发 refresh 也可能用 inactive collider 重建形体。
3. **Editor 旁路：** `ApplyPhysicsEditorSideEffects` 不处理 collider 的 `m_bActive`（尺寸属性有 refresh，Active 无）。

已有 `RefreshPhysicsBody()` 模式（先 `DestroyPhysicsBody` 再按 collider 注册）是正确的；缺的是 **触发点** 与 **active 过滤**。

## 修复方案（已批准）

### A — 生命周期挂钩（`ColliderComponent` 基类集中）
| 事件 | 动作 |
|------|------|
| `ApplyActivationToSystems` | `RefreshOwningRigidBody()` |
| `RemoveActivationFromSystems` | `RefreshOwningRigidBody()` |
| `~ColliderComponent` | `RefreshOwningRigidBody()`（Remove Component 时；erase 后析构，不再被 `FindColliderComponent` 命中） |

不在 `GameObject::RemoveComponent` 写特判；与 CORE-F06 Active 语义一致。

### B — Active 过滤
- `RigidBodyComponent::FindColliderComponent()` — 仅返回 `IsActive()` 的 collider。
- `RefreshPhysicsBody(colliderOverride)` — override 为 inactive 时视为无 collider。
- `PhysicsSystem::RebuildWorldBodies` — 选 collider 时要求 `IsActive()`。
- `ApplyPhysicsEditorSideEffects` — collider 的 `m_bActive` 变更时 refresh（对齐 Inspector undo 路径）。

### 策略
- 禁 collider / 删 collider / 无 active collider：`DestroyPhysicsBody` 后不重建 → 无碰撞（RigidBody 可仍 Active）。
- 多 Collider 同 GO：仍只认第一个 **active** collider（既有局限，本次不扩 scope）。

## 修复
- **2026-09-02 已落地（方案 A+B）：**
  - `ColliderComponent`：`ApplyActivationToSystems` / `RemoveActivationFromSystems` / `~ColliderComponent` → `RefreshOwningRigidBody()`
  - `FindColliderComponent` / `RebuildWorldBodies` / `RefreshPhysicsBody(override)` — 仅 active collider
  - `ApplyPhysicsEditorSideEffects` — collider `m_bActive`
- **Automated:** `physics-smoke`, `physics-shapes` PASS.
- **Manual:** Editor `test.mescene` 禁/删 collider、再启用 — 已确认。

## 回归验证
- [x] Disable collider Active → no collision with that shape.
- [x] Remove collider → body unregistered; object falls through or no contact.
- [x] Re-enable collider → collision restored.
- [x] Disable RigidBody Active — CORE-F06 未回归.
- [x] `physics-smoke` / `physics-shapes` pass.

## 关联
- [CORE-F06 Component Activate Design](../Platform/Core/CORE-F06_COMPONENT_ENABLE_DESIGN.md)
- [TD-026](../TECH_DEBT.md)
- [BUG-PHYS-003](./BUG-PHYS-003.md)

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-02 | Registered during CORE-F06 acceptance |
| 2026-09-02 | 根因确认；方案 A+B 实施；Fixed（用户 Editor 目视） |
