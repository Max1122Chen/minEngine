# Material System — 深度拓展与使用优化

Last updated: 2026-05-23  
Status: **Phase 0–5 ✅**（维护模式）— 详见 [MATERIAL_SYSTEM_PHASE5.md](./MATERIAL_SYSTEM_PHASE5.md)  
前置：`MATERIAL_EDITOR_PLAN.md` E0–E4 ✅；`MATERIAL_SHADING_MODEL_PLAN.md`（Unlit + BlinnPhong）✅

---

## 0) 文档结构

| 章节 | 内容 |
|------|------|
| §1 | **已拍板决策**（你的 6 点 + 补充建议） |
| §2 | **疑问解答**（Normal vs AO、Translucent 与 TranslucencyPass） |
| §3 | **Phase 0 详细设计**（即将实现） |
| §4 | Phase 1 概要（详见 [MATERIAL_SYSTEM_PHASE1.md](./MATERIAL_SYSTEM_PHASE1.md)） |
| §5 | Phase 2 概要（详见 [MATERIAL_SYSTEM_PHASE2.md](./MATERIAL_SYSTEM_PHASE2.md)） |
| §6 | Phase 3+ 概要 |
| §7 | UE / 现代材质对照（保留作参考） |
| §8 | 参考与变更记录 |

---

## 1) 已拍板决策

| # | 议题 | 决定 |
|---|------|------|
| 1 | 预览球偏小 | **只改默认相机距离**，FOV 保持 45°；Editor **暂不提供** FOV/距离滑条 |
| 2 | Normal vs AO 顺序 | **先 Normal，后 AO**（见 §2.1） |
| 3 | MaterialOutput pin UI | **两层策略**（见 §2.2、Phase 1 概要） |
| 4 | Translucent | **资产 Blend Mode 与 TranslucencyPass 绑定**（见 §2.3）；P1 先 Opaque+Masked，Translucent 与 Pass 同一里程碑 |
| 5 | 节点扩充 | **少量即可**：`Lerp` + 放开 `Subtract`/`Divide`（及可选 `Min`/`Max`）；不碰 Custom/求导/大量新节点 |
| 6 | PBR vs BlinnPhong | **长期并存**，不迁移后删除 BlinnPhong |

**Pin 策略（定稿）：**

- **Shading Model 不支持的 property**（如 Unlit 下的 Metallic/Roughness）→ MaterialOutput 上 **不画 pin（隐藏）**。
- **Blend Mode 门控的 property**（如 Opaque 下暂时用不到的 Opacity，或 Masked 才强依赖的 Opacity）→ pin **可见但灰显、禁止连线**；提示「当前 Blend Mode 不可用，换 Mode 后可接」。
- **切换 Shading Model 或 Blend Mode** 时 → 对变为 **Hidden** 或 **Disabled** 的 pin，**自动断开**已有连接（`DisconnectInput` + 清 `NodeDef` 引用），并 `NotifyGraphChanged` + `MaterialGraphIds::Reset`（若 pin id 变化）。

---

## 2) 疑问解答

### 2.1 Normal 和 AO 哪个先做？（你看不出优劣时的建议）

| | **Normal（推荐先做）** | **AO（推荐后做）** |
|---|------------------------|-------------------|
| **解决什么问题** | 表面朝向、法线贴图凹凸、光照方向正确 | 缝隙变暗、间接光被挡 |
| **依赖** | 网格切线/副切线、shader 里 TBN 或简化扰动 | 至少有一条间接/环境项可乘 |
| **对 PBR** | **硬性前置**（无法谈 PBR 而不谈 Normal） | 重要但可第一版 PBR 先常数 1.0 |
| **对当前 BlinnPhong** | 立刻改善预览球「塑料感」 | 单方向光场景下 **视觉冲击较小** |
| **编辑器** | 需新 `MP_Normal` + 采样/Unpack 节点（可第二批节点） | 一个 float pin + Multiply 即可接入 |
| **实现量** | 中（属性 + 模板 + 可能 TBN） | 小（属性 + 模板乘一项） |

