# MAT-F03 — MaterialInterface + Material 迁入

## Meta
- **ID:** MAT-F03
- **Type:** Refactor + Feature
- **Status:** Planned
- **Owner:** project maintainer
- **Last updated:** 2026-09-18
- **Related:** [Roadmap](./MAT-F01_MATERIAL_RUNTIME_PRODUCTIZATION_ROADMAP.md) · [MAT-F02 Proxy](./MAT-F02_MATERIAL_RENDER_PROXY_DESIGN.md) · [MAT-F04](./MAT-F04_MATERIAL_INSTANCE_DESIGN.md)
- **Depends on:** **MAT-F02 Done**

## TL;DR

对标 `UMaterialInterface`：组件 / 资源槽只持 **`MaterialInterface`**；唯一渲染出口仍是 **`GetRenderProxy()`**（F02 已保证 Draw 只认 Proxy）。`Material` 实现 Interface。

## Scope
- **In:** `MaterialInterface` 基类；`Material` 迁入；组件与反射槽迁移；SceneProxy 填充改为 `iface->GetRenderProxy()`
- **Out:** Instance；改 Proxy 内核；Usage 枚举 / PSO precache（UE 体积项）

## Reader quick start
1. [MAT-F02](./MAT-F02_MATERIAL_RENDER_PROXY_DESIGN.md) §3.8  
2. 本文件 §3  
3. UE：`UMaterialInterface::GetRenderProxy` / `GetBaseMaterial`

---

## 1) 背景

F02 后 Draw 已稳；缺多态槽位则 Instance 无法挂到组件。Interface = **身份与查询**，不是第二套绑定路径。

## 2) 现状（F02 后预期）

- Draw / MDC：`MaterialRenderProxy*`
- 组件：仍 `shared_ptr<Material>`

## 3) 方案

### 3.1 类型（UE 对照）

```text
UMaterialInterface          → MaterialInterface
  UMaterial                 → Material : Asset + MaterialInterface
  UMaterialInstance         → (F04)
```

**继承默认 A：** `Material : public Asset, public MaterialInterface`  
**回退 B：** 无双继承（codegen 冲突时）——开工 Pre-flight 锁定。

`MaterialInterface` MVP **不必**继承 `MEObject`（可后加反射）；优先薄抽象。

### 3.2 API（对标 UE 最小集）

| API | 行为 |
|-----|------|
| `GetBaseMaterial()` | `Material*`；沿 parent 链最终材质（Material 返回 `this`） |
| `GetRenderProxy()` | 非空 `MaterialRenderProxy*` |
| `IsTranslucent` / `IsMasked` / `IsCompiledForDraw` | 默认经 `GetRenderProxy()` 或 BaseMaterial |

**禁止：** `MaterialInterface::BindForDraw` 或任何绕过 Proxy 的 Draw API。

### 3.3 迁移清单

| 位置 | 改动 |
|------|------|
| `StaticMeshComponent` / Skeletal / 相关 | `shared_ptr<MaterialInterface>`；`SetMaterial` 接受 Interface |
| `GetMaterial()` | 返回 `MaterialInterface*`（或 shared_ptr） |
| SceneProxy 填充 | `m_MaterialProxy = iface ? iface->GetRenderProxy() : nullptr` |
| 资产加载赋默认材质 | `shared_ptr<Material>` 隐式/显式转为 Interface |
| 序列化/反射字段 | 随组件类型更新（注意 gen） |

### 3.4 切片

| Slice | 内容 |
|-------|------|
| S01 | Interface 声明；Material 实现；编译 |
| S02 | 组件 + 反射 + 赋值路径 |
| S03 | grep `shared_ptr<Material>` 生产组件槽；文档 Done |

## 4) 备选

| 选项 | 结论 |
|------|------|
| A 双继承 | **默认** |
| B 无双继承 | 冲突回退 |
| Interface 继承 Asset | **否** — 仅 Material 是 Asset；Instance 另议（F04/F06） |

## 5) 风险

| 风险 | 缓解 |
|------|------|
| 双继承 / GENERATED_BODY | A/B 门禁 |
| 序列化断 | S02 含加载一关卡回归 |

## 6) 验收

- [ ] 网格组件材质槽为 Interface
- [ ] 填充 SceneProxy 只用 `GetRenderProxy()`
- [ ] Draw 路径无回退到 `Material*` 绑定
- [ ] 无 Instance 也可 Done

## 7) Status note

**Planned** — F02 审批落地后再升 Review。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-18 | 按 UE Interface 最小集补强；明确禁止 Interface 绑 Draw |
| 2026-09-18 | 从「首刀」后移为 F03 |
