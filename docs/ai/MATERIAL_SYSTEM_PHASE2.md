# Material System — Phase 2 详细设计

Last updated: 2026-05-23  
Status: **✅ 验收通过（2026-05-23）** — 启动 [MATERIAL_SYSTEM_PHASE3.md](./MATERIAL_SYSTEM_PHASE3.md)  
前置：[MATERIAL_SYSTEM_PHASE1.md](./MATERIAL_SYSTEM_PHASE1.md) ✅ 验收通过（2026-05-23）；[MATERIAL_SYSTEM_ROADMAP.md](./MATERIAL_SYSTEM_ROADMAP.md) §1 拍板决策不变。

---

## 0) Phase 2 目标与边界

| 做 | 不做 |
|----|------|
| `MaterialBlendMode::Translucent` **与 `TranslucencyPass` 绑定**（队列 + 混合 + Opacity 语义） | `MaterialShadingModel::PBR`（Phase 3） |
| 网格 **切线** 数据通路 + shader **TBN** | 各向异性、Clear Coat、Subsurface |
| 法线贴图工作流：`MP_Normal` **切线空间** + 图节点（Unpack / 采样） | Parallax、Detail Normal、Triplanar |
| Capability / 编辑器 Blend Mode 三档齐全 | Undo、Material Function、Shader 文本预览 |
| Unlit / BlinnPhong **均支持** Translucent + Normal map | 重写 BlinnPhong 为 PBR |

**不在 Phase 2（已在 Phase 1 完成）：**

- `Masked` + `discard` / alpha test（P1.1 ✅）
- `MaterialCapabilityUtil` 框架（P1.1 ✅，本阶段 **扩展表** 即可）

**建议总工期（学习节奏）：** 约 2–3 周，按 **P2.1 → P2.2 → P2.3** 交付，每步可单独验收。

---

## 1) 实施顺序（推荐）

```text
P2.1  Translucent + TranslucencyPass + IsTranslucent() + Opacity 编译/合成   ← 先堵住管线「假半透明」
P2.2  网格 a_Tangent + TBN glslinc + 编译环境 UsesTangentFrame              ← 法线贴图的地基
P2.3  MP_Normal 切线语义 + Normal 图节点 + BlinnPhong 扰动光照                ← 依赖 P1.2 的 MP_Normal pin
```

**理由：**

1. **Translucent** 与 Shading/Normal **正交**；尽早让 `BuildRenderQueue` 与 `IsTranslucent()` 说真话，避免「资产选了半透明仍走 BasePass」。
2. **TBN** 动网格布局与顶点声明，面大于单节点；完成后再接 Normal 贴图，避免「图能连、mesh 没切线」。
3. **P1.2 `MP_Normal`（WSN）** 应在 Phase 1 完成；P2.3 把语义 **升级为切线空间** 并接贴图。若 P1.2 未做，P2.3 合并实现 pin + TBN（见 §6.1）。

---

## 2) 现状快照（设计依据）

| 组件 | 现状 | Phase 2 要改 |
|------|------|----------------|
| `Material::IsTranslucent()` | 恒 `false` | 随 `m_BlendMode == Translucent` |
| `RenderPipeline::BuildRenderQueue` | 已按 `IsTranslucent()` 分流 | 无需改逻辑，依赖上项 |
| `TranslucencyPass` | 已 `EnableBlend`、深度写入关、**从远到近**排序 | 确认图材质 shader 输出 **alpha**；可选 `DisableCull` |
| `MaterialBlendMode` | `Opaque` / `Masked`（P1.1） | 增加 `Translucent` |
| `MP_Normal` / `MP_AO` | Phase 1 计划项，代码可能未全上 | P2.3 依赖 Normal pin；AO 仍归 P1.3，不阻塞 P2 |
| 网格顶点 | `a_Position`, `a_TexCoord`, `a_Normal`（Assimp 已 `CalcTangentSpace` 但未写入） | 增加 `a_Tangent`（`vec4`，`w` 为 handedness） |
| `MP_Normal` 语义（P1 设计） | 世界空间 WSN | P2 改为 **切线空间**（TSN），默认 `(0,0,1)` |
| BlinnPhong 光照 | `normalize(v_WorldNormal)` | 有 TSN 时 `normalize(TBN * perturbNormal)` |

