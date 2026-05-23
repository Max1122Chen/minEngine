# Material System — Phase 1 详细设计

Last updated: 2026-05-23  
Status: **✅ P1.1–P1.4 验收通过**（目视 + `--material-ir-test`）；可启动 Phase 2  
前置：[MATERIAL_SYSTEM_ROADMAP.md](./MATERIAL_SYSTEM_ROADMAP.md) Phase 0 ✅；[MATERIAL_SHADING_MODEL_PLAN.md](./MATERIAL_SHADING_MODEL_PLAN.md)

---

## 0) Phase 1 目标与边界

| 做 | 不做 |
|----|------|
| `MP_Normal`、`MP_AO` 与编译/模板接线 | `MaterialShadingModel::PBR`（Phase 3） |
| `MaterialBlendMode`：**Opaque + Masked**（渲染 + 编辑器） | **Translucent** + TranslucencyPass（Phase 2，与 Pass 绑定） |
| MaterialOutput **Capability**（Hidden / Disabled / Active） | Undo、Content Browser |
| 少量节点：`Lerp`；白名单放开 `Subtract`/`Divide`（可选 `Min`/`Max`） | Normal 贴图 / TBN / `NormalUnpack` 节点（Phase 2） |
| BlinnPhong 使用图 Normal + AO | Specular float3 pin |
| PBR 与 BlinnPhong **长期并存**（本阶段只铺属性，不新增 PBR） | Custom 节点、求导、大量新节点 |

**建议总工期（学习节奏）：** 约 1–2 周，按下面 **P1.1 → P1.4** 顺序交付、每步可单独验收。

---

## 1) 实施顺序（推荐）

```text
P1.1  BlendMode + Capability + Output pin UI + PruneLinks     ← 地基，与 Normal 正交
P1.2  MP_Normal（属性 + 编译 + BlinnPhong 使用，无 TBN）        ← 先 Normal
P1.3  MP_AO（属性 + BlinnPhong 环境项）                       ← 后 AO
P1.4  节点：Lerp 新建；Subtract/Divide/Min/Max 仅白名单        ← 可并行于 P1.2 末
```

**理由：** Capability 会改变 MaterialOutput 上画哪些 pin；应先于或随 `MP_Normal` 一起上线，避免「加了 Normal pin 但 Unlit 仍显示」。节点库独立，可放最后降低编译器回归面。

---

## 2) P1.1 — BlendMode + Capability（地基）

### 2.1 枚举与资产字段

**文件：** `MaterialCompileTypes.h`（或新建 `MaterialBlendTypes.h` 再被 Material 引用）

```cpp
ME_ENUM()
enum class MaterialBlendMode : uint8_t
{
    Opaque = 0,
    Masked,
    // Translucent — Phase 2（勿在 P1 半套接入）
};
```

**`Material.h`：**

```cpp
ME_PROPERTY()
MaterialBlendMode m_BlendMode = MaterialBlendMode::Opaque;
```

- `.memtl` 序列化随反射生成（`Material.gen.cpp` 重生成）。
- 默认 **Opaque**，旧资产无字段时反序列化为 Opaque。

### 2.2 Pin 可见性模型

```cpp
enum class MaterialPropertyPinVisibility : uint8_t
{
    Hidden,    // 不画 pin（当前 ShadingModel 无此语义）
    Disabled,  // 画 pin，灰显，禁止 ConnectPins / 不可拖线
    Active,
};
```

**查询 API（类静态方法，放 `MaterialCapability.h` / `MaterialCapability.cpp`）：**

```cpp
class MaterialCapabilityUtil
{
public:
    static MaterialPropertyPinVisibility GetPropertyPinVisibility(
        MaterialProperty property,
        MaterialShadingModel shadingModel,
        MaterialBlendMode blendMode);

    static bool IsPropertyRequiredAtCompile(
        MaterialProperty property,
        MaterialShadingModel shadingModel,
        MaterialBlendMode blendMode);

    // 给定材质当前设置，返回仍 Active 的 property 列表（Details 提示用）
};
```

### 2.3 Capability 表（P1 定稿）

**Shading 维度（Hidden）：**

| Property | Unlit | BlinnPhong |
|----------|-------|------------|
| Albedo | Active | Active |
| Emissive | Active | Active |
| Metallic | **Hidden** | Active |
| Roughness | **Hidden** | Active |
| Normal | **Hidden** | Active |
| AO | **Hidden** | Active（P1.3 后） |
| Opacity | 见 Blend 行 | 见 Blend 行 |
| WorldPositionOffset | P1 不暴露 Output pin（仍可在枚举/顶点阶段保留） |

**Blend 维度（对仍「存在」的 property）：**

