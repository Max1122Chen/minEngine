# CORE-F11 — Property Accessor Thunks — Implementation Plan

## Meta
- **ID:** `CORE-F11`
- **Type:** Implementation Plan
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-05
- **Related:** [Design Spec](./CORE-F11_PROPERTY_ACCESSOR_THUNKS_DESIGN.md)

## TL;DR

S01–S06 Done。Inspector live 走 temp→Assign；语义 PostEdit / 实现不双调。

## Scope
- **In:** Design §In + S06 live Assign
- **Out:** Design §Out；Lua；全引擎字段迁移；ObjectPtr live Assign；拖动 EndEdit

## Reader quick start
1. [Design](./CORE-F11_PROPERTY_ACCESSOR_THUNKS_DESIGN.md) §3.5
2. 下表切片
3. `PROGRESS_LOG.md`

---

## 1) 切片总览

| Slice ID | 内容 | 状态 | 验证 |
|----------|------|------|------|
| `CORE-F11-S01` | header tool：解析 Getter/Setter meta；注入 `ME_REFLECTION_PROPERTY_*_THUNK` 宏 + `ADD_FIELD_ACCESSORS`；C++ 宏定义在 `ReflectionMacros.h` | **Done** | 生成样例编译；非法 meta tool 失败；坏方法名编译失败 |
| `CORE-F11-S02` | `MEProperty` Get/Set fn；`AssignProperty` / `GetPropertyValue`；PostEdit 虚函数骨架 | **Done** | `reflection-function` assign |
| `CORE-F11-S03` | Editor `ApplySetObjectProperty` → Assign/PostEdit；代表物理/组件字段挂 Setter | **Done** | 删 side-effects；组件 PostEdit |
| `CORE-F11-S04` | Serializer 叶子 + `m_Owner` resolve → Assign；TD-026 Done | **Done** | `serialization-archive` · `scene-clone` |
| `CORE-F11-S05` | 删除 `ApplyPhysicsEditorSideEffects`；BUG/Registry/Progress 收口 | **Done** | `physics-smoke` + docs |
| `CORE-F11-S06` | Inspector live：Primitive/flat Object → temp→Assign；`notifyPostEdit=!HasSetter`；削物理 PostEdit 重复分支 | **Done** | `physics-smoke` + Editor 编译 |

状态：`Planned | In Progress | Done | Blocked | Deferred | Cancelled`

---

## 2) 切片详情

### CORE-F11-S01 — Tool + codegen thunks
- **Goal:** `meta=(Getter/Setter)` → 注入 C++ thunk 宏；调整 thunk 行为只改 `ReflectionMacros.h`
- **Touch:** `ReflectionMacros.h`；`scripts/minEngine_header_tool.py`；`.gen.cpp`
- **DoD:** 合法注解生成 `ME_REFLECTION_PROPERTY_*_THUNK` + accessor 指针；非法 meta（空/非标识符）tool 报错；无 meta 字段仍 `nullptr` accessors；坏方法名 **C++ 编译失败**
- **Verify:** 跑 header tool / 增量生成；编译样例
- **Note:** 不在 Python 侧做完整签名 AST 校验（Design §3.3）

### CORE-F11-S02 — Runtime Assign API
- **Goal:** 属性可查询 thunk；Assign/Get 行为符合 Design §3.4
- **Touch:** `MEProperties.h`；Reflection 注册；`MEObject` PostEdit；新 API 头（如 `PropertyAssign.h`）
- **DoD:** 有 Setter 调计数；无 Setter 直写；PostEdit 仅在 notify 时调用
- **Verify:** `minEngineTests` 新 case 或扩展 reflection 套件

### CORE-F11-S03 — Editor 切入
- **Goal:** Inspector/Undo 权威写走 Assign
- **Touch:** `SceneEditor.cpp`；可选 PropertyPath；1–N 个真实组件 Setter 注解
- **DoD:** 代表字段不再依赖战术 side-effects 列表中的对应分支
- **Verify:** 手动或测试：改 SimulatePhysics / 等价字段

### CORE-F11-S04 — Serialize + TD-026
- **Goal:** 反序列化与 Owner resolve 走 Assign
- **Touch:** `Serializer.cpp`；Component `SetOwner` 绑定 `m_Owner`
- **DoD:** TD-026 **Done**；加载后 Owner/激活语义正确（回归 CORE-F06 场景）
- **Verify:** `scene-clone`、场景加载相关测试

### CORE-F11-S05 — 清债与文档
- **Goal:** 删除或极简 `ApplyPhysicsEditorSideEffects`；关 BUG-CORE-001（或标 Fixed + 残余说明）
- **Touch:** Physics editor hooks；TECH_DEBT；BUG docs；Registry；ACTIVE_WORK；PROGRESS_LOG
- **DoD:** Design §6 勾完（除 S06）；verify / 关键 suite 绿
- **Verify:** `serialization-archive` · `scene-clone` · physics smoke（若有）· `verify.ps1` 从 bin

### CORE-F11-S06 — Inspector live Assign
- **Goal:** live 控件不直捅成员；统一 `Get`→temp→`Assign`；语义 PostEdit 由 Setter 承担，实现层有 Setter 不调虚函数
- **Touch:** `SceneEditorInspectorSource.cpp`；`RigidBodyComponent` / `ColliderComponent` PostEdit 瘦身
- **DoD:** Primitive + flat Object（Color）经 Assign；有 Setter 字段改值走 Setter；PostEdit 覆盖仅留无 Setter 字段；Design §3.5 与代码一致
- **Verify:** Editor 编译；`physics-smoke`；可选目视 Simulate/HalfExtent
- **Out:** ObjectPtr live Assign；`m_Transform` 专用 TransformWidget（仍虚 PostEdit）

---

## 3) 依赖顺序

```text
S01 → S02 → S03 → S04 → S05 → S06
```

## 4) 延后 / 取消切片

| Slice ID | Reason | Unblock condition | Next check |
|----------|--------|-------------------|------------|
| Lua Assign | 非本期 | Script 写属性需求 | 另开 Feature |
| Setter→ME_FUNCTION 双挂 | 非热路径必需 | 脚本要调同一 Set | follow-up |
| ObjectPtr live Assign | S06 Out | Inspector ObjectPtr 也要走 Setter | 小 follow-up |

## 5) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-05 | 初版五切片 |
| 2026-09-05 | S01：宏注入契约与 Design §3.3 对齐 |
| 2026-09-05 | S01–S05 Done |
| 2026-09-05 | 开 S06：Inspector live Assign + PostEdit 语义澄清 |
| 2026-09-05 | **S06 Done** |