---

## 3) P2.1 — Translucent + TranslucencyPass

### 3.1 枚举与运行时

**`MaterialCompileTypes.h`：**

```cpp
ME_ENUM()
enum class MaterialBlendMode : uint8_t
{
    Opaque = 0,
    Masked,
    Translucent,  // NEW
};
```

**`Material.h`：**

```cpp
bool IsTranslucent() const { return m_BlendMode == MaterialBlendMode::Translucent; }
bool IsMasked() const { return m_BlendMode == MaterialBlendMode::Masked; }
// Opaque：两者皆 false
```

**队列（已存在，验收即可）：**

```text
IsTranslucent()  → ctx.TranslucentQueue → TranslucencyPass（在 BasePass 之后）
否则             → ctx.OpaqueQueue      → BasePass（Opaque + Masked）
```

### 3.2 Capability 表（Blend 维扩展）

| Property | Opaque | Masked | Translucent |
|----------|--------|--------|-------------|
| Opacity | **Disabled** | **Active** + compile warn 若未接 | **Active** + **Required**（未接 warn，默认 1.0） |
| 其余（Albedo、Normal、Metallic…） | 随 Shading 表 | 同左 | 同左 |

**Shading 维不变：** Unlit 仍 Hidden Metallic/Roughness/Normal（P1 Capability）；Translucent **不**改变 Shading 隐藏规则。

**切换 Blend Mode：** 继续走 `MaterialEditor::SetBlendMode` → `PruneInvalidMaterialOutputLinks`（P1.1 已有）。

### 3.3 编译与 Shader

**`MaterialCompileEnvironment`：** 已有 `BlendMode`；Assembler 分支：

| BlendMode | Fragment 行为 |
|-----------|----------------|
| Opaque | 无 `discard`；`FragColor.a = Opacity`（可恒 1） |
| Masked | `Opacity < kMaskedClipThreshold` → `discard`（P1.1 ✅） |
| Translucent | **无** `discard`；`FragColor.a = FragmentMaterialInputs.Opacity` |

**模板锚点（两档 Shading 共用）：**

- 保持 `FRAGMENT_MASKED_CLIP`（仅 Masked 注入）。
- 新增可选 `FRAGMENT_BLEND_ALPHA_OUTPUT` 或直接在模板写死：`FragColor = vec4(rgb, FragmentMaterialInputs.Opacity)`（Unlit / BlinnPhong 各自 rgb 计算后统一）。

**Unlit + Translucent：** `rgb = Albedo + Emissive`，**仍不参与** 场景光；alpha 参与混合。

**BlinnPhong + Translucent：** 光照结果 + Emissive，alpha 来自 Opacity；**仍走** `TranslucencyPass`（深度写入关、排序）。

### 3.4 渲染状态（Pass 层，非材质）

`TranslucencyPass` 现状：

- `EnableBlend()` → 默认 `GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA`（`OpenGLRHI`）
- `SetDepthMask(false)`

**P2.1 可选增强（非必须）：**

- 绘制前 `DisableCull()`，绘制后恢复（双面半透明预览）。
- 文档注明：不支持折射、不支持独立透明阴影。

**不做：** 按材质切换 blend 方程（Phase 2 统一 alpha blend）。

### 3.5 编辑器

| 位置 | 行为 |
|------|------|
| `MaterialDetailsWindow` | Blend Mode 增加 **Translucent** |
| `MaterialGraphWindow` | Opacity pin：Opaque 灰显；Masked/Translucent Active |
| Compile | Translucent 且 Opacity 未接 → **Warning**（与 Masked 同级） |

### 3.6 资产与测试

| 项 | 内容 |
|----|------|
| 金样例 | 可选 `MaterialIRSmoke_Translucent.memtl`（Unlit 或 BlinnPhong + 常数 Opacity &lt; 1） |
| 更新 | `MaterialIRSmoke.memtl` 保持 `m_BlendMode: 0`；新样例 `m_BlendMode: 2` |
| 测试 | `--material-ir-test`：编译 Translucent 环境 → fragment **无** `discard`，含 `FragColor` alpha；on-disk 字段检查 |

### 3.7 P2.1 验收

