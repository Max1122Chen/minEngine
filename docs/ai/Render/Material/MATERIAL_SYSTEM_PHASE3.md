# Material System — Phase 3 详细设计

Last updated: 2026-05-23  
Status: **✅ 验收通过（2026-05-23）** — 下一步 [MATERIAL_SYSTEM_PHASE4.md](./MATERIAL_SYSTEM_PHASE4.md)（**仅 IBL**）  
前置：[MATERIAL_SYSTEM_PHASE2.md](./MATERIAL_SYSTEM_PHASE2.md) ✅ 目视验收通过（用户确认）；[MATERIAL_SYSTEM_ROADMAP.md](./MATERIAL_SYSTEM_ROADMAP.md) §1 拍板（PBR 与 BlinnPhong **并存**）不变。

---

## 0) Phase 3 目标与边界

| 做 | 不做 |
|----|------|
| `MaterialShadingModel::PBR` + **GGX** 金属/粗糙度工作流 | 删除或改写 BlinnPhong（**并存**） |
| 复用 P2：**TBN**、`MP_Normal` TSN、`Translucent` Pass | **硬件 Tessellation**、Nanite、虚拟几何 |
| **ComponentMask** 进 Add Node（灰度 AO/Rough/Metallic 接线） | Parallax / POM（**延后**，见 PHASE4 §8） |
| `MP_Roughness` 在 PBR 模板中 **真正参与 BRDF** | Specular-Glossiness 工作流（除非拍板纳入） |
| Capability / Details **PBR 列** | IBL / 环境贴图（**Phase 4**，见 [PHASE4](./MATERIAL_SYSTEM_PHASE4.md)） |
| `--material-ir-test` PBR 子集 | **World Position Offset** 暴露与位移（**P4**） |
| 可选：BlinnPhong 补接 `Roughness`（小改，不挡 PBR） | ORM 打包贴图 **自动解包节点**（可 P3.4 或 P4） |

**建议总工期（学习节奏）：** 约 2–3 周，按 **P3.1 → P3.2 → P3.3 → P3.4** 交付。

---

## 1) 实施顺序（推荐）

```text
P3.1  图节点 + Capability 地基（ComponentMask、PBR 枚举、Details UI）
P3.2  GGX BRDF glslinc + PBR.vert/frag.template（复用 SceneLights/Shadows/TBN）
P3.3  编译器 / MIR / IR 测试 + marble 资产目视
P3.4  金样例 + 文档关门；可选 BlinnPhong Roughness 修补
```

**理由：**

1. **ComponentMask** 不依赖 PBR，但 PBR 验收（AO、单通道 rough）依赖它；与 P3.1 同批上线最省返工。
2. **BRDF 与模板** 是 P3 核心，放在节点/UI 之后可避免「能选 PBR 但编译不过」的中间态过久。
3. **位移 / 真凹凸** 属于渲染器 + 顶点域里程碑，与 PBR **正交**，单独 Phase（§11）。

---

## 2) 现状快照（P2 结束）

| 组件 | 现状 | P3 要改 |
|------|------|---------|
| `MaterialShadingModel` | `Unlit`, `BlinnPhong` | 增加 `PBR` |
| `MP_*` | Albedo/Normal/AO/Metallic/Roughness/Emissive/Opacity 已有 | PBR **重解释** Metallic/Roughness；Roughness 进 BRDF |
| `MP_WorldPositionOffset` | 枚举 + MIR 顶点占位；Output **Hidden** | P3 仍 Hidden；P4 暴露 |
| `ComponentMask` | 代码有，`BuildIR` 有 | **未进 Registry** → P3.1 注册 |
| `TextureSample` | 仅 RGBA / RGB 输出 | 可选增加 **R** 输出（与 Mask 二选一或都要） |
| `UsesTangentFrame` | `BlinnPhong` 恒 true | PBR 同规则（凡 PBR 即 TBN + `a_Tangent`） |
| `BlinnPhong.frag.template` | 用 Metallic 作 spec scale；**不用 Roughness** | 可选小修补；语义 **不与 PBR 统一** |
| `Shaders/PBR.*` | 空占位 | 由 **模板 + glslinc** 生成，非手写静态 shader |
| 贴图加载 | 1/3/4 通道 → R8/RGB8/RGBA8 ✅ | P3 文档注明 sRGB/linear 约定 |

---

## 3) P3.1 — 枚举、Capability、图节点

### 3.1 `MaterialShadingModel::PBR`

**`MaterialCompileTypes.h`：**

```cpp
enum class MaterialShadingModel : uint8_t
{
    Unlit = 0,
    BlinnPhong,
    PBR,   // NEW — 金属/粗糙度 GGX
};
```

