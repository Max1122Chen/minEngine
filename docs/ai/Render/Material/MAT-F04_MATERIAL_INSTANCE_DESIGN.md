# MAT-F04 — MaterialInstance（Parent + 覆盖 + Instance Proxy）

## Meta
- **ID:** MAT-F04
- **Type:** Feature
- **Status:** Planned
- **Owner:** project maintainer
- **Last updated:** 2026-09-18
- **Related:** [Roadmap](./MAT-F01_MATERIAL_RUNTIME_PRODUCTIZATION_ROADMAP.md) · [MAT-F02](./MAT-F02_MATERIAL_RENDER_PROXY_DESIGN.md) · [MAT-F03](./MAT-F03_MATERIAL_INTERFACE_DESIGN.md) · [MAT-F05](./MAT-F05_MATERIAL_INSTANCE_DYNAMIC_DESIGN.md)
- **Depends on:** MAT-F02 + MAT-F03 Done
- **UE 锚点：** `UMaterialInstance` + `FMaterialInstanceResource`（`MaterialInstanceSupport.h` / `MaterialInstance.cpp`）

## TL;DR

对标 UE MIC/MI 运行时核心（不含存盘）：`MaterialInstance` 持 Parent + 覆盖表，拥有 **`InstanceMaterialRenderProxy`**。Draw 仍只问 Proxy；参数 **本地覆盖 → Parent Proxy**（与 `FMaterialInstanceResource::GetParameterValue` 同构）。**不** Compile；**不**写回 Parent。

## Scope
- **In:** Instance 类；覆盖表；Instance Proxy；组件可挂；双物体异覆盖验收；单测 Parent 不被改
- **Out:** MID 工厂糖（F05）；`.memi`（F06）；Static Switch；Parent=另一 Instance（D1 后置）

## Reader quick start
1. MAT-F02 §3.8 预留契约  
2. 本文件 §3  
3. UE：`FMaterialInstanceResource::GetParameterValue` miss → Parent Proxy

---

## 1) 背景

F02 提供可替换 Proxy；F03 提供槽位。F04 把「覆盖」放进 **第二种 Proxy**，而不是 Draw 分支。

## 2) 方案

### 2.1 类型

```text
MaterialInterface
  ├── Material
  └── MaterialInstance
        ├── Parent: shared_ptr<Material>   // MVP：仅 Material（路线图 D1）
        ├── Override maps (scalar / texture by name)
        └── owns InstanceMaterialRenderProxy
```

可选：Instance 是否 `Asset` —— **MVP 否**（纯运行时对象）；存盘进 F06。

### 2.2 InstanceMaterialRenderProxy（对标 FMaterialInstanceResource）

| 虚函数 | 行为 |
|--------|------|
| `GetBaseMaterial()` | `Parent->GetBaseMaterial()` |
| `GetGPUShader*` / Layout / Blend | forward BaseMaterial（与 Parent 共用编译产物） |
| `Get*ParameterValue*` | 覆盖命中 → 返回；否则 `Parent->GetRenderProxy()->Get*…` |
| `GetMaterialShaderBindingSet` | **本 Proxy 自有** BindingSet（按解析后的贴图等重建） |
| `BindForDraw` | dirty 则按虚 Get\* 填 UBO/BindingSet（可复用 Default 的重建算法，数据源换虚 Get） |
| `Invalidate` | Set* 时调用 |

**游戏线程 Set\*：** 只写 Instance 覆盖表 + `m_Proxy->InvalidateParameterBindings()`。  
**禁止：** `Set*` 修改 `Parent` 的 `m_*Parameters`。

### 2.3 与 UE 的差异（刻意）

| UE | 我们 |
|----|------|
| GT 参数数组 + RT `RenderThread_UpdateParameter` | 单线程一份覆盖 + Invalidate |
| 可有 StaticPermutation Resource | **Out** |
| Parent 可为 Interface 链 | MVP Parent **仅 Material** |

### 2.4 API 草稿

```cpp
class MaterialInstance : public MaterialInterface // (+ MEObject 若需)
{
public:
    static std::shared_ptr<MaterialInstance> Create(std::shared_ptr<Material> parent);

    void SetScalarParameter(const std::string& name, float value);
    void SetTextureParameter(const std::string& name, std::shared_ptr<Texture2D> texture);

    Material* GetBaseMaterial() override;
    MaterialRenderProxy* GetRenderProxy() override;
};
```

### 2.5 切片（详设开工时再拆 Implementation）

| Slice | Goal |
|-------|------|
| S01 | 类 + 覆盖表 + Instance Proxy Get\* 链；编译 |
| S02 | Bind/BindingSet 自有缓存；挂组件画出来 |
| S03 | 双 Mesh 异覆盖 + Parent 不变单测 |
| S04 | 文档 Done |

## 3) 验收

- [ ] 两 Mesh 同 Parent、不同 scalar/texture 覆盖，画面正确
- [ ] Assert/测试：Instance Set 后 Parent 默认值未变
- [ ] Draw 不 `#include` 覆盖表；只经 Instance Proxy
- [ ] `GetGPUShader` 与 Parent 为同一编译结果（指针相等或等价）

## 4) Status note

**Planned** — 依赖 F02 的「BindingSet 在 Proxy 上」决策；若 F02 改回 BindingSet 留 Material，本 Feature 须重开设计。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-18 | 按 `FMaterialInstanceResource` 补强 Instance Proxy 行为与切片 |
| 2026-09-18 | 编号调整为 F04 |
