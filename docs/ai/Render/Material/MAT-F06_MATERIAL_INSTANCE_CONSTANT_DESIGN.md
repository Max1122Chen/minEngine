# MAT-F06 — MIC 资产 / Editor（`.memi`）

## Meta
- **ID:** MAT-F06
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-19
- **Related:**
  - [Roadmap](./MAT-F01_MATERIAL_RUNTIME_PRODUCTIZATION_ROADMAP.md)
  - [MAT-F04 MaterialInstance](./MAT-F04_MATERIAL_INSTANCE_DESIGN.md)
  - [MAT-F05 MaterialInstanceDynamic](./MAT-F05_MATERIAL_INSTANCE_DYNAMIC_DESIGN.md)
  - [MAT-F03 MaterialInterface](./MAT-F03_MATERIAL_INTERFACE_DESIGN.md)
- **Depends on:** MAT-F04 Done（存储 + Proxy）；MAT-F05 Done（MID 工厂；本 Feature 不改 MID 渲染模型）
- **UE 锚点：** `UMaterialInstanceConstant`（可存盘 MIC）vs `UMaterialInstanceDynamic`；Content Browser 创建/打开

## TL;DR

给 **可存盘的 Material Instance（MIC）** 资产身份：扩展名 **`.memi`**，反射可序列化 Parent + 覆盖表；Content Browser / Loader 可创建与打开。运行时仍走 F04 **同一套** `MaterialInstance` 覆盖 + `InstanceMaterialRenderProxy`。顺带把 Instance 纳入 **`ME_CLASS`**，用反射类型分发替换 F05 `Create` 里的 `dynamic_cast`（及可选虚函数 `ResolveParentMaterial`）。

## Scope
- **In:**
  - `MaterialInstance`（及必要时 MIC 别名/子类）**`ME_CLASS`** + gen；`Create`/`Load` 路径 `SetClass`
  - `.memi` 序列化：Parent 引用（GUID）+ scalar/texture 覆盖
  - `MaterialInstanceLoader`（或扩展 AssetTypeRegistry）
  - Content Browser：Create / Open / Save MIC（最小可用）
  - 关卡/组件槽可挂已加载的 MIC（已是 `MaterialInterface`）
  - 去掉 MID `Create` 对 RTTI `dynamic_cast` 的依赖（`IsA` 或 Interface 虚解析）
- **Out:**
  - 完整 Material Instance Editor 图（参数面板 MVP 即可）
  - Parent = 另一 Instance 的完整链（可做薄切片；默认仍 D1 落到 Base Material）
  - Static Switch / 置换编译
  - 新 Proxy 类型
  - 把 MID 序列化进 `.memi`（MID 仍「通常不存盘」）

## Reader quick start
1. F04：覆盖表 + Instance Proxy  
2. F05：MID = 运行时工厂；**不**存盘  
3. 本文件：MIC = 资产 + 反射 + CB  
4. UE：MIC vs MID 用途差，渲染同构

---

## 1) 背景与目标

### Pain
- 需要「同 Material、异参数」的**可复用预设**（关卡间共享、CB 可点）
- F04/F05 的 Instance/MID 是纯运行时对象：**无**资产路径、无序列化、无反射类型
- MID `Create` 用 `dynamic_cast` 区分 Material vs Instance——在未注册 `ME_CLASS` 时不得已；应在 F06 收口

### 成功态
- CB 新建 `Foo.memi` → Parent 指向某 `.memtl` → 设覆盖 → 保存/重载覆盖仍在
- Mesh 槽挂 MIC；Draw 经 Instance Proxy；改 MIC 覆盖不影响 Parent `.memtl`
- `MaterialInstanceDynamic::Create(parent)` 用 `MEClass::IsA` 或 Interface 虚方法解析 Parent，**无** RTTI cast

---

## 2) 现状（F05 完成后）

| 已有 | 缺口 |
|------|------|
| `MaterialInstance` + overrides + Instance Proxy | 非 `ME_CLASS`；无 `.memi` |
| `MaterialInstanceDynamic::Create` | `dynamic_pointer_cast` / `dynamic_cast` |
| 组件 `shared_ptr<MaterialInterface>` | 无法从磁盘加载 Instance 资产 |
| `MaterialLoader` / `.memtl` | 无 Instance Loader |

---

## 3) 方案

### 3.1 类型关系

```text
MaterialInterface (ME_CLASS, Asset)
  ├── Material              // .memtl
  └── MaterialInstance      // F06：ME_CLASS；可有 AssetMeta → .memi
        └── MaterialInstanceDynamic   // 仍可不注册为独立资产类型；可选 ME_CLASS 仅运行时
```

**选用：** 可存盘对象就是 **`MaterialInstance` + AssetMeta**（对标 UE MIC ≈ 带包路径的 Instance）。不必再发明 `MaterialInstanceConstant` 类，除非编辑器需要强类型区分——MVP 用资产扩展名/TypeId 区分即可。

| | Material | MI（运行时/MIC） | MID |
|--|----------|------------------|-----|
| `ME_CLASS` | 是 | **F06 起是** | 可选；默认跟 MI 或仅运行时 |
| 磁盘 | `.memtl` | **`.memi`** | 否 |
| Proxy | Default | Instance | 同 Instance |

### 3.2 序列化契约（`.memi`）

建议字段（与现有 Json 资产风格对齐，开工时钉死）：