- [ ] Details 选 Translucent → Opacity **可连**；Opaque 下仍灰显。
- [ ] Preview 中半透明球/板 **透过** 看到背景（同一 TranslucencyPass）。
- [ ] `BuildRenderQueue`：Translucent 材质 **不进** `OpaqueQueue`（日志或断点）。
- [ ] Masked 仍 `discard`，与 Translucent 互不干扰。
- [ ] `--material-ir-test` 通过（含 Translucent 编译子集）。

---

## 4) P2.2 — 切线数据 + TBN

### 4.1 网格顶点契约

**标准布局（与 Assimp `aiProcess_CalcTangentSpace` 对齐）：**

```text
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;
layout(location = 2) in vec3 a_Normal;
layout(location = 3) in vec4 a_Tangent;   // xyz = tangent, w = sign for bitangent handedness
```

**C++ `AssetManager` 加载 StaticMesh：**

- 扩展 `Vertex` 结构，写入 `mesh->mTangents`、handedness（由 `mNormals` × `mTangents` 与副切线方向推导，或 Assimp 4.1+ `mTangentW`）。
- `CreateVertexDefinition` 增加 `a_Tangent` → `Float4`。

**引擎内置网格（Preview 球、EngineDefault 等）：**

- 凡走 BlinnPhong 且需 Preview 法线贴图，必须 **补切线**（程序化生成或重新导出）。
- **验收最低线：** Preview 球 + `MaterialIRSmoke` 使用的 mesh 具备 `a_Tangent`。

### 4.2 Shader：`MaterialTangentFrame.glslinc`（新建）

职责：

```glsl
// 输入：世界空间 T, N（来自 varyings），可选 TSN（切线空间法线，已 Unpack）
// 输出：世界空间扰动后法线
vec3 BuildWorldNormalFromTangentSpace(vec3 worldTangent, vec3 worldNormal, float tangentSign, vec3 tangentSpaceNormal);
```

实现要点：

- `bitangent = cross(worldNormal, worldTangent) * tangentSign`
- `mat3 TBN = mat3(normalize(worldTangent), normalize(bitangent), normalize(worldNormal))`
- `return normalize(TBN * tangentSpaceNormal)`

**顶点 varying 扩展：**

- `v_WorldTangent`（`vec3`）+ `v_TangentSign`（`float`），或合并 `vec4 v_WorldTangent4`
- 在 `MeshVertexLightingVaryings.glslinc` 或材质壳 `VERTEX_LIGHTING_VARYINGS` 中追加

### 4.3 编译期开关 `UsesTangentFrame`

**`MaterialCompileEnvironment`：**

```cpp
bool UsesTangentFrame = false;
```

**置 true 条件（P2 推荐）：**

- `ShadingModel == BlinnPhong`（或将来 PBR），且
- `MP_Normal` 在 MIR 中 **非默认**（图有连线，或贴图节点贡献），**或** 全局 Phase 2 简化：**凡 BlinnPhong 即启用 TBN**（实现快，但 Unlit 不加切线 attribute）。

**推荐（可维护）：** MIRBuilder / 编译诊断阶段扫描 `SetMaterialOutput(MP_Normal)` 的 arg 是否为「非顶点默认」；是则 `UsesTangentFrame = true`。

**Assembler 分支：**

| UsesTangentFrame | Vertex IO | Fragment |
|------------------|-----------|----------|
| false | 仅 `a_Normal`（与现 P1 一致） | `N = normalize(v_WorldNormal)` 或 `FragmentMaterialInputs.Normal`（WSN） |
| true | `+ a_Tangent` | `N = BuildWorldNormalFromTangentSpace(..., FragmentMaterialInputs.Normal)` |

**与 P1.2 WSN 迁移：**

- P1：`MP_Normal` = 世界空间向量。
- P2：`MP_Normal` = **切线空间**；未接贴图时默认 `vec3(0,0,1)` → 等价于几何法线。
- **旧 `.memtl`：** 若 P1 曾用 Constant3 接 WSN 法线，升级后语义变化，需在 PROGRESS_LOG **一次性** 说明；打开资产可 Warn「Normal 现为切线空间」。

### 4.4 P2.2 验收

