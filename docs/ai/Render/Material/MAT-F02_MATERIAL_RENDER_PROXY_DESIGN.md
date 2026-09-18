# MAT-F02 — MaterialRenderProxy（渲染绑定真源）

## Meta
- **ID:** MAT-F02
- **Type:** Feature + Refactor
- **Status:** In Progress
- **Owner:** project maintainer
- **Last updated:** 2026-09-18
- **Related:**
  - [Roadmap MAT-F01](./MAT-F01_MATERIAL_RUNTIME_PRODUCTIZATION_ROADMAP.md)
  - [MAT-F03 Interface](./MAT-F03_MATERIAL_INTERFACE_DESIGN.md) · [MAT-F04 Instance](./MAT-F04_MATERIAL_INSTANCE_DESIGN.md)
  - 代码：`Material.h` / `Material.cpp` · `SceneMeshDrawUtils.cpp` · `MeshDrawCommand.h` · `*SceneProxy` · `*MeshComponent`
  - UE 锚点（`D:/Dev/GitRepo/UnrealEngine`）：
    - `Public/Materials/MaterialRenderProxy.h` — `FMaterialRenderProxy`（参数查询 + Uniform cache）
    - `Private/Materials/Material.cpp` — `FDefaultMaterialInstance : FMaterialRenderProxy`
    - `Private/Materials/MaterialInstanceSupport.h` — `FMaterialInstanceResource`
    - `Public/Materials/MaterialInterface.h` — `GetRenderProxy()` 纯虚
- **Depends on:** 现有 `Material::Compile` / BindingSet / `BindForDraw`（已有）

## TL;DR

把 Draw 用的 **参数解析视图 + GPU 绑定缓存（UBO / BindingSet）** 从 `Material*` 直调，抽成可替换的 **`MaterialRenderProxy`**。`Material` 保留图 / Compile / 默认参数值 / shader；`DefaultMaterialRenderProxy` 对标 UE `FDefaultMaterialInstance`。MDC / SceneProxy / Draw utils **只持 Proxy\***。本 Feature 不做 Interface/MI；但 API 必须让 F04 Instance Proxy 能子类化而不再改 Draw。

## Scope
- **In:**
  - `MaterialRenderProxy` 基类 + `DefaultMaterialRenderProxy`
  - **GPU 绑定缓存迁到 Proxy**（见 §3.2）；Layout + Shader 留在 Material
  - 虚函数：参数查询（按名/slot）+ `BindForDraw` + shader/layout/binding 访问 + Invalidate
  - Draw 全路径改 `MaterialRenderProxy*`
  - `Material::Set*` → 改默认值表 + `InvalidateParameterBindings`
  - 删除生产路径 `Material::BindForDraw`
- **Out:**
  - `MaterialInterface` / MI / MID / `.memi`
  - 渲染线程 `FRenderResource`、双线程 Uniform cache
  - `GetMaterialWithFallback` 默认材质树
  - Static permutation / 独立 `FMaterial` 类层级

## Reader quick start
1. §3.0 UE 对照（必读）  
2. §3.2 所有权（**Blocking 决策已写入默认**）  
3. §3.3 API  
4. §3.5 切片  
5. 代码：`SceneMeshDrawUtils` / `Material::RebuildMaterialShaderBindingSet`

---

## 1) 背景与目标

### Pain
Draw 持 `Material*` 并调用 `BindForDraw` / `GetMaterialShaderBindingSet`。Instance 若要覆盖参数，只能污染资产或在 Draw utils 分支——两者都不可接受。

### UE 解法（源码职责）
| 对象 | 职责 |
|------|------|
| `UMaterial` | 图、编译、`FMaterialResource`（shader map）、默认参数 |
| `FDefaultMaterialInstance` | Material 的默认 `FMaterialRenderProxy`；`GetParameterValue` 从 MaterialResource/表达式取默认 |
| `FMaterialInstanceResource` | Instance 的 Proxy；本地覆盖 miss → `Parent->GetRenderProxy()` |
| Renderer | **只问 Proxy** 要参数与（经 Proxy）可用的 `FMaterial` |