| Property | Opaque | Masked |
|----------|--------|--------|
| Opacity | **Disabled** | **Active** + **Compile Required**（未接则用默认 1.0 或 Warn） |
| 其余 Active 属性 | Active | Active |

**说明：**

- Unlit + Masked：仍 **Hidden** Metallic/Roughness/Normal；Opacity **Active**（裁剪用）。
- Translucent（Phase 2）：Opacity **Required**；可能 Weak Normal 等另表。

### 2.4 编辑器行为

| 位置 | 行为 |
|------|------|
| `MaterialDetailsWindow` | 增加 **Blend Mode** Combo（Opaque / Masked） |
| `MaterialGraphWindow` 画 MaterialOutput 输入 | `GetPropertyPinVisibility` → Hidden 不 `BeginPin`；Disabled 画灰显圆点 + `ImGui::ItemHoverable` 禁止拖线 |
| `MaterialEdGraph::CanConnectPins` | 目标 pin 对应 property 非 Active → `false` + reason |
| `MaterialEditor::SetShadingModel` / `SetBlendMode` | 调用 `PruneInvalidMaterialOutputLinks(Material&)` |

**`PruneInvalidMaterialOutputLinks`（`MaterialEditor` 或 `MaterialEdGraph` 成员）：**

```text
对每个 MaterialOutput 节点、每个 inputIndex：
  visibility = GetPropertyPinVisibility(MP from pin name, material.m_ShadingModel, material.m_BlendMode)
  if visibility != Active && input.IsConnected():
      graph.DisconnectInput(edNode, inputIndex)
NotifyGraphChanged + InvalidateGraphCanvas + MaterialGraphIds::Reset（若 pin 集合变化）
```

**不自动恢复** Disabled→Active 时的旧连线（与 UE 一致）。

### 2.5 运行时 Masked（最小实现）

| 层 | 改动 |
|----|------|
| `Material` | `bool IsMasked() const`; `IsTranslucent()` 仍 `false`（Phase 2） |
| `RenderPipeline::BuildRenderQueue` | Masked 仍进 `OpaqueQueue`（与 Opaque 同 pass） |
| `BasePass` / 材质绑定 | Masked 时 `discard` 或 `alpha test`：`FragmentMaterialInputs.Opacity < 0.5`（阈值常量 `kMaskedClipThreshold = 0.5`，后续可做成材质参数） |
| Unlit / BlinnPhong 模板 | 在 `FragColor` 输出前增加 clip（仅当 compile 环境知道 `BlendMode==Masked`） |

**`MaterialCompileEnvironment` 扩展：**

```cpp
MaterialBlendMode BlendMode = MaterialBlendMode::Opaque;
```

`MaterialCompiler::Compile` 从 `Material` 传入；Assembler 在 Masked 时注入 `#define MATERIAL_MASKED 1` 或生成 `if (Opacity < threshold) discard;`。

### 2.6 P1.1 验收

- [x] Unlit：MaterialOutput **无** Metallic/Roughness/Normal/AO pin。
- [x] BlinnPhong：上述 pin **可见可连**（AO 在 P1.3 前可 Hidden 至 P1.3 完成）。
- [x] Opaque：Opacity pin **灰显**且无法连线。
- [x] Masked：Opacity **可连**；未接时 Compile Warn；alpha test 目视有裁剪。
- [x] Unlit → BlinnPhong：Metallic 上原有连线被 prune；BlinnPhong → Unlit 同理。
- [x] `--material-ir-test` 仍通过（Smoke 为 Unlit+Opaque）。

---

## 3) P1.2 — MP_Normal（无 TBN 第一版）

### 3.1 数据模型

**`MaterialTypes.h`：**

```cpp
enum MaterialProperty
{
    MP_Albedo = 0,
    MP_Metallic,
    MP_Roughness,
    MP_Emissive,
    MP_Opacity,
    // 插入在 MaterialShadingPropertyCount 之前
    MP_Normal,   // NEW — float3，切线/世界语义见下
    MaterialShadingPropertyCount,
    MP_WorldPositionOffset,
    MaterialPropCount,
};
```

**约定（P1 写死，Phase 2 再升级）：**

- 图里 `MP_Normal` 为 **世界空间法线**（WSN），长度不要求归一化，光照前 `normalize`。
- **不接** 时：BlinnPhong 用 `v_WorldNormal`（来自 `a_Normal` 变换），与当前一致。
- **不接** 时：Unlit **忽略** Normal（Capability 已 Hidden pin）。
- Phase 2：增加 `TextureSample` + TBN，可改为 **切线空间** 法线贴图。

**`MaterialPropertyUtil`：** 名称 `"Normal"`，类型 `GetFloat3()`，默认 `(0,0,1)` 在常量折叠里按需处理。