- `.memtl` 默认仍为 `Unlit` / 现有 smoke；新样例 `m_ShadingModel: 2`（以反射序为准）。
- `MaterialDetailsWindow`：Shading Model 下拉增加 **PBR**。
- `MaterialEditor::SetShadingModel` → `PruneInvalidMaterialOutputLinks`（已有）。

### 3.2 Capability 表（PBR 列）

| Property | Unlit | BlinnPhong | **PBR** |
|----------|-------|------------|---------|
| Albedo | Active | Active | Active |
| Normal | Hidden | Active | **Active** |
| AO | Hidden | Active | **Active** |
| Metallic | Hidden | Active | **Active** |
| Roughness | Hidden | Active | **Active** |
| Emissive | Active | Active | Active |
| Opacity | Blend 门控 | 同左 | 同左 |
| WorldPositionOffset | Hidden | Hidden | Hidden（P4） |

Blend 维（Opaque / Masked / Translucent）与 P2 一致；**PBR + Translucent** 合法。

### 3.3 ComponentMask（并入 P3）

**节点：** `MaterialGraphNodeDef_ComponentMask`（已存在）

| 方案 | 说明 | 推荐 |
|------|------|------|
| A | 单节点 + 反射字段 `ChannelIndex`（0=R…3=A） | ✅ 省调色板条目 |
| B | 四个节点 `MaskR` / `MaskG` / `MaskB` / `MaskA` | UE 风格，palette 臃肿 |

**接线：**

```text
TextureSample(AO, slot N).RGB ──→ ComponentMask(R) ──→ MaterialOutput.AO
TextureSample(Rough, slot M).RGB ──→ ComponentMask(R) ──→ MaterialOutput.Roughness
```

**注册：** `MaterialGraphNodeRegistry::RegisterCreatableNodes` 增加 ComponentMask。

**可选增强（非必须）：** `TextureSample` 增加输出 pin **R**（`float`），少一跳 Mask；与 ComponentMask **可并存**。

### 3.4 P3.1 验收

- [x] Details 可选 PBR；Unlit 下 Metallic/Roughness/Normal/AO 仍 Hidden。
- [x] Add Node 可创建 ComponentMask；`float3/float4 → float` 可接到 AO / Roughness / Metallic。
- [x] 切换 ShadingModel 时非法 pin 自动断线。

---

## 4) P3.2 — GGX 模板与光照

### 4.1 新建 `MaterialPBR.glslinc`

职责（参考 Epic PBR 与 LearnOpenGL PBR）：

- `DistributionGGX(N, H, roughness)`
- `GeometrySmith`（Schlick-GGX G）
- `FresnelSchlick`（F0 由 `metallic` + `albedo` 混合）
- `CalcDirLightPBR` / `CalcPointLightPBR` / `CalcSpotLightPBR` — **签名与现有 `MaterialPhongLighting.glslinc` 平行**，内部换 BRDF，**阴影调用复用** `MaterialSceneShadows.glslinc`

**F0 规则（金属/粗糙度工作流）：**

```glsl
vec3 F0 = vec3(0.04);
F0 = mix(F0, albedo, metallic);
```

**Roughness：** 使用 `FragmentMaterialInputs.Roughness`（可 `max(roughness, 0.04)` 防除零）。

**AO：** `Lo *= ao` 或 `(ambient + Lo) * ao` — **待拍板**（§9 #3）；首版推荐 **仅调制间接/环境项**，直接光不乘 AO（与 UE 常见做法一致）。

### 4.2 模板 `PBR.vert.template` / `PBR.frag.template`

- **顶点：** 与 `BlinnPhong.vert.template` 同壳 — `MeshVertexLightingVaryings` + **TBN varyings** + `u_LightViewProj`。
- **片段：**
  - include `MaterialPBR.glslinc` + `MaterialSceneShadows.glslinc`
  - `N` = `BuildWorldNormalFromTangentSpace(..., FragmentMaterialInputs.Normal)`（与 BlinnPhong P2 一致）
  - 累加 dir/point/spot **PBR** 项 × shadow
  - `FragColor = vec4(rgb + Emissive, Opacity)`；Masked/Translucent 锚点与 P2 共用

### 4.3 编译环境

```cpp
// MaterialCompiler.cpp
env.UsesTangentFrame = (shadingModel == BlinnPhong || shadingModel == PBR);
```

Assembler：`ShadingModel == PBR` → 选 PBR 模板路径；fragment struct 声明 Metallic/Roughness/AO/Normal/Albedo。