- [x] `AssetManager` 加载 `a_Tangent`（Assimp + fallback）；顶点布局 `Float4`。
- [x] BlinnPhong：`UsesTangentFrame=true`，vertex `a_Tangent`，fragment `BuildWorldNormalFromTangentSpace`。
- [x] 默认 `MP_Normal` = TSN `(0,0,1)`；`--material-ir-test` 更新。
- [ ] 目视：不接 Normal 贴图时与 P1 几何法线一致；Preview 球切线无 NaN。

---

## 5) P2.3 — 法线贴图工作流

### 5.1 图节点（最小集）

| 节点 | 类型 | 说明 |
|------|------|------|
| **保留** `TextureSample` | RGB → float3 | 采样 BC5/BC1 法线贴图 |
| **新建** `NormalUnpack`（名可调整） | float3 → float3 | `rgb * 2.0 - 1.0`，可选 `normalize` |

**不新建** 独立 `TextureSampleNormal` 资产类型（Phase 2）；用 **TextureObject + TextureSample + NormalUnpack → MaterialOutput.Normal** 即可。

**Palette：** `NormalUnpack` 进 `GetCreatableNodes()` ✅

**法线贴图 recipe（Editor）：**

```text
TextureCoordinate.UV ─┬─→ TextureSample(AlbedoTex).RGB ──→ MaterialOutput.Albedo
                      └─→ TextureSample(NormalTex).RGB ──→ NormalUnpack ──→ MaterialOutput.Normal
```

- 两个 `TextureObject`：slot 0 = 反照率，slot 1 = 法线贴图（linear RGB，非 sRGB 更佳）
- Shading = **BlinnPhong**；mesh 需有 `a_Tangent`（P2.2）

### 5.2 `MP_Normal` 与 MaterialOutput

- Pin 顺序（与 P1 一致）：`Albedo, Normal, Metallic, Roughness, Emissive, Opacity`。
- Capability：BlinnPhong **Active**；Unlit **Hidden**；PBR（Phase 3）**Active**。
- `GetMaterialPropertyType(MP_Normal)` → `float3`。
- **默认值：** 编译器折叠为 `vec3(0,0,1)`（切线空间 +Z）。

### 5.3 BlinnPhong 模板

```glsl
vec3 N = /* UsesTangentFrame */
    ? BuildWorldNormalFromTangentSpace(v_WorldTangent, v_WorldNormal, v_TangentSign, FragmentMaterialInputs.Normal)
    : normalize(FragmentMaterialInputs.Normal);  // 仅当保留 WSN 回退路径时
```

P2 定稿：**有 TBN 时始终走 TBN 分支**；`FragmentMaterialInputs.Normal` 仅 TSN。

### 5.4 Masked / Translucent 与法线

- **Masked：** 先光照/上色，再 `discard`（P1 顺序）；法线影响光照正确即可。
- **Translucent：** 扰动法线影响光照；alpha 仍 Opacity。

### 5.5 金样例与 Editor

| 资产 | 用途 |
|------|------|
| `MaterialIRSmoke_NormalMap.memtl` | BlinnPhong + `container` 法线贴图 + Opaque |
| 可选 + Translucent 变体 | 验证排序与 alpha |

**Preview：** 球体换用法线贴图 UV 正确的 mesh；或保持 box 贴图。

### 5.6 P2.3 验收

- [x] `NormalUnpack` 节点 + Add Node 调色板；`TextureSample.RGB → NormalUnpack → Normal` 编译通过。
- [x] `--material-ir-test` `VerifyNormalMapWorkflow`（TBN + unpack *2 - 1 + `u_Texture1`）。
- [ ] 目视：法线贴图 Preview 凹凸可见（需自备 normal map 纹理 + 重启 Editor 加载切线 mesh）。
- [ ] 与 Translucent、Masked 各 1 个目视 case。

---

## 6) 依赖与衔接

### 6.1 Phase 1 未完成时的折叠策略

| P1 项 | Phase 2 策略 |
|-------|----------------|
| P1.2 `MP_Normal` WSN 未做 | **P2.3 同时实现** pin + Capability + 默认 TSN |
| P1.3 `MP_AO` 未做 | 不阻塞 P2；可在 P2 后补 |
| P1.4 节点未做 | 不阻塞 P2；Lerp 与 Normal 图独立 |

### 6.2 与 Phase 3（PBR）的接口