**建议路径：** P1 先 **`MP_Normal`**（枚举、Output pin、Capability、BlinnPhong 模板读切线法线）；P1 末或 Phase 2 再加 **`MP_AO`**。  
**Specular 色 pin：** BlinnPhong **不单独加** Specular float3；继续用 Metallic 作高光强度掩码 + Roughness 控锐度（与现有 `MATERIAL_SHADING_MODEL_PLAN` 一致），避免与 PBR 语义打架。

---

### 2.2 Pin「隐藏」vs「灰显」— 和 UE 的对应关系

UE Material 主节点大致是：

- 换 **Shading Model** 会 **隐藏** 一批用不到的输入（例如 Unlit 没有 Metallic）。
- 换 **Blend Mode** 会 **启用/禁用** Opacity、Opacity Mask 等（禁用时常灰显且不能连）。

我们拆成两类，避免「Unlit 下 Opacity 灰显」这种误导（Unlit 仍可能需要 Translucent 时的 Opacity，那是 **Blend** 维度）：

```text
MaterialPropertyPinVisibility:
  Hidden      // 当前 ShadingModel 根本不存在该语义 → 不画 pin
  Disabled    // 属性存在，但当前 BlendMode 不允许接 → 灰显 + 禁止 ConnectPins
  Active      // 可连线
```

**切换模型时断线：** 仅对从 `Active` 变为 `Hidden` 或 `Disabled` 的 pin 执行；`Disabled` 若将来变 `Active` **不**自动恢复旧连线（与 UE 一致，避免悄悄连回错误图）。

---

### 2.3 Translucent 要不要和 TranslucencyPass 绑定？

**要绑定，但是「渲染管线绑定」，不是「和 Shading Model 绑成一个 enum」。**

```text
Material 资产
  m_ShadingModel   → 怎么算光（Unlit / BlinnPhong / PBR）
  m_BlendMode      → 怎么合成（Opaque / Masked / Translucent）

RenderPipeline::BuildRenderQueue
  if (material->GetBlendMode() == Translucent)  → TranslucentQueue  → TranslucencyPass
  else if Masked                                 → OpaqueQueue + alpha test（BasePass）
  else                                           → OpaqueQueue（BasePass）
```

**现状：** `Material::IsTranslucent()` **恒为 `false`**，图材质全部进 `OpaqueQueue`；`TranslucencyPass` 已存在但未接图材质。

**结论：**

- **不应**把 Translucent 写成「某个 Shading Model 的子集」；Unlit + Translucent、PBR + Translucent 都合法。
- **应该**在实现 `MaterialBlendMode::Translucent` 时 **同时** 做：队列分流、`TranslucencyPass` 绑定、混合状态、Opacity 参与合成、编辑器 Disabled/Required 规则。
- **分期建议：** P1 先 **Opaque + Masked**（Masked 可 alpha test，仍走 BasePass）；**Translucent 与 TranslucencyPass 同一子里程碑**（避免「材质选了半透明却走不透明 Pass」）。

---

## 3) Phase 0 — 详细设计（即将做）

**目标：** 预览体感正常 + 非法连线不再崩溃。  
**预估：** 1–2 天量级（含自测）。  
**不在 P0：** 新 MaterialProperty、Blend Mode、Capability 表、新节点。

---

### P0-A — 预览相机默认构图

**文件：** `minEngine/minEngine/src/Runtime/Function/Render/MaterialPreviewViewport.cpp` → `SetupPreviewCamera()`

**改动（定稿数值，可微调 10%）：**

| 参数 | 现值 | 新值 | 说明 |
|------|------|------|------|
| `eye` | `(2.5, 1.5, 2.5)` | **`(1.15, 0.8, 1.15)`** | 距离约从 3.9 降到 1.7，球占屏更大 |
| `target` | `(0,0,0)` | 不变 | |
| `FOV` | `45°` | **不变** | 不在 Editor 暴露调节 |
| `zNear/zFar` | `0.1 / 200` | 不变 | |