### 成功态
- 生产 Draw **只**经 `MaterialRenderProxy*`
- Proxy 可被子类替换（F04）而不改 `SceneMeshDrawUtils`
- 现网可画、改参仍正确

---

## 2) 现状

| 位置 | 行为 |
|------|------|
| `Material` | 图、Compile、`m_*Parameters`、**BindingSet、UBO、TextureViewCache**、`BindForDraw` |
| `MeshDrawCommand::m_Material` | `Material*` |
| `*SceneProxy::m_Material` | `Material*` |
| Components | `shared_ptr<Material>` → `sceneProxy.m_Material = m_Material.get()` |
| `SceneMeshDrawUtils` | 直读 Material 的 shader / BindingSet / BindForDraw |
| `SetScalarParameter` | 只改表；`BindForDraw` **每 draw** 重传 UBO |
| `SetTextureParameter` | 改表 + 立即 `RebuildMaterialShaderBindingSet` |

---

## 3) 方案

### 3.0 UE → minEngine 对照（本 Feature 范围）

```text
UE                                      minEngine F02
──                                      ─────────────
UMaterial                               Material (Asset)
  Graph / compile                         m_Graph / Compile()
  FMaterialResource (shader map)          m_GPUShader(+Skinned) + BindingSetLayout
  default parameter values                m_ScalarParameters / m_TextureParameters
  DefaultMaterialInstance ─────────────►  DefaultMaterialRenderProxy (owned)
       GetParameterValue(defaults)              GetScalar/GetTexture → 读 Material 默认表
       (UniformExpressionCache on proxy)        BindingSet + ScalarUBO + SRV cache **on proxy**

FMaterialRenderProxy (abstract)         MaterialRenderProxy (abstract)
FMaterialInstanceResource               (F04) InstanceMaterialRenderProxy

Mesh draw holds proxy                   MeshDrawCommand::m_MaterialProxy
```

**刻意不镜像：** `EvaluateUniformExpressions` 异步、FeatureLevel 多 Resource、Fallback 材质树。  
**合一步等价：** Proxy.`BindForDraw` =「解析当前视图参数 → 更新 UBO / 必要时重建 BindingSet」。

### 3.1 目标结构

```text
Material
  ├── compile products: shaders, BindingSetLayout, ParameterLayout
  ├── default values: m_ScalarParameters, m_TextureParameters
  ├── owns unique_ptr<MaterialRenderProxy>  // DefaultMaterialRenderProxy
  └── GetRenderProxy()

DefaultMaterialRenderProxy : MaterialRenderProxy
  ├── weak/raw back-pointer to Material (non-owning)
  ├── owns: ScalarUBO, BindingSet, TextureViewCache / SRVs  // GPU 绑定缓存
  ├── Get* → Material 默认参数表
  └── BindForDraw / Invalidate

Draw: MDC / SceneProxy → MaterialRenderProxy* only
```

### 3.2 所有权决策（已拍板默认）

| 数据 | 所有者 | 理由 |
|------|--------|------|
| Graph / IR / Compile 诊断 | `Material` | 资产真源 |
| `RHIShader` / Skinned / **BindingSetLayout** | `Material` | 编译产物；Instance 共用同一 layout/shader（无 Static Switch） |
| 默认 scalar/texture **值** | `Material` | 资产默认；Set* 改这里 |
| **Scalar UBO、BindingSet、SRV/ViewCache** | **`MaterialRenderProxy`** | 对标 UE Proxy 上的 Uniform cache；F04 Instance 才能各自持有绑定缓存而不改 Parent |
| Proxy 对象本身 | 游戏对象（Material / 日后 Instance）`unique_ptr` | 与 UE Default/Resource 同寿模型（无 RT 延迟删则同线程销毁即可） |