| Phase 2 留下 | Phase 3 接 |
|--------------|------------|
| TBN + `a_Tangent` + `UsesTangentFrame` | PBR 模板复用同一 `glslinc` |
| `MP_Normal` TSN 语义 | PBR 法线项同语义 |
| `Translucent` 队列/Pass | PBR + Translucent 合法组合 |
| Capability 三档 Blend | 增加 PBR 列（Metallic/Roughness/Normal Active） |

---

## 7) 文件清单（汇总）

| 操作 | 路径 |
|------|------|
| 改 | `MaterialCompileTypes.h`、`Material.h`、`MaterialCapability.cpp`（Translucent 行） |
| 改 | `MaterialCompiler` / `GLSLMaterialShellAssemblerImpl`（Translucent alpha、UsesTangentFrame） |
| 改 | `Unlit.frag.template`、`BlinnPhong.frag.template` |
| 新增 | `Assets/EngineDefault/Shaders/Include/GLSL/MaterialTangentFrame.glslinc`（名可调整） |
| 改 | `MeshVertexLightingVaryings.glslinc` 或壳层 IO |
| 改 | `AssetManager.cpp`（顶点切线）、EngineDefault / Preview mesh |
| 改 | `MaterialGraphNodeDefs` + Registry（`NormalUnpack`） |
| 改 | `MaterialDetailsWindow.cpp`（Translucent combo） |
| 改 | `MaterialIRTest.cpp` / `MaterialGoldenAssetTest.cpp`（BlendMode 2 字段） |
| 可选 | `MaterialIRSmoke_Translucent.memtl`、`MaterialIRSmoke_NormalMap.memtl` |

---

## 8) 风险与对策

| 风险 | 对策 |
|------|------|
| 无切线 mesh 链接 BlinnPhong+Normal 图 | 编译 **Error**：`UsesTangentFrame` 但 draw 命令 layout 无 `a_Tangent`；或加载时强制验证 |
| WSN → TSN 语义变更 | PROGRESS_LOG + 打开资产 Warn；不自动改图 |
| Translucent 与 Masked 混淆 | UI 文案区分；测试矩阵分列 |
| 半透明排序错误 | 仅 mesh 中心排序；Phase 2 不实现 per-pixel order |
| Preview 球无切线 | 专项补全 Preview mesh，列入 P2.2 验收 |
| `IsTranslucent` 仍为 false 的遗漏 | 单测 / 队列断言；Code review 清单 |

---

## 9) Phase 2 完成标准

```text
P2.1 Translucent 验收通过
AND P2.2 TBN + 网格切线 验收通过
AND P2.3 法线贴图图节点 + BlinnPhong 扰动 验收通过
→ 更新 MATERIAL_SYSTEM_ROADMAP §5、PROGRESS_LOG
→ 启动 Phase 3 详细设计（PBR）
```

---

## 10) 参考

- [MATERIAL_SYSTEM_ROADMAP.md](./MATERIAL_SYSTEM_ROADMAP.md) — §2.3 Translucent 与 Pass
- [MATERIAL_SYSTEM_PHASE1.md](./MATERIAL_SYSTEM_PHASE1.md) — Capability、Masked、MP_Normal WSN
- [MATERIAL_SHADING_MODEL_PLAN.md](./MATERIAL_SHADING_MODEL_PLAN.md)
- Epic：[Physically Based Materials](https://dev.epicgames.com/documentation/en-us/unreal-engine/physically-based-materials-in-unreal-engine) — Normal、Blend Mode

---

## 附录 A) 测试矩阵（目视 + 自动化）

| Shading | Blend | Normal 图 | 期望 |
|---------|-------|-----------|------|
| Unlit | Opaque | — | 与 P1 一致 |
| Unlit | Translucent | — | Alpha 混合，无光照 |
| BlinnPhong | Opaque | — | 几何法线 |
| BlinnPhong | Opaque | 有 | TBN 凹凸 |
| BlinnPhong | Masked | 有 | 裁剪 + 凹凸 |
| BlinnPhong | Translucent | 有 | 排序透明 + 凹凸 |

自动化最小集：`--material-ir-test` 扩展 `CompileForDiagnostics(..., Translucent)` + on-disk `m_BlendMode: 2`；TBN 关键字可选 assert。