**`MaterialGraphNodeDef_MaterialOutput` 构造：** 在 `Roughness` 与 `Emissive` 之间或按字母插入 `Normal` input（**顺序变更会改变 pin index** → 旧 `.memtl` 需迁移或接受重连；建议在 PROGRESS_LOG 注明一次性格子重排）。

推荐 pin 顺序（与 UE 习惯接近）：

```text
Albedo, Normal, Metallic, Roughness, Emissive, Opacity
```

### 3.2 编译与模板

| 组件 | 改动 |
|------|------|
| `BuildFragmentMaterialInputsStructGlobal` | 自动包含 `MP_Normal`（循环 `MaterialShadingPropertyCount`） |
| `MIREmitter::SetMaterialOutput` / MIRBuilder | 已有 property 循环，无需特殊 case |
| `BlinnPhong.frag.template` | 光照使用 `N = normalize(FragmentMaterialInputs.Normal)` **若** 有连接；实现方式二选一： |
| | **A（推荐 P1）：** 始终写 `FragmentMaterialInputs.Normal`，未连接时 MIR 填 `v_WorldNormal`（需在 Builder 里对 Normal 做 default 到 varying） |
| | **B：** shader 里 `mix(v_WorldNormal, FragmentMaterialInputs.Normal, useCustomNormal)` — 更复杂 |

**P1 选 A：** `ResolveMaterialPropertyInput(MP_Normal)` 无连接时，编译期注入 **ExternalInput / 顶点 varying** 默认，与 Albedo 默认类似。

**`MaterialValueTypeUtil`：** `GetMaterialPropertyValueType(MP_Normal)` → `GetFloat3()`；MaterialOutput Normal input 登记。

### 3.3 编辑器

- Capability：BlinnPhong **Active**，Unlit **Hidden**（P1.1 表）。
- 无需新 Palette 节点即可验收：用 `Constant3` 接 Normal 做倾斜测试。

### 3.4 P1.2 验收

- [x] BlinnPhong + Constant3 接 Normal → Preview 高光方向随法线变化。
- [x] Unlit 不显示 Normal pin；切 BlinnPhong 后 pin 出现。
- [x] 非法 float → Normal 仍被 P0 `CanConnectPins` 拒绝。
- [x] `MaterialIRTest`：`VerifyConstant3ToNormalBlinnPhong` + BlinnPhong body 含 `FragmentMaterialInputs.Normal`。

**Phase 2 衔接：** `MaterialGraphNodeDef_TextureSampleNormal`、mesh `a_Tangent`、`TBN` glslinc。

---

## 4) P1.3 — MP_AO

### 4.1 数据模型

- `MP_AO`：`float`，范围 [0,1]，默认 `1.0`（不遮蔽）。
- 插入 `MaterialShadingPropertyCount` 前（例如 Normal 之后）：`..., MP_Normal, MP_AO, ...`。
- Capability：BlinnPhong **Active**，Unlit **Hidden**。

### 4.2 模板（BlinnPhong）

在 **环境光 / 间接项** 上乘 AO（与单方向光场景一致的第一版）：

```glsl
// 伪代码：diffuseAmbient *= FragmentMaterialInputs.AO;
// direct light 可乘也可不乘 — P1 建议 **全光照项** 乘 AO，简单可预期
litResult *= FragmentMaterialInputs.AO;
```

Unlit：不使用 AO（pin Hidden）；若将来 Unlit 要 AO 只影响自发光，另议。

### 4.3 P1.3 验收

- [x] ScalarParameter 接 AO=0.3 → 整体变暗（BlinnPhong Preview）。
- [x] 与 Normal 同时接：两项独立生效。
- [x] Opaque/Masked + Capability 仍正确。

---

## 5) P1.4 — 节点扩充

### 5.1 新建 `MaterialGraphNodeDef_Lerp`

| 项 | 内容 |
|----|------|
| Inputs | `A`、`B`（多态，`GetNodeInputPinType` → `nullptr`）、`Alpha`（`float`） |
| Output | `Value`（多态，`nullptr`） |
| BuildIR | `emitter.Lerp(alpha, a, b)` 或现有 `MIREmitter::Lerp` |
| Palette | `MaterialGraphNodeRegistry::RegisterCreatableNodes` |
| `MaterialValueTypeUtil` | 与 Multiply 相同：输入/输出 `nullptr`（AreConnectable 允许） |
| `DrawNode` | 可选：三个输入名 + Alpha `DragFloat`；或仅显示 "Lerp" |

### 5.2 仅白名单放开（已有 C++）

| 节点 | 操作 |
|------|------|
| `Subtract` | `GetCreatableNodes()` 增加一项 |
| `Divide` | 同上 |
| `Min` / `Max` | 可选，一并加入（实现量极低） |