**不动：** 球 `Scale(1,1,1)`、Preview 专用 light 角度（除非目视仍偏小再单独调 scale）。

**验收：**

- [x] Material 模式默认 512×512（或任意常见比例）Preview 窗，球 **明显更大**，无需用户缩放 Dock（2026-05-22 实现）
- [x] 改 RT 宽高比后 `SyncPreviewCameraAspect` 不拉伸变形（逻辑未改）
- [x] Scene 模式主视口无变化（仅改 Preview 相机）

---

### P0-B — 连线类型校验（防崩）

#### 3.1 根因

```text
MaterialGraphWindow::TryConnectPins
  → MaterialEdGraph::ConnectPins   // 无类型检查
  → BuildIR / emitter.Input          // 类型不对 → 断言/空指针/崩溃
```

#### 3.2 为何用 `MaterialValueType`（不用 `MIRPrimitiveType` 直接暴露在图 API）

| 点 | 说明 |
|----|------|
| **语义** | 类型描述的是「材质图 pin 上的值」，属于 Material/Editor 域；`MIR*` 是编译器 IR 域，不应成为 Editor 公共 API 的主语。 |
| **泛用** | 现有 MIR 已是 `MIRValueType` 基类：`float3`  primitive + **`Texture2D` object**（`TextureObject` → `TextureSample`）。若 P0 只写 `MIRPrimitiveType*`，纹理链要么漏校验要么强转，后续必改。 |
| **实现** | P0 **不新造一套 enum**；`MaterialValueType` = 对 canonical `const MIRValueType*` 的薄封装（typedef 或 `struct { const MIRValueType* MirType; }`），比较规则集中在 `AreMaterialValueTypesCompatible`。 |

```cpp
// MaterialValueType.h — 材质图 / 资产 / 编辑器 共用，不 include MIR 头也可前向声明 + 实现在 .cpp
using MaterialValueType = const MIRValueType*;

bool AreMaterialValueTypesCompatible(MaterialValueType from, MaterialValueType to);
const char* GetMaterialValueTypeDisplayName(MaterialValueType type);  // P0 可选："Float3","Texture2D"

MaterialValueType GetNodeOutputPinType(const MaterialGraphNodeDef* def, int32_t outputIndex);
MaterialValueType GetNodeInputPinType(const MaterialGraphNodeDef* def, int32_t inputIndex);
MaterialValueType GetMaterialPropertyValueType(MaterialProperty property);  // 包装 MaterialPropertyUtil
```

**兼容规则（P0）：**

| 规则 | 说明 |
|------|------|
| R0 | `from` / `to` 非空且非 `Poison` |
| R1 | 双方类型已知时 **Canonical 相等**（`from == to`） |
| R2 | 任一侧为 `nullptr`（Multiply 等多态 pin）→ **允许**（`AreConnectable`） |
| R3 | **不做** 隐式 cast |
| R4 | Output → Input 方向不变 |

实现：`MaterialValueTypeUtil` 类静态方法（非文件级 free 函数，符合 `hard-constraints.mdc`）。

#### 3.3 `MaterialEdGraph` 与 Editor

```cpp
bool CanConnectPins(..., std::string* outReason = nullptr) const;
bool ConnectPins(...);  // 入口：if (!CanConnectPins(...)) return false;
```

- **类型表：** `MaterialValueTypeRegistry.cpp`（或 `MaterialGraphPinTypes.cpp` 内实现）按 `MEClass` + pin 名登记；与 Palette 白名单同源维护。
- **Editor：** `TryConnectPins` → `CanConnectPins` → 失败 `RejectNewItem`。
- **加载：** `ValidateMaterialAsset` 对非法边报错，禁止进 BuildIR 崩溃。

**首批 pin 类型（示例）：**