### 4.4 与 BlinnPhong 语义隔离

| 输入 | BlinnPhong（保持） | PBR（新） |
|------|-------------------|-----------|
| Metallic | `MaterialSpecularFromMetallic` 标量扩 RGB | 金属度 0–1，改 F0 |
| Roughness | （当前未用） | GGX α = roughness² |
| Normal | TSN + TBN | 同左 |

**文档 + 注释** 写清：同一 pin 名，**不同 ShadingModel 不同含义**（ROADMAP 已拍板并存）。

### 4.5 P3.2 验收

- [x] 编译 PBR 材质生成 `DistributionGGX`（或项目命名）关键字。
- [x] Preview 球 PBR + 常数 Metallic/Roughness 目视有金属/漫反射差异。
- [x] 阴影仍生效（dir/point/spot 至少方向光）。

---

## 5) P3.3 — 测试与 marble 工作流

### 5.1 图 recipe（`MyMEProject` 已有贴图）

| Slot | 贴图 | 接法 |
|------|------|------|
| 0 | `marble_cliff_03_diff_1k.jpg` | → Albedo |
| 1 | `marble_cliff_03_nor_gl_1k.jpg` | → NormalUnpack → Normal |
| 2 | `marble_cliff_03_rough_1k.jpg` | → ComponentMask(R) → Roughness |
| 3 | `marble_cliff_03_ao_1k.jpg` | → ComponentMask(R) → AO |

Metallic：常数 `0`（非金属大理石）或 ScalarParameter。

### 5.2 `--material-ir-test`

新增/扩展：

- `CompileForDiagnostics(..., ShadingModel=PBR, BlinnPhong, blend=Opaque)`
- 断言 fragment 含 GGX 符号、`FragmentMaterialInputs.Roughness`、`u_Texture2` 等
- 可选：与 `VerifyNormalMapWorkflow` 类似的 `VerifyPBRMarbleWorkflow`

### 5.3 sRGB / Linear（文档 + 后续）

P3 **不强制** 引擎侧 sRGB 纹理类型；在 PROGRESS_LOG 注明：

- Albedo：`sRGB` 采样（若未来 `Texture2D` 带 gamma 标记再实现）
- Normal / Rough / AO：`linear`

首版目视可接受；**拍板**是否 P3 加 `Texture2D::ColorSpace` 元数据（§9 #6）。

### 5.4 P3.3 验收

- [x] marble 四张贴图 PBR Preview 目视合理（法线、暗缝 AO、粗糙度高光展宽）。
- [x] `--material-ir-test` 全绿（含 `VerifyPBRWorkflow`）。

---

## 6) P3.4 — 金样例与关门

| 资产 | 内容 | 状态 |
|------|------|------|
| `MaterialIRSmoke_PBR.memtl` | PBR + Opaque + marble slot 布局 | 可选，未做（目视 + IR 测试已覆盖） |
| `MaterialIRSmoke_PBR_Translucent.memtl` | PBR + Translucent | 可选，未做（P3 拍板 7A） |

文档：

- [x] [MATERIAL_SYSTEM_ROADMAP.md](./MATERIAL_SYSTEM_ROADMAP.md) §6 → Phase 3 ✅
- [x] [PROGRESS_LOG.md](../../PROGRESS_LOG.md)
- [x] [MATERIAL_SYSTEM_PHASE4.md](./MATERIAL_SYSTEM_PHASE4.md) IBL 设计稿

---

## 7) 文件清单（汇总）

| 操作 | 路径 |
|------|------|
| 改 | `MaterialCompileTypes.h`（`PBR` 枚举） |
| 改 | `MaterialCapability.cpp`、`MaterialDetailsWindow.cpp` |
| 改 | `MaterialGraphNodeRegistry.cpp`（ComponentMask） |
| 改 | `MaterialCompiler.cpp`、`GLSLMaterialShellAssemblerImpl.cpp` |
| 新增 | `Assets/EngineDefault/Shaders/Include/GLSL/MaterialPBR.glslinc` |
| 新增 | `Assets/EngineDefault/Shaders/Template/GLSL/PBR.vert.template`、`PBR.frag.template` |
| 改 | `MaterialIRTest.cpp` |
| 新增 | `MyMEProject/Assets/Materials/MaterialIRSmoke_PBR.memtl` |
| 可选 | `MaterialGraphNodeDef_TextureSample` 增加 R 输出；`BlinnPhong.frag.template` 读 Roughness |

---

## 8) 风险与对策