**Compile 之后：** Material 写好 shaders + layout；调用 `GetRenderProxy()->InvalidateParameterBindings()`（或 `NotifyCompiled`），由 Proxy 按 layout 重建 BindingSet。  
**禁止：** Draw utils 再读 `Material` 的 BindingSet/UBO 字段（字段迁走或改为 private + 仅 Proxy friend）。

### 3.3 API 契约

```cpp
class MaterialRenderProxy
{
public:
    virtual ~MaterialRenderProxy() = default;

    /** Compiled material that owns the shader/layout. Instance Proxy returns Parent's base. */
    virtual Material* GetBaseMaterial() const = 0;

    // --- draw queries (may forward to GetBaseMaterial for shaders/layout) ---
    virtual bool IsCompiledForDraw() const = 0;
    virtual bool IsCompiledForSkinnedDraw() const = 0;
    virtual bool IsTranslucent() const = 0;
    virtual bool IsMasked() const = 0;

    virtual RHIShader* GetGPUShader() const = 0;
    virtual RHIShader* GetGPUShaderSkinned() const = 0;
    virtual RHIShaderBindingSetLayout* GetMaterialShaderBindingSetLayout() const = 0;

    /** Resolved binding set for *this* proxy view (owned by this proxy). */
    virtual RHIShaderBindingSet* GetMaterialShaderBindingSet() const = 0;

    /**
     * Parameter view (UE GetParameterValue 缩略版).
     * DefaultMaterialRenderProxy: Material 默认表。
     * Instance Proxy (F04): 覆盖 miss → ParentProxy->Get*.
     */
    virtual bool GetScalarParameterValue(const std::string& name, float& outValue) const = 0;
    virtual bool GetTextureParameterValue(
        const std::string& name, std::shared_ptr<Texture2D>& outTexture) const = 0;

    /** Optional slot helpers for Bind path (layout 按 SlotIndex). */
    virtual bool GetScalarParameterValueBySlot(int slotIndex, float& outValue) const = 0;
    virtual bool GetTextureParameterValueBySlot(
        int slotIndex, std::shared_ptr<Texture2D>& outTexture) const = 0;

    /**
     * Ensure GPU cache matches parameter view, then upload scalar UBO as needed.
     * May no-op heavy rebuild if not invalidated.
     */
    virtual void BindForDraw(RHICommandList& cmdList) = 0;

    /** After game-thread Set* / Compile that changes layout or values. */
    virtual void InvalidateParameterBindings() = 0;
};
```

`DefaultMaterialRenderProxy`：
- `GetBaseMaterial()` → owner `Material*`
- `GetGPUShader*` / Layout / Blend / IsCompiled* → forward Material
- `Get*Parameter*` → 读 Material 的 `m_*Parameters`
- `GetMaterialShaderBindingSet` → **Proxy 自有** `m_BindingSet`
- `BindForDraw`：若 dirty 则按 **虚 Get\*BySlot** 填资源并 `CreateShaderBindingSet`；再按 slot 写 UBO（逻辑迁自今日 `RebuildMaterialShaderBindingSet` + `BindForDraw`）
- **不** Compile、不持 Graph

`Material` 对外：
- `MaterialRenderProxy* GetRenderProxy() const;`（构造即非空）
- `SetScalarParameter` / `SetTextureParameter`：改默认表 + `InvalidateParameterBindings()`
- **删除** 公开 `BindForDraw`；内部可保留 private helper 仅供 Default Proxy friend 调用（S01 过渡），S02 后删除或仅 Proxy 实现

**Invariant：** `SceneMeshDrawUtils` 不得 `#include` 后调用 Material 绑定 API；只认 Proxy。

### 3.4 Draw / 组件迁移（F02）