| 节点 | 输出/输入 | MaterialValueType |
|------|-----------|-------------------|
| Constant / ScalarParameter | Value | `GetFloat()` |
| Constant3 / MakeFloat3 | Value / R,G,B | `GetFloat3()` / `GetFloat()` |
| Multiply, Add, … | A, B | `GetFloat()` |
| TextureCoordinate | UV | `GetFloat2()` |
| TextureObject | Texture | `GetTexture2D()` |
| TextureSample | Texture in | `GetTexture2D()`；UV `GetFloat2()`；RGBA `GetFloat4()`；RGB `GetFloat3()` |
| MaterialOutput | 各 pin | `GetMaterialPropertyValueType(MP_*)` |

#### 3.4 文件清单

| 操作 | 路径 |
|------|------|
| 新增 | `MaterialValueType.h/.cpp`（兼容函数 + pin 查询；实现文件可 include `MaterialIRTypes.h`） |
| 改 | `MaterialPropertyUtil`（可选：`GetMaterialPropertyValueType` 返回 `MaterialValueType`，内部仍用 primitive singleton） |
| 改 | `MaterialEdGraph.h/.cpp` |
| 改 | `MaterialGraphWindow.cpp` |
| 改 | `Material.cpp` / `ValidateMaterialAsset` |
| 可选 | `MaterialIRTest`：非法连接 `CanConnectPins == false` |

#### 3.5 验收

- [x] float 输出 → float3 输入（如 **Albedo** 口）拖线：**拒绝**，程序不崩（`CanConnectPins` + `AreConnectable`）
- [x] TextureObject(Texture2D) → TextureSample(Texture) **可连**；类型表已覆盖
- [x] float3 → float3（Albedo）可连；Smoke 图 + `--material-ir-test` exit 0
- [x] 拒绝时 UI 上连线不生成（`TryConnectPins` → `RejectNewItem`）
- [x] `Editor.exe --material-ir-test` exit 0（含 ObjectManager 测试 bootstrap）
- [ ] 现有 `MaterialIRSmoke.memtl` 打开、Compile、Save 无回归（需 Editor 目视）

---

### Phase 0 完成标准

```text
P0-A 验收通过
AND
P0-B 验收通过
→ 更新 PROGRESS_LOG，再开启 Phase 1 详细设计（§4 展开）
```

---

## 4) Phase 1 概要（✅ 已完成）

**完整设计：** [MATERIAL_SYSTEM_PHASE1.md](./MATERIAL_SYSTEM_PHASE1.md)（checklist 已全部勾选，2026-05-23）

**目标：** 属性与规则健全 + 少量节点；**仍不做 PBR、不做 Translucent**（Translucent 归 Phase 2）。

| 子阶段 | 状态 | 内容 |
|--------|------|------|
| **P1.1** | ✅ | `MaterialBlendMode`（Opaque/Masked）+ `MaterialCapabilityUtil` + Output pin Hidden/Disabled + `PruneInvalidMaterialOutputLinks` + Masked alpha test |
| **P1.2** | ✅ | `MP_Normal`（float3，**世界空间**；无 TBN）+ BlinnPhong 使用 |
| **P1.3** | ✅ | `MP_AO` + BlinnPhong 乘光照 |
| **P1.4** | ✅ | 新节点 `Lerp`；白名单 `Subtract`/`Divide`/`Min`/`Max` |

---

## 5) Phase 2 概要（**已完成**）

**完整设计：** [MATERIAL_SYSTEM_PHASE2.md](./MATERIAL_SYSTEM_PHASE2.md) ✅

**目标：** 半透明走对 Pass；法线贴图 + TBN；**不**做 PBR。

**下一步：** [MATERIAL_SYSTEM_PHASE3.md](./MATERIAL_SYSTEM_PHASE3.md) — PBR + ComponentMask。

| 子阶段 | 内容 |
|--------|------|
| **P2.1** | `MaterialBlendMode::Translucent` + `IsTranslucent()` + Capability/Opacity Required + fragment alpha + `TranslucencyPass` 验收 |
| **P2.2** | `a_Tangent` 写入网格 + `MaterialTangentFrame.glslinc` + `UsesTangentFrame` 编译开关 |
| **P2.3** | `MP_Normal` **切线空间** + `NormalUnpack` 节点 + BlinnPhong 扰动法线 |