| 风险 | 对策 |
|------|------|
| Metallic 双语义混淆 | UI 文案 + 文档；BlinnPhong 工具提示「非 PBR 金属度」 |
| 低模 + 法线过强 | 艺术家调 strength；P4 再加 NormalStrength 节点 |
| 单通道贴图误接 RGB→float | ComponentMask + 类型检查已有 |
| PBR 过亮/能量不守恒 | 首版不要求严格能量守恒；后续加 π 除项与 IBL |
| Translucent PBR 排序 | 与 P2 相同限制；文档写明 |

---

## 9) Phase 3 完成标准

- [x] P3.1 ComponentMask + PBR 枚举/UI/Capability 验收
- [x] P3.2 GGX 模板 + 阴影 验收
- [x] P3.3 marble 目视 + `material-ir-test` 通过
- [x] P3.4 文档关门（金样例 `.memtl` 为可选 backlog）

**下一步：** [MATERIAL_SYSTEM_PHASE4.md](./MATERIAL_SYSTEM_PHASE4.md) — **IBL only**（Parallax / WPO / 编辑器 Undo 延后）。

---

## 10) 需你拍板（默认推荐标 ✅）

| # | 议题 | 选项 | 推荐 |
|---|------|------|------|
| 1 | **BRDF 范围** | A) 仅直接光 GGX；B) + 常数 ambient；C) + IBL 立方体贴图 | **A**（P3）；C 放 P4 |
| 2 | **工作流** | A) 仅 Metallic-Roughness；B) 再加 Specular-Glossiness | **A** |
| 3 | **AO 用法** | A) 只乘环境/间接；B) 乘最终颜色 | **A** |
| 4 | **ComponentMask** | A) 单节点 + ChannelIndex；B) 四节点 R/G/B/A | **A** |
| 5 | **TextureSample.R** | A) P3 不加，仅 Mask；B) 同时加 R 输出 pin | **B**（实现量小，AO 更顺） |
| 6 | **sRGB** | A) P3 仅文档约定；B) P3 做 Texture 元数据 + shader 分支 | **A** |
| 7 | **PBR + Translucent** | A) P3 测 Opaque 即可；B) P3 必测 Translucent | **A**（组合测试放 P3.4 可选） |
| 8 | **BlinnPhong Roughness** | A) P3 顺手接到 Phong 高光锐度；B) 永久遗留 | **A**（小改，减少困惑） |
| 9 | **默认 Preview 材质** | A) 继续 IRSmoke；B) 新项目默认 PBR marble | **A** |

回复示例：`1A 2A 3A 4A 5B 6A 7A 8A 9A` 或逐条改选。

---

## 11) 位移贴图与「真凹凸」— 延后说明

> Parallax / WPO / Tessellation **不在 Phase 4**。Phase 4 仅 IBL。下列时间表供后续 Backlog 参考。

先区分概念（避免与 P2 **法线贴图** 混淆）：

| 技术 | 效果 | 是否改几何 | minEngine 计划阶段 |
|------|------|------------|-------------------|
| **Normal map**（P2 ✅） | 光照凹凸 | 否 | **现在** |
| **Parallax / POM** | 视差假深度、 silhoutte 仍平 | 否 | **P5 / Backlog** |
| **WPO 高度偏移** | 顶点沿法线推出 | 是（低模上易裂） | **P5 / Backlog** |
| **Tessellation + Displacement** | 细分后面片位移 | 是（真几何） | **P5+**（需 hull/domain shader、阴影/碰撞同步） |
| **离线高模** | 烘焙 Normal/Height | 导入侧 | 随时（DCC） |

**结论（直接回答）：**

- **「看起来鼓起来」且不改网格：** P2 法线贴图 = **现在**；更强一点 = **P4 Parallax**（快，仍非真几何）。
- **「网格真的鼓起来」：** 最早 **P4 WPO**（中模 + 高度图，学习成本低）；**真·Displacement map 驱动细分** = **P5+**，要等渲染管线支持 tessellation 并改 ShadowPass/Preview mesh。

**依赖链（更新 2026-05-23）：**

```text
P3 PBR ✅
  → P4 IBL（环境 cubemap + split-sum）
  → P5+ Parallax / WPO / Tessellation（按优先级另排）
```

---

## 12) 参考

- [MATERIAL_SYSTEM_PHASE2.md](./MATERIAL_SYSTEM_PHASE2.md)
- [MATERIAL_SHADING_MODEL_PLAN.md](./MATERIAL_SHADING_MODEL_PLAN.md)
- Epic: [Physically Based Materials](https://dev.epicgames.com/documentation/en-us/unreal-engine/physically-based-materials-in-unreal-engine)
- LearnOpenGL: [PBR Theory](https://learnopengl.com/PBR/Theory)
