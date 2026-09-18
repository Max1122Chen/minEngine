# MAT-F05 — MaterialInstanceDynamic

## Meta
- **ID:** MAT-F05
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-18
- **Related:**
  - [Roadmap](./MAT-F01_MATERIAL_RUNTIME_PRODUCTIZATION_ROADMAP.md)
  - [MAT-F04 MaterialInstance](./MAT-F04_MATERIAL_INSTANCE_DESIGN.md)（存储 + Instance Proxy）
  - [MAT-F02 Proxy](./MAT-F02_MATERIAL_RENDER_PROXY_DESIGN.md) · [MAT-F03 Interface](./MAT-F03_MATERIAL_INTERFACE_DESIGN.md)
  - [MAT-F06](./MAT-F01_MATERIAL_RUNTIME_PRODUCTIZATION_ROADMAP.md)（`.memi` / MIC 编辑器 — 本 Feature Out）
- **Depends on:** MAT-F04 Done
- **UE 锚点：** `Engine/.../Public/Materials/MaterialInstanceDynamic.h` — `UMaterialInstanceDynamic : UMaterialInstance`；`Create`；`SetScalarParameterValue` / `SetTextureParameterValue` 等

## TL;DR

对标 UE **MID**：运行时 `Create(parent)` 得到可每帧 `Set*` 的 Instance；**类型上是 `MaterialInstance` 子类**，**数据与 Proxy 完全复用 F04**（同一覆盖表 + 同一 `InstanceMaterialRenderProxy`）。与 MIC 的差别在 **创建路径与用途契约**（动态、常不存盘），不在渲染模型。

## Scope
- **In:**
  - `MaterialInstanceDynamic` 类（或等价工厂，见 §4）
  - `Create` / `CreateFromMaterial` 工厂（Parent = `MaterialInterface*`，MVP 解析到 Base `Material*`）
  - 高频 `SetScalarParameter` / `SetTextureParameter`（继承或薄封装 F04）
  - 组件可 `SetMaterial(mid)`；Demo 或单测「每帧改色」
  - 文档：MID vs MI vs Material 何时用哪个
- **Out:**
  - `.memi` 序列化 / Content Browser MIC（**F06**）
  - `SetParameterByIndex` 极限优化 API（UE 有；我们后置）
  - Static Switch；复制 MIC→MID 全参数拷贝工具（可做薄 `CopyParameterValues` 可选 slice）
  - 新的 Proxy 类型或第二套覆盖表

## Reader quick start
1. 先读 [MAT-F04](./MAT-F04_MATERIAL_INSTANCE_DESIGN.md)（Instance Proxy 已存在）  
2. 本文件 §3（MID 与 MI 关系）  
3. UE：`UMaterialInstanceDynamic::Create`

---

## 1) 背景与目标

### Pain
玩法/脚本需要「临时改个颜色/贴图」而不：
- 改坏共享 `Material` 资产默认值，或
- 在编辑器里先做好 MIC 再加载。

UE 用 **MID** 解决：运行时 new 一个 Instance，Parent 指向任意 `UMaterialInterface`，Set 走同一套 Instance 参数系统。

### 成功态
- `auto mid = MaterialInstanceDynamic::Create(parentMaterial); mid->SetScalarParameter(...);` 挂到 Mesh 后画面变
- 每帧 Set 仍只 Invalidate Proxy，不 Compile、不改 Parent
- 与 F04 静态/逻辑 Instance **共享** Proxy 与覆盖存储实现

---

## 2) 现状（F04 完成后预期）

- 已有 `MaterialInstance` + `InstanceMaterialRenderProxy`
- 组件槽为 `MaterialInterface`
- **缺** 明确的运行时工厂与「可每帧改」产品契约命名

---

## 3) 方案

### 3.1 与 UE / F04 / F06 的关系

```text
MaterialInterface
  ├── Material                         // 资产；改默认值 = 改资产
  └── MaterialInstance                 // F04：Parent + 覆盖 + Instance Proxy
        ├── (逻辑/可存盘 MIC)          // F06 给 Asset 身份 + .memi
        └── MaterialInstanceDynamic    // F05：运行时 Create；通常不存盘
```

| | Material | MI（F04） | MID（F05） | MIC（F06） |
|--|----------|-----------|------------|------------|
| Compile | 是 | 否 | 否 | 否 |
| 覆盖表 | 默认值表 | 有 | **同 F04** | 同 F04 + 序列化 |
| Proxy | Default | Instance | **同 Instance** | 同 Instance |
| 典型创建 | 加载资产 | 代码/工具 | **`Create` 运行时** | Content Browser |
| 改参频率 | 少 | 中 | **高（可每帧）** | 编辑期 |

**Invariant：** MID **不**另建 `DynamicMaterialRenderProxy`；`GetRenderProxy()` 返回的就是 F04 那套 Instance Proxy（或 MID 拥有的同一类型实例）。

### 3.2 类型与工厂

**选用 A：** 子类

```cpp
class MaterialInstanceDynamic : public MaterialInstance
{
public:
    /** Parent may be Material or another Instance; MVP: require resolvable Base Material (D1). */
    static std::shared_ptr<MaterialInstanceDynamic> Create(
        std::shared_ptr<MaterialInterface> parent,
        const std::string& debugName = {});

    // Set* inherited from MaterialInstance — document as frame-safe
};
```

