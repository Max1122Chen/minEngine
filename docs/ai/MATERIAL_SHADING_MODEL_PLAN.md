# Material Shading Model 阶段设计（Unlit 阴影 + BlinnPhong）

Last updated: 2026-05-21  
Status: **设计定稿（实现前）**  
Pending（本阶段不做）: 材质编辑器 UI、扩展 NodeDef 库

---

## 1) 目标

在 **图材质 / `.memtl` 资产** 路径上恢复与旧 `Phong.vert/frag` 等价的**场景表现能力**：

| 能力 | Unlit | BlinnPhong |
|------|-------|------------|
| 相机 + `u_Model` 变换 | ✅ | ✅ |
| 图编译 `FragmentMaterialInputs` | ✅ Albedo 等 | ✅ |
| 方向光 / 点光 / 聚光 **阴影** | ✅ 接收 | ✅ 接收 |
| Blinn-Phong 漫反射 + 高光 | ❌ | ✅ |
| 与现有 ShadowPass / cascade / PCF 一致 | ✅ | ✅ |

**不在本阶段：** PBR、材质编辑器、大量新节点、旧 `Phong.meshader` 资产迁移工具。

---

## 2) 枚举与命名

### `MaterialShadingModel`（`MaterialCompileTypes.h`）

```cpp
enum class MaterialShadingModel : uint8_t
{
    Unlit = 0,
    BlinnPhong,   // 替代原 DefaultLit；经典 Blinn-Phong + 全类型光阴影
    // PBR,        // 后续新增；实现后作为“默认物理材质”，不再使用 DefaultLit 这个名字
};
```

- **删除 / 不重命名沿用** `DefaultLit`（避免与将来 PBR 语义冲突）。
- 反射 `ME_ENUM`、`.memtl` 的 `m_ShadingModel`、Assembler 分支、BasePass 分支一并改名。
- 资产默认：`MaterialIRSmoke.memtl` 可保持 `Unlit`；另可做 `MaterialIRSmoke_BlinnPhong.memtl` 或同图改枚举做验收。

### 与旧术语对照（设计用，非 1:1 兼容旧 JSON）

| 旧 Phong (`u_Material`) | 新图材质 (`FragmentMaterialInputs`) | 说明 |
|-------------------------|-------------------------------------|------|
| `DiffuseMap` × 常数 | `Albedo` | 已由图节点写入 |
| `SpecularMap` + `Shininess` | `Roughness` + 可选 `Metallic` | BlinnPhong 用 **Roughness** 控高光锐度（`shininess ∝ 1/roughness` 一类映射）；**Metallic** 作高光强度掩码（非 PBR 金属工作流） |
| （无） | `Emissive` | 光照后叠加 |
| （无） | `Opacity` | 输出 alpha |
| `Normal`（仅顶点） | 网格 `a_Normal` | 光照/阴影 bias 用，**不进图**（首版） |

旧 Phong 的 **Metallic** 语义与 PBR 不同；图里的 `MP_Metallic` 在 BlinnPhong 模板中解释为 **specular scale**，直到 PBR 阶段再按金属工作流重定义。

---

## 3) 架构：共享场景光照 GLSL + 按模型合成

```text
EngineDefault/Shaders/Include/GLSL/
  MeshVertexUniforms.glslinc          // 已有 PerFrameData + u_Model
  MeshVertexPosition.glslinc        // 已有 gl_Position
  MeshVertexLightingVaryings.glslinc   // 新增：FragPos, Normal, TexCoord, FragPosLightSpace, FragPosViewSpace
  SceneLights.glslinc                  // 从 Phong.frag 抽：LightsData、CalcDir/Point/Spot（或仅声明）
  SceneShadows.glslinc                 // 从 Phong.frag 抽：PCF/PCSS、三种 shadow sampler + UBO

Template/GLSL/
  Unlit.vert.template                  // + 注入 LightingVaryings + u_LightViewProj（方向光 cascade）
  Unlit.frag.template                  // + SceneShadows；合成：Albedo * visibility + Emissive
  BlinnPhong.vert.template             // 同 Unlit 顶点壳（可共用锚点）
  BlinnPhong.frag.template             // SceneLights + SceneShadows；合成 Blinn-Phong(Albedo, Roughness, …)
```

**原则：** `Shaders/Phong.frag` 保持为 **legacy 参考实现**；新模板 **include 抽出来的同一份逻辑**，避免第三套阴影代码。

---

## 4) 运行时绑定（BasePass）

当前 `MeshPassSceneBinding::bBindLighting == true` 才绑 `LightsData` + 全部 shadow map（见 `RenderPassBase::BindSceneDrawResources`）。

建议拆成两个概念（实现时二选一即可）：

| Flag | 绑定内容 |
|------|----------|
| `bBindSceneLighting` | `LightsData`、`DirLightViewProjs`、`CascadeFarPlanes`、spot/point shadow maps |
| `bBindShadowResources` | 可与上相同（本阶段 **Unlit 与 BlinnPhong 均为 true**） |

**BasePass 映射：**

| `m_ShadingModel` | `bBindSceneLighting` | Fragment 行为 |
|------------------|----------------------|---------------|
| `Unlit` | **true**（为阴影） | 不跑 Blinn-Phong，只算 **shadow visibility** |
| `BlinnPhong` | **true** | 全光照 + 阴影 |
| （legacy Phong meshader） | 走旧 shader，不经过 Material 编译器 | 不变 |

`TranslucencyPass`：首版 BlinnPhong/Unlit 图材质仍 **opaque-only**；半透明后续再读 `Opacity`。

**顶点：** 需要 `u_LightViewProj`（与旧 Phong.vert 一致）用于 `FragPosLightSpace`。Assembler 在 Unlit/BlinnPhong 顶点壳注入 `uniform mat4 u_LightViewProj` + `MeshVertexLightingVaryings.glslinc`。