**推荐顺序：** P2.1 → P2.2 → P2.3（Translucent 与 Normal 正交，先修管线语义）。

**已完成、不再计入 P2：** P1.1 `Masked` + `discard`（alpha test）。

---

## 6) Phase 3+ 概要

| 阶段 | 内容 |
|------|------|
| **Phase 3** ✅ | [MATERIAL_SYSTEM_PHASE3.md](./MATERIAL_SYSTEM_PHASE3.md)：PBR(GGX)、ComponentMask、TextureSample.R、`material-ir-test` |
| **Phase 4** ✅ | [MATERIAL_SYSTEM_PHASE4.md](./MATERIAL_SYSTEM_PHASE4.md)：**IBL**（HDR capture、split-sum、`CalcIndirectPBR`、Pass bind） |
| **Phase 5** ✅ | [MATERIAL_SYSTEM_PHASE5.md](./MATERIAL_SYSTEM_PHASE5.md)：irradiance / prefilter / SkyBoxComponent + SkyBoxPass |
| **Backlog** | Parallax/POM、WPO（用户排期）；Tessellation、Translucent IBL、Detail Normal |
| **Deferred** | 编辑器 Undo、Shader 预览、Content Browser、Material Function（用户更高优先级非材质项） |

---

## 7) UE / 现代材质对照（参考，不重复展开）

四个正交维度：**Shading Model**、**Blend Mode**、**Domain**、**Material Properties**。  
当前 minEngine：`Unlit`/`BlinnPhong`；Blend 未接；Properties 见 `MaterialTypes.h`（无 Normal/AO）。

详细表见初稿 §2.2–2.5；与 Epic [Physically Based Materials](https://dev.epicgames.com/documentation/en-us/unreal-engine/physically-based-materials-in-unreal-engine) 对照阅读即可。

---

## 8) 参考与变更记录

| 文档 | 用途 |
|------|------|
| [MATERIAL_EDITOR_PLAN.md](./MATERIAL_EDITOR_PLAN.md) | 编辑器 E0–E4 |
| [MATERIAL_SHADING_MODEL_PLAN.md](./MATERIAL_SHADING_MODEL_PLAN.md) | Unlit/BlinnPhong |
| [MATERIAL_RUNTIME_BRIDGE_CHECKLIST.md](./MATERIAL_RUNTIME_BRIDGE_CHECKLIST.md) | 进 Viewport |
| [PROGRESS_LOG.md](../../PROGRESS_LOG.md) | 时间线 |

| 日期 | 说明 |
|------|------|
| 2026-05-22 | 初稿 |
| 2026-05-22 | 拍板 §1、疑问 §2、**Phase 0 详细设计 §3**；P1+ 降为概要 |
| 2026-05-22 | P0-B：pin 类型 API 定为 **MaterialValueType**（`const MIRValueType*` 薄封装），非 `MIRPrimitiveType` 直出 |
| 2026-05-22 | Phase 0 实现提交 `9089671`；**MATERIAL_SYSTEM_PHASE1.md** 详细设计 |
| 2026-05-22 | P1.1 实现（BlendMode/Capability/Masked）；**MATERIAL_SYSTEM_PHASE2.md** 详细设计 |
| 2026-05-23 | **Phase 1 验收通过**（目视 + `--material-ir-test`）；启动 Phase 2 实现（P2.1） |
| 2026-05-23 | **Phase 3 验收通过**；[MATERIAL_SYSTEM_PHASE4.md](./MATERIAL_SYSTEM_PHASE4.md) IBL 设计定稿（Parallax/WPO/编辑器延后） |
| 2026-05-23 | **Phase 4 关门**；[MATERIAL_SYSTEM_PHASE5.md](./MATERIAL_SYSTEM_PHASE5.md) **仅 IBL**（POM/WPO → Backlog） |