| 位置 | 改动 |
|------|------|
| `MeshDrawCommand` | `MaterialRenderProxy* m_MaterialProxy = nullptr;` |
| `StaticMeshSceneProxy` / Skeletal / Sprite / Widget | `m_MaterialProxy`；填充：`material->GetRenderProxy()` |
| `StaticMeshComponent` 等 | **仍** `shared_ptr<Material>`；`CreateSceneProxy` 写 Proxy* |
| `RenderScene` 等同步路径 | 凡赋 `m_Material` 处改为 Proxy |
| `SceneMeshDrawUtils` / `ForwardRenderer` 收集与 translucent 判断 | 经 Proxy（`IsTranslucent` 等） |
| `grep` 门禁 | 生产路径无 `->BindForDraw` on Material；MDC 无 `Material* m_Material` |

F03 再改组件存储类型；F02 不阻塞。

### 3.5 切片

| Slice | Goal | Verify |
|-------|------|--------|
| **S01** | 新增 Proxy 头/源；GPU 缓存迁到 DefaultProxy；Material 拥有 Proxy；Compile/Set 走 Invalidate；保留一条能画的路径（可暂经 `GetRenderProxy()->BindForDraw`） | 编译通过；已有 material 相关测试 |
| **S02** | MDC / 全部 SceneProxy / Forward / Draw utils / RenderScene 改 `m_MaterialProxy`；删 Material 公开 Bind | 场景可画；grep 门禁 |
| **S03** | 确认 scalar/texture Set 后画面正确；必要时补单测「Set scalar 不改错 BindingSet」 | 手验或测试 |
| **S04** | Registry/Progress → Done；可选 TD：双线程 Uniform cache | 文档 |

### 3.6 文件布局

**选用：** `Runtime/Function/Render/MaterialRenderProxy.h` + `.cpp`（与 `Material.h` 同级，少目录抖动）。  
若后续 Material 子目录膨胀，再挪到 `Render/Material/`。

### 3.7 寿命与栖息地（≠ SceneProxy）

**`MaterialRenderProxy` 不活在 `RenderScene` 里**，也不由 Scene 容器拥有。它和 **SceneProxy 是两类完全不同的「Proxy」**：

| | **Primitive SceneProxy** | **MaterialRenderProxy** |
|--|--------------------------|-------------------------|
| 表示谁 | 场景里的一个 **图元/组件**（网格、精灵…） | 一个 **材质对象** 的「渲染绑定视图」 |
| 谁创建/拥有 | 组件 `CreateSceneProxy()` → 通常挂进 RenderScene | **`Material` / 日后 `MaterialInstance` 拥有**（`unique_ptr`） |
| 寿命 | 随组件注册/注销、场景刷新 | 随材质对象（Asset / 运行时 Instance）同寿 |
| 在 Draw 中 | 提供 mesh/transform/… | 提供 shader 查询 + 参数绑定 |
| 对方关系 | SceneProxy **裸指针引用** `MaterialRenderProxy*`（不拥有） | 可被多个 SceneProxy / 多条 MDC 同时引用同一 Proxy |

```text
shared_ptr<Material>  (游戏/资产侧，可在组件上)
        │ owns
        ▼
DefaultMaterialRenderProxy   ←── 活在这里（材质对象内部）
        ▲
        │ raw ptr only（不拥有）
StaticMeshSceneProxy / MeshDrawCommand / RenderScene 遍历到的图元
        │
        ▼
   活在 RenderScene 的是 SceneProxy，不是 MaterialRenderProxy
```

对标 UE：`UMaterial::DefaultMaterialInstance` / `UMaterialInstance::Resource` 挂在 **UObject 材质上**；`FPrimitiveSceneProxy` 画的时候再去 `GetRenderProxy()`，并不是把 Material Proxy new 进 Scene。

| 指针 | 契约 |
|------|------|
| `Material::m_RenderProxy` | Material 析构时销毁；须先于仍在用的 SceneProxy/MDC 裸指针失效 |
| SceneProxy / MDC 中 `MaterialRenderProxy*` | **不拥有**；与今日 `Material*` 相同——换材质/卸材质时改写或清零；材质 Asset 释放前组件不得再 draw 该指针 |