**不改** `MaterialGraphNodeNefs.cpp` 主体逻辑。

### 5.3 P1.4 验收

- [x] Details Add Node 可建 Lerp；A/B 可接 float 或 float3（与 Multiply 一致）；Alpha 接 float。
- [x] Lerp → Albedo，Compile + Preview 正常。
- [x] Subtract/Divide 在图中可添加并编译（Smoke 图外单独小图测试即可）。

---

## 6) 文件清单（汇总）

| 操作 | 路径 |
|------|------|
| 新增 | `MaterialCapability.h/.cpp`（或 `MaterialPropertyCapability.*`） |
| 改 | `MaterialCompileTypes.h`、`Material.h`、反射生成 |
| 改 | `MaterialTypes.h`、`MaterialPropertyUtil.cpp` |
| 改 | `MaterialGraphNodeDefs/MaterialGraphNodeDef.h`、`MaterialGraphNodeNefs.cpp`（Output pins 顺序、Lerp） |
| 改 | `MaterialValueType.cpp`（Lerp、新 property types） |
| 改 | `MaterialCompiler` / `GLSLMaterialShellAssemblerImpl`（BlendMode、Masked clip、Normal/AO） |
| 改 | `BlinnPhong.frag.template`（+ 可能 `Unlit.frag.template` Masked） |
| 改 | `Material.cpp`（`IsMasked`、`Validate` Required） |
| 改 | `RenderPipeline.cpp` / `BasePass.cpp`（若需 per-draw blend 状态） |
| 改 | `MaterialEditor.cpp/.h`（`SetBlendMode`、`PruneInvalidMaterialOutputLinks`） |
| 改 | `MaterialDetailsWindow.cpp`（Blend Mode UI） |
| 改 | `MaterialGraphWindow.cpp`（Hidden/Disabled pins） |
| 改 | `MaterialGraphNodeRegistry.cpp` |
| 可选 | `MaterialIRTest.cpp`、金样例 `MaterialIRSmoke_BlinnPhong_Masked.memtl` |

---

## 7) 与 Phase 2 / Phase 3 的接口

| Phase 1 留下 | Phase 2 接 |
|--------------|------------|
| `MP_Normal` WSN 占位 | TBN + 法线贴图节点 |
| `MaterialBlendMode` 仅 Opaque/Masked | `Translucent` + `TranslucentQueue` + `IsTranslucent()` |
| Opacity Disabled@Opaque | Translucent Required + 混合状态 |
| Capability 框架 | 扩展表一行，不 rewrite |

| Phase 3 接 |
|------------|
| `MaterialShadingModel::PBR` + 新模板；复用 Normal/AO/Metallic/Roughness |
| Capability 表增加 PBR 列 |

---

## 8) 风险与对策

| 风险 | 对策 |
|------|------|
| MaterialOutput pin 重排破坏旧 `.memtl` | 文档 + 打开时 `Validate` Warn；或迁移脚本（非 P1 必须） |
| Masked clip 与透明排序混淆 | P1 不做 Translucent；命名区分 Masked vs Translucent |
| Normal WSN 与日后 TBN 语义变化 | `.memtl` 仅存图连接，语义写进 `MATERIAL_SHADING_MODEL_PLAN`  changelog |
| Capability 与 `CanConnectPins` 不一致 | 单一真源 `MaterialCapabilityUtil`；单测 visibility + connect 矩阵 |

---

## 9) Phase 1 完成标准

- [x] P1.1 验收通过
- [x] P1.2 验收通过
- [x] P1.3 验收通过
- [x] P1.4 验收通过
- [x] 已更新 [MATERIAL_SYSTEM_ROADMAP.md](./MATERIAL_SYSTEM_ROADMAP.md) §4、[PROGRESS_LOG.md](../../PROGRESS_LOG.md)
- [x] **下一步：** [MATERIAL_SYSTEM_PHASE2.md](./MATERIAL_SYSTEM_PHASE2.md) — 自 **P2.1 Translucent** 起实现

---

## 10) 参考

- [MATERIAL_SYSTEM_ROADMAP.md](./MATERIAL_SYSTEM_ROADMAP.md)
- [MATERIAL_SHADING_MODEL_PLAN.md](./MATERIAL_SHADING_MODEL_PLAN.md)
- [MATERIAL_EDITOR_PLAN.md](./MATERIAL_EDITOR_PLAN.md)

| 日期 | 说明 |
|------|------|
| 2026-05-22 | Phase 1 详细设计初稿 |
| 2026-05-23 | P1.1–P1.4 验收 checklist 全部勾选；Phase 1 关门 |