| 字段 | 含义 |
|------|------|
| Parent | `shared_ptr` / `$guid` → `Material`（MVP；日后可 Interface） |
| ScalarOverrides | `map<string, float>` 或数组 `{Name, Value}` |
| TextureOverrides | `map<string, Texture2D ref>` |

- **不**序列化 Proxy、BindingSet、GPU 缓存  
- 加载后：`InitializeParent` + 灌覆盖 + `EnsureRenderProxy`；Parent Material 须已 Compile（或延迟到首次 Draw）

### 3.3 反射与类型分发（承接 F05 遗留）

**目标：** `MaterialInstanceDynamic::Create` 不再依赖 C++ RTTI。

**路径 A（推荐，与引擎一致）：**  
`MaterialInstance`（及可选 MID）加 `ME_CLASS`；`Create`/`NewObject`/`Loader` 后 `SetClass`。

```cpp
if (parent->IsA(Material::StaticClass()))
    materialParent = /* shared from parent as Material */;
else if (parent->IsA(MaterialInstance::StaticClass()))
    materialParent = static_cast<MaterialInstance*>(parent.get())->GetParentShared();
```

注意：`shared_ptr` 回收仍需 `static_pointer_cast`（在 `IsA` 确认后安全），或 Interface 提供：

**路径 B（并行可做）：**  
`MaterialInterface::ResolveParentMaterial(self)` 虚函数——不依赖 `m_Class` 是否已 Set，对测试里裸 `make_shared` 更稳。

**默认：** F06 **A + B 都做**——虚函数为权威解析；`IsA` 用于编辑器/工具分支。禁止再引入新的 `dynamic_cast` 材质族分支。

### 3.4 Loader / Registry

| 项 | 行为 |
|----|------|
| AssetTypeId | `"MaterialInstance"`（或 `"MIC"`） |
| 扩展名 | `.memi` |
| Load | 反序列化 → Finalize（校验 Parent）→ 可选预 Invalidate Proxy |
| Save | 只写 Parent + 覆盖 |

对标 `MaterialLoader`；不 Compile Instance。

### 3.5 Editor MVP

| Slice 意图 | 最小行为 |
|------------|----------|
| CB Create | 选 Parent Material → 写空覆盖 `.memi` |
| Open | 打开 Inspector：Parent 引用 + 覆盖列表（可先反射属性） |
| Save | 写回 `.memi` |
| 拖到 Mesh | 已有 Interface 槽 |

**Out（后置）：** 专用 Instance 编辑器窗口、从 MID 拷贝到 MIC 工具。

### 3.6 切片建议

| Slice | Goal |
|-------|------|
| **S01** | `MaterialInstance` `ME_CLASS` + gen；Create/Load `SetClass`；`ResolveParentMaterial`；改写 MID `Create` |
| **S02** | `.memi` 序列化往返单测（Parent + scalar/texture） |
| **S03** | Loader + AssetTypeRegistry |
| **S04** | CB Create/Open/Save MVP |
| **S05** | 文档 Done；Registry |

### 3.7 与 F05 / 路线图

- F05 Done **不**阻塞：当前 RTTI 可保留到 S01  
- 路线图 D1：Parent MVP 仍为 Material；Instance 链后置  
- D5：本 Feature 兑现 `.memi`

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. MIC = `MaterialInstance` + `.memi` | 少类型；与 F04 一致 | MIC/运行时 MI 同名需靠 Meta 区分 | **选用** |
| B. 新类 `MaterialInstanceConstant` | 对标 UE 命名 | 双存储/双 Proxy 风险 | 否（除非编辑器强需求） |
| C. 仅虚函数、不上 ME_CLASS | 改动小 | CB/序列化仍缺类型 | 否；资产必须反射 |

---

## 5) 风险与缓解

| 风险 | 缓解 |
|------|------|
| 无 Meta 的 MI 与 MIC 混淆 | 运行时 `Create` 不设 AssetMeta；Loader 必设 |
| Parent GUID 丢失 | 加载失败明确错误；禁止静默空 Parent |
| MID 误存盘 | 资产类型不含 MID；Save 路径拒绝 Dynamic |
| `IsA` 遇未 SetClass | S01 强制工厂/Loader SetClass；保留虚 `ResolveParentMaterial` |

---

## 6) 验收标准

- [x] `.memi` 保存/加载后覆盖与 Parent 正确
- [x] CB 可创建并打开 MIC；可挂到 Mesh 绘制（实现已接；维护者收口合并）
- [x] `MaterialInstance` 为 `ME_CLASS`；MID `Create` **无** `dynamic_cast`/`dynamic_pointer_cast` 材质分支
- [x] 无新 Proxy 类；Draw 仍只问 Instance Proxy
- [x] MID 仍默认不序列化为 `.memi`

**验证建议：** 单测序列化往返；Editor 手验 CB + 场景。

---

## 7) Status note

**Done** — S01–S05：`.memi` 资产面 + CB MVP + ME_CLASS / ResolveParentMaterial。后置：专用 Instance 编辑器、Parent=Instance 链。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-19 | **Done / S05：** 文档收口；维护者合并收口 |
| 2026-09-19 | **S04：** CB 创建 `.memi`；Inspector 改 scalar 覆盖并 Save；材质槽按 `MaterialInterface` 列候选 |
| 2026-09-18 | **实现开始：** S01 ResolveParentMaterial + ME_CLASS；S02/S03 Loader `.memi` |
| 2026-09-18 | **初版：** `.memi` / ME_CLASS / 替换 MID RTTI；切片 S01–S05 |