无 UE `ENQUEUE_RENDER_COMMAND` 延迟删（单线程）。

### 3.8 为 F04 预留的契约（本 Feature 必须遵守）

1. Draw **永不**读覆盖表，只调 Proxy 虚函数。  
2. `GetGPUShader*` 始终来自 `GetBaseMaterial()`（Instance 不 Compile）。  
3. `GetMaterialShaderBindingSet()` 返回 **该 Proxy 自己的** set（Instance 将有自己的 cache）。  
4. 参数解析只通过 `Get*ParameterValue*` 虚函数链（Default 读 Material；Instance 读覆盖再 Parent Proxy）。

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. GPU BindingSet/UBO 迁 Proxy | F04 可独立覆盖贴图；对标 UE cache-on-proxy | F02 改动面略大 | **选用** |
| B. BindingSet 留 Material，Proxy 只转发 | F02 更小 | Instance 改贴图必污染 Parent 或 Draw 旁路 | **否决** |
| C. F02 同期 Instance Proxy | 一次竖切 | 缺 Interface；范围过大 | **否** → F04 |
| D. 先 Interface 后 Proxy | 组件先多态 | Draw 仍焊死 Material | **否决**（D0） |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| Compile 与 Proxy 缓存不同步 | 黑屏 / 错 layout | Compile 成功末尾必须 Invalidate；layout 变则重建 BindingSet |
| 迁 UBO/BindingSet 漏字段 | 链接错 / 运行崩溃 | S01 以「行为字节级等价」搬迁现有 Rebuild/Bind |
| SceneProxy 悬空 Proxy* | 崩溃 | 与现 Material* 相同清零契约 |
| `BindForDraw` 每帧写 UBO 的性能 | 可接受 | F02 保持现状；优化后置 |
| 双入口残留 | 回归 | S02 grep |

---

## 6) 验收标准

- [ ] `MaterialRenderProxy` + `DefaultMaterialRenderProxy`；`Material::GetRenderProxy()` 恒非空
- [ ] Scalar UBO / Material BindingSet / 纹理 SRV 缓存 **不**再作为 Draw 入口留在 Material 公有 API
- [ ] 生产 Draw（`SceneMeshDrawUtils`、Forward mesh/widget 提交）只使用 `MaterialRenderProxy*`
- [ ] 无公开 `Material::BindForDraw`
- [ ] Set scalar/texture 后下一帧正确
- [ ] 现网场景 / Editor 可画
- [ ] 无 Interface/Instance 类也可 Done
- [ ] §3.8 四条预留契约在代码中成立（可读 code review）

**验证建议：** `cmake --build build --target minEngineTests Editor`；相关测试；手验改材质参数。

---

## 7) Status note

**In Progress** — S01–S02 代码已落地；等维护者场景/Editor **手验**后标 Done。

**实现摘要：**
- `MaterialRenderProxy` / `DefaultMaterialRenderProxy`；GPU BindingSet/UBO/SRV 在 Proxy
- `Material::GetRenderProxy()`；Compile → `NotifyCompiled`
- MDC / SceneProxy / Draw utils / Forward 使用 `m_MaterialProxy`
- 验证：`material-ir` + `render-graph` PASSED（从 `minEngine/bin`）

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-18 | **实现：** S01–S02 代码；Status → In Progress（待手验） |
| 2026-09-18 | **澄清栖息地：** Proxy ≠ SceneProxy；由 Material/MI 拥有，不进 RenderScene（§3.7） |
| 2026-09-18 | **完善：** UE `FDefaultMaterialInstance` 对照；**GPU 缓存迁 Proxy**；参数虚查询；F04 预留契约；Compile↔Invalidate |
| 2026-09-18 | 升格为 F02 详设（Proxy 先行） |
| 2026-09-18 | （历史）曾为 F05 摘要 |
