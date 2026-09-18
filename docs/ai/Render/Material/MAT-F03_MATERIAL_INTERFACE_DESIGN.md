# MAT-F03 — MaterialInterface + Material 迁入

## Meta
- **ID:** MAT-F03
- **Type:** Refactor + Feature
- **Status:** Done
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

**继承（已落地，偏离原稿双继承）：** 反射 `ResolvePendingSuperClasses` 拒绝第二个 super，且未登记的第二基类会报 unresolved。因此不用 `Material : Asset, MaterialInterface`。

```text
Asset
  └── MaterialInterface     ← MEObject/Asset 单链，组件槽可序列化
        └── Material
```

`Material` 仍是 `Asset`。后续 Instance 若也继承 Interface，会同样是 `Asset`（`m_Meta` 可空，不强制进资产库）。这是反射约束，不是「Interface 拥有图」。

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
| A 双继承 | 反射拒绝第二 super | **否决** |
| B `MaterialInterface : Asset`，`Material` 继承之 | 单链、槽位可序列化 | **选用** |

## 5) 风险

| 风险 | 缓解 |
|------|------|
| 双继承 / GENERATED_BODY | A/B 门禁 |
| 序列化断 | S02 含加载一关卡回归 |

## 6) 验收

- [x] 网格组件材质槽为 Interface
- [x] 填充 SceneProxy 只用 `GetRenderProxy()`
- [x] Draw 路径无回退到 `Material*` 绑定
- [x] 无 Instance 也可 Done

## 7) Status note

**Done** — Editor 打开关卡，网格材质可画。反射注册补进 `MaterialInterface.gen.cpp` 后，父类与材质槽均可解析。场景仍按 GUID 引用 `Material`，未改资产文件。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-18 | **Done：** Editor 手验通过；反射 gen 链入后父类/槽位可解析 |
| 2026-09-18 | **实现：** 单继承 `MaterialInterface : Asset`；组件槽改为 Interface |
| 2026-09-18 | 按 UE Interface 最小集补强；明确禁止 Interface 绑 Draw |
| 2026-09-18 | 从「首刀」后移为 F03 |