- `Create`：分配 MID → 设 Parent（`shared_ptr` 保活）→ 构造/确保 Instance Proxy → 返回
- Parent 为另一 Instance 时：MVP **拒绝或只取 `GetBaseMaterial()` 当 Parent**（与路线图 D1 一致）；完整 Instance 链后置

**备选 B：** 不设子类，仅 `MaterialInstance::CreateDynamic(...)` 返回 `shared_ptr<MaterialInstance>`  
- 更少类型；少了「这是动态的」类型标签与日后 Lua/反射区分  
- **默认仍选 A**（对标 UE 清晰）

### 3.3 寿命（再次强调 ≠ SceneProxy）

MID 对象由 **游戏侧 `shared_ptr`** 持有（组件槽、脚本局部变量）。  
其内部 `unique_ptr<InstanceMaterialRenderProxy>` 随 MID 同寿。  

SceneProxy / MDC 只缓存 `GetRenderProxy()` 裸指针；换 MID / 释放 MID 时与换 Material 相同规则。

```text
shared_ptr<MaterialInstanceDynamic>  (组件或脚本持有)
        │ owns
        ▼
InstanceMaterialRenderProxy
        ▲
        │ raw
SceneProxy.m_MaterialProxy
```

### 3.4 Set* 契约

| 规则 | 说明 |
|------|------|
| 只写本 Instance 覆盖表 | 永不写 Parent `Material` 默认参数 |
| 每次 Set → `InvalidateParameterBindings` | 下一帧 `BindForDraw` 刷新 UBO/BindingSet |
| 允许每帧调用 | 性能：F05 不强制做 ByIndex 优化；脏标记避免无脏全量重建贴图 set（沿用 F04 Proxy 实现） |
| 未知参数名 | MVP：no-op 或 log once（与 F04 对齐，开工时统一） |

### 3.5 组件 / 脚本接入

| 位置 | 改动 |
|------|------|
| 组件 `SetMaterial` | 已接受 Interface → 可直接挂 MID |
| Demo / 测试 | 新建 MID、每帧改 scalar、断言画面或 Proxy Get 值变化 |
| Lua（若已有 Material 绑定） | 可选 slice：暴露 `Create` + `SetScalar`；无绑定则手测/C++ 测即可 Done |

### 3.6 切片

| Slice | Goal | Verify |
|-------|------|--------|
| **S01** | `MaterialInstanceDynamic` + `Create`；复用 F04 存储/Proxy | 编译；Create 后 `GetRenderProxy()` 非空 |
| **S02** | 挂到 Mesh；Set scalar/texture 画面正确；Parent 默认值不变 | 单测 + 手验 |
| **S03** | 「每帧 Set」冒烟（测试循环或 Demo） | 连续 N 帧改色无崩、Parent 未污 |
| **S04** | 简短用法说明写入 Material README；Registry → Done | 文档 |

### 3.7 与 F06 边界

- F05 **不**要求 MID 可序列化进关卡为独立资产。
- 若关卡需要持久覆盖，走 F06 MIC，或关卡里存「Parent + 覆盖表」另议——**不**在 F05 做。
- 允许日后 `Create` 从 MIC 拷贝初始覆盖（可选，非本 Feature DoD）。

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. `MID : MaterialInstance` | 对标 UE；类型可辨 | 多一个类名 | **选用** |
| B. 仅工厂返回 MI | 少类 | 无动态语义类型 | 否（默认） |
| C. MID 自有另一套 Proxy | — | 重复；违背 F02/F04 | **否决** |

---

## 5) 风险与缓解

| 风险 | 缓解 |
|------|------|
| 误在 Material 上每帧 Set 当「动态」 | README：动态改参用 MID；改资产默认用 Material |
| Create 后 Parent 被释放 | `shared_ptr` Parent；Create 失败若 parent 空 |
| 与 F04 重复劳动 | F05 禁止复制覆盖/Proxy 逻辑；只加类型+工厂+验收 |
| 过早 Lua 绑定拖慢 | S03 可用纯 C++ 测；Lua 可选 |

---

## 6) 验收标准

- [x] `MaterialInstanceDynamic::Create` 可用；返回对象是 `MaterialInterface`
- [x] `GetRenderProxy()` 为 Instance Proxy（与 F04 同族）；Draw 经 Proxy
- [x] Set scalar/texture 影响画面；**Parent Material 默认值不变**
- [x] 连续多帧 Set 稳定（冒烟）
- [x] 无 `.memi` / 无新 Proxy 类也可 Done
- [x] Material README 有 MID 用法三行说明

**验证：** `material-ir` 含 MID smoke（8 帧 Set）；RTTI 解析 Parent 留 **MAT-F06** 收口。

## 7) Status note

**Done** — MID 工厂 + 复用 F04 Proxy。类型分发改 `MEClass`/虚解析见 [MAT-F06](./MAT-F06_MATERIAL_INSTANCE_CONSTANT_DESIGN.md)。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-18 | **Done：** 单测通过；RTTI→反射延后 F06 |
| 2026-09-18 | **实现：** MID 类 + Create；多帧 Set smoke；README 用法 |
| 2026-09-18 | **展开详设：** 寿命、工厂、切片、MIC 边界、与 SceneProxy 对比 |
| 2026-09-18 | 初版摘要；编号 F05 |