---

## 5) 分阶段实现（本阶段仅 1 + 2）

### Phase S1 — Unlit + 全类型光阴影

**目标：** `test.mescene` + `MaterialIRSmoke`（Unlit）在方向光/点光/聚光阴影下明暗正确，无“全亮平面”。

**工作项：**

1. 从 `Phong.frag` 抽出 `SceneShadows.glslinc`（含 dir cascade + spot + point，过滤器宏与现网一致）。
2. 新增 `MeshVertexLightingVaryings.glslinc`；`Unlit.vert.template` 增加锚点 `VERTEX_LIGHTING_VARYINGS` / `VERTEX_LIGHT_VIEWPROJ`。
3. `Unlit.frag.template`：
   - include `SceneShadows` + `SceneLights`（仅 shadow 所需部分，或 shadow 函数依赖的最小 light 数据结构）。
   - 在 `FRAGMENT_STAGE_BODY` 之后、`FragColor` 之前计算 `float shadowVis`（与 Phong 各灯 shadow 项一致，**支持全部光类型**）。
   - 合成：`FragColor = vec4(FragmentMaterialInputs.Albedo * shadowVis + FragmentMaterialInputs.Emissive, Opacity)`（环境光项可后续加常数 ambient）。
4. `BasePass`：Unlit 图材质 `bBindLighting`（或等价）**打开**，与 BlinnPhong 同绑 shadow。
5. `MaterialIRTest`：可选增加“生成 frag 含 `u_DirLightShadowMap`”断言；目视 test 场景。

**完成标准：**

- Editor 打开 `test`，Unlit 材质物体在阴影区域明显变暗，移动光源/物体阴影跟随。
- `--material-ir-test` 仍通过；生成 Unlit frag 含 shadow sampler 声明。

---

### Phase S2 — BlinnPhong 模板

**目标：** `m_ShadingModel = BlinnPhong` 的 `.memtl` 在视觉上接近旧 Phong（漫反射 + 高光 + 阴影），输入来自图属性而非 `u_Material`。

**工作项：**

1. `MaterialShadingModel::BlinnPhong` + `BlinnPhong.vert/frag.template`。
2. `GLSLMaterialShellAssemblerImpl::ResolveTemplateSet` 实现 BlinnPhong 分支；移除 `DefaultLit` 错误路径。
3. `BlinnPhong.frag.template`：
   - `FRAGMENT_STAGE_BODY` 写入 `FragmentMaterialInputs`；
   - 调用与 Phong 等价的 `CalcDirLight` / `CalcPointLight` / `CalcSpotLight`，但 **diffuse 基色用 `Albedo`**，specular 用 `Roughness`/`Metallic` 映射（文档化公式，例如 `shininess = mix(4, 256, 1.0 - Roughness)`）。
   - `FragColor = vec4(lighting + Emissive, Opacity)`。
4. 验收资产：`MaterialIRSmoke_BlinnPhong.memtl`（或改 smoke 枚举）+ 可选 specular 图仍用 `Albedo` 单纹理起步。
5. Legacy `Shaders/Phong.*` 保留；静态 mesh 仍可用旧 shader，直到项目主动迁移。

**完成标准：**

- test 场景中 BlinnPhong 材质：高光、阴影、多光源与旧 Phong 场景同一量级正确。
- 编译失败时诊断清晰（模板缺失、anchor 未替换等）。

---

## 6) 与现有 MIR / 图节点的关系

- **不必为 S1/S2 新增 NodeDef**；现有 `MaterialOutput` + Albedo/Metallic/Roughness/Emissive 足够。
- `MP_WorldPositionOffset`：本阶段 vertex body 仍可为空；WPO 与阴影并列，后续单独 milestone。
- PBR：新枚举值 `PBR` + 新模板；**替代**将来计划中的 “DefaultLit” 概念，与 BlinnPhong 并列，不改写 BlinnPhong。

---

## 7) Pending（明确不做）

| 项 | 说明 |
|----|------|
| 材质编辑器 | UI + 编辑逻辑，等 S1/S2 视口验收后再设计 |
| 扩展 NodeDef | 编辑器 MVP 之后 |
| 自动化测试重设计 | Layer B/C（场景+资产）单独 milestone |
| 删除 `Shaders/Phong.*` | 仅当项目内无 legacy 引用后 |

---

## 8) 建议 PR / 提交切分

1. `refactor(glsl): extract SceneLights/SceneShadows includes from Phong.frag`  
2. `feat(material): Unlit template receives full light-type shadows`  
3. `feat(material): rename DefaultLit to BlinnPhong shading model + templates`  
4. `chore(assets): optional MaterialIRSmoke_BlinnPhong.memtl for regression`

---

## 9) 风险与对策

| 风险 | 对策 |
|------|------|
| 三套阴影逻辑漂移 | 只维护 `SceneShadows.glslinc` 一份 |
| Unlit 绑光 UB O 开销 | 可接受；与 Phong 一致 |
| `Metallic` 在 BlinnPhong 与 PBR 语义冲突 | 文档 + 枚举分轨；PBR 模板重写解释 |
| 顶点缺少 `a_Normal` 的 mesh | 与 Phong 相同，fallback `vec3(0,1,0)` |

---

## 10) 决策记录（用户确认）

- **S1 阴影范围：** 方向光 + 点光 + 聚光（与现有阴影管线一致），不做“仅方向光”简化。
- **枚举：** 使用 `BlinnPhong`，不用 `DefaultLit`；未来 `PBR` 单独枚举。
- **后续：** 编辑器、更多节点 — **pending**，本文件完成后再开新设计。
