# Material System — Phase 4 详细设计（IBL）

Last updated: 2026-05-23  
Status: **P4.1 进行中**（Parallax / WPO / 编辑器 Undo 等 **不在本 Phase**）  
前置：[MATERIAL_SYSTEM_PHASE3.md](./MATERIAL_SYSTEM_PHASE3.md) ✅ 验收通过（2026-05-23）；[MATERIAL_SYSTEM_ROADMAP.md](./MATERIAL_SYSTEM_ROADMAP.md)

---

## 0) Phase 4 目标与边界

| 做 | 不做（显式延后） |
|----|------------------|
| **PBR 环境光（IBL）**：替换/增强 `kMaterialPBRAmbientStrength` 常数 ambient | Parallax / POM |
| 引擎级 **环境 Cubemap** 资源 + BasePass/TranslucencyPass **绑定** | `MP_WorldPositionOffset` 暴露 |
| `MaterialPBR.glslinc` 增加 **间接光**（diffuse irradiance + specular prefilter） | Tessellation / 真 displacement |
| 与 P3 **AO 语义**一致：IBL 项 × `FragmentMaterialInputs.AO` | 材质编辑器 Undo、Shader 预览 |
| `--material-ir-test` IBL 关键字子集 | Specular-Glossiness 工作流 |
| 可选：`Texture2D` **sRGB** 元数据（若 IBL 与 albedo 一起做更顺） | 全场景 Sky 渲染（可用静态 cubemap 顶） |

**用户拍板（2026-05-23）：** Phase 4 **仅 IBL**；其它材质增强与编辑器项 **优先级更低**，另排期。

**建议工期：** 约 1.5–2 周（学习节奏），**P4.1 → P4.2 → P4.3 → P4.4**。

---

## 1) 现状（P3 结束）

### 1.1 PBR 片段合成（当前）

```glsl
vec3 ambient = vec3(kMaterialPBRAmbientStrength) * albedo * AO;  // 0.03 常数
vec3 direct = dir + point + spot;   // GGX × shadow
vec3 result = ambient + direct + emissive;
```

问题：无 **方向性环境反射**、金属高光在暗部不自然、与 LearnOpenGL / UE 的 IBL 差距大。

### 1.2 引擎已有能力

| 组件 | 现状 |
|------|------|
| `RHITextureCube` / `OpenGLTextureCube` | ✅ 阴影 point-light depth cube 已用；P4.1 颜色 cubemap 校验通过 |
| `RHI::CreateRHITextureCube` | ✅ 六面 2D 上传；失败时返回 `nullptr`（`GetID()==0`） |
| `TextureCubeLoader` | ✅ 六面 PNG + `CreateSolidColorCube`（`--material-ir-test`） |
| `EngineIBLEnvironment` | ✅ irradiance 加载或 6 色 fallback；PBR 绑定 unit 4–6 |
| `TextureCube` 资产类 | ✅ 头文件有，**无** AssetManager 资产类型（仍用 Loader） |
| `SceneLights` UBO | ✅ 方向/点/聚光 + 阴影 |
| 天空 / 捕获 | ❌ 无动态 Skybox、无 `glGenerateMipmap` cubemap 预滤波管线 |

### 1.3 P3 AO 拍板（延续）

**间接光（含 IBL）× AO**；**直接光不乘 AO**（已在 P3 `PBR.frag.template` 分离 `ambient` 与 `direct`）。

---

## 2) IBL 方案选型（推荐）

### 2.1 目标模型（Split-Sum Approximation）

与 [LearnOpenGL PBR IBL](https://learnopengl.com/PBR/IBL/Specular-IBL) / Epic 一致：

```text
Lo_ibl = (kD_ibl * diffuseIBL + kS_ibl * specularIBL) * AO

diffuseIBL  = texture(irradianceMap,  N).rgb * albedo
specularIBL = texture(prefilterMap,   R).rgb * (F * brdfLUT.x + brdfLUT.y)
```

其中 `R = reflect(-V, N)`，`F0` / `kD` / `kS` 与 P3 直接光相同。

### 2.2 三张贴图 vs 简化首版

| 方案 | 内容 | 优点 | 缺点 |
|------|------|------|------|
| **A（推荐）** | HDR **环境** + **Irradiance** + **Prefiltered** + **BRDF LUT**（2D） | 质量稳定、与教学资料一致 | 需离线或启动时预计算；资产 4 份 |
| B | 单张 **HDR cubemap**，shader 内用同一贴图 `textureLod` 近似 | 实现快 | 质量差、难调 |
| C | 仅 **Irradiance** cubemap + 直接光 specular | 半套 IBL | 金属环境反射弱 |

**Phase 4 定稿：方案 A**，允许第一版用 **引擎打包的默认 LDR/HDR 资源**（不依赖用户项目）。

### 2.3 预计算由谁做

| 选项 | 说明 | 推荐 |
|------|------|------|
| A | **离线工具**（Python/scripts）生成 `.cubemap` 或六面 PNG + LUT PNG，提交 `EngineDefault` | ✅ 首版 |
| B | 引擎启动时对 HDR 做 GPU 卷积 | 二期；复杂度高 |

---

## 3) 实施顺序

```text
P4.1  环境资源 + 渲染绑定（非图材质）
P4.2  MaterialIBL.glslinc + 扩展 MaterialPBR
P4.3  PBR.frag.template 接入 IBL；移除/降级常数 ambient
P4.4  material-ir-test + 目视 + 文档关门
```

---

## 4) P4.1 — 环境资源与 Pass 绑定

### 4.1 资源布局（建议路径）

```text
Assets/EngineDefault/Textures/IBL/
  newport_loft.hdr              # 或现有免费 HDR（需确认许可）
  newport_irradiance.dds        # 或 六面 PNG 目录
  newport_prefilter.dds
  newport_brdf_lut.png          # 512×512 RG16F 等效
```

若暂不引入 DDS：六面 `irradiance_*.jpg` + `prefilter_*.jpg`（带 mip）+ `brdf_lut.png`。

### 4.2 C++ 侧

| 任务 | 说明 |
|------|------|
| `AssetManager::LoadAsset_Impl<TextureCube>` | 从 meta 或约定文件夹加载 6 面 / 立方 HDR |
| `EngineEnvironment` 或 `RenderSystem` 持有 | `shared_ptr<TextureCube>` ×3（irradiance、prefilter、brdf 2D） |
| `RenderPipeline` / `BasePass` | PBR 绘制前绑定 `u_IrradianceMap`、`u_PrefilterMap`、`u_BrdfLUT`（**slot 与材质纹理错开**） |
| `TranslucencyPass` | 与 BasePass **同一套** IBL 绑定 |

**纹理单元规划（待实现时锁定）：**

```text
材质图：u_Texture0..N     （现有 ParameterLayout）
场景：   LightsData UBO、shadow maps（现有）
IBL：     建议固定 u_EnvIrradiance / u_EnvPrefilter / u_BrdfLUT（全局 uniform，不进 Material IR）
```

### 4.3 验收

- [x] `TextureCubeLoader` + `OpenGLTextureCube` 颜色 cubemap 上传（`VerifyTextureCubeRHICreation`）。
- [x] PBR fragment 声明 `u_EnvIrradianceMap` / `u_EnvPrefilterMap` / `u_EnvBrdfLUT`；BasePass/TranslucencyPass 绑定 unit 4–6。
- [x] 无 irradiance 资源 → 6 色 validation cubemap + **Info** 日志（`EngineIBLEnvironment`）。
- [ ] 目视：PBR 材质已绑定 IBL 纹理（P4.2 后才在 shader 中采样，环境光仍为常数 ambient）。
- [ ] Cubemap 磁盘加载失败时行为与日志（有 fallback，未单独测坏路径）。

---

## 5) P4.2 — Shader：`MaterialIBL.glslinc`

### 5.1 新建 include

职责：

```glsl
vec3 CalcIndirectPBR(
    vec3 N, vec3 V,
    vec3 albedo, float metallic, float roughness, float ao,
    samplerCube irradianceMap,
    samplerCube prefilterMap,
    sampler2D brdfLUT);
```

内部：

- `F0 = mix(vec3(0.04), albedo, metallic)`
- `kS = FresnelSchlick(max(dot(N,V),0), F0)`；`kD = (1-kS)*(1-metallic)`
- `diffuseIBL = texture(irradianceMap, N).rgb`
- `R = reflect(-V, N)`；`prefiltered = textureLod(prefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb`
- `brdf = texture(brdfLUT, vec2(max(dot(N,V),0), roughness)).rg`
- `specularIBL = prefiltered * (F * brdf.x + brdf.y)`
- `return (kD * albedo * diffuseIBL + specularIBL) * ao`

`MAX_REFLECTION_LOD` 与 prefilter mip 级数一致（如 4.0 或 `log2(size)`）。

### 5.2 与 `MaterialPBR.glslinc` 关系

- **不**改直接光函数签名。
- `PBR.frag.template` 在 `direct` 累加后调用 `CalcIndirectPBR`。
- 删除或 `#ifdef` 掉 `kMaterialPBRAmbientStrength` 路径（IBL 启用时）。

### 5.3 验收

- [ ] 编译后 fragment 含 `CalcIndirectPBR` 或等价符号。
- [ ] `roughness=0` 时 specular IBL 锐利；`roughness=1` 时模糊。

---

## 6) P4.3 — 模板与编译器

### 6.1 `PBR.frag.template` 合成顺序

```glsl
vec3 norm = ... TBN ...;
vec3 viewDir = ...;
vec3 direct = dir + point + spot;
vec3 indirect = CalcIndirectPBR(norm, viewDir, albedo, metallic, roughness, AO, ...);
vec3 result = direct + indirect + emissive;
FragColor = vec4(result, opacity);
```

### 6.2 `GLSLMaterialShellAssemblerImpl`

- `BuildFragmentSceneLighting` 在 `ShadingModel == PBR` 时额外 `#include MaterialIBL.glslinc`。
- 在 fragment **preamble** 或 scene lighting 块注入 **IBL uniform 声明**（若不由全局 UBO 提供）。

### 6.3 BlinnPhong / Unlit

**本 Phase 不改** BlinnPhong 环境项（仍可用 Phong 内 `ambientStrength`）。避免 scope 膨胀。

### 6.4 验收

- [ ] Preview 球 PBR：旋转相机可见 **环境反射** 变化。
- [ ] 金属（Metallic≈1）暗部有 **高光环境**；非金属 primarily diffuse IBL。
- [ ] AO=0 区域 IBL 变暗（缝隙）。

---

## 7) P4.4 — 测试与文档

| 项 | 内容 |
|----|------|
| `VerifyPBR_IBL` | 编译 PBR fragment 含 `irradiance` / `prefilter` / `brdf` 采样；GPU link |
| 金样例 | 可选 `MaterialIRSmoke_PBR_IBL.memtl`（仍用 marble，仅验证绑定） |
| 文档 | 本文件验收勾选；ROADMAP §6；PROGRESS_LOG |

---

## 8) 延后清单（非 P4）

| 项 | 建议阶段 |
|----|----------|
| Parallax / POM | P5 或 Backlog |
| WPO / displacement | P5+ |
| 编辑器 Undo、Shader 预览 | 用户更高优先级非材质项 |
| `MaterialIRSmoke_PBR.memtl` | P3 可选；可与 P4 金样例合并 |
| sRGB `Texture2D` 元数据 | P4 可选尾或 P5 |
| 动态 Sky / 实时捕获 cubemap | P5+ |

---

## 9) 风险与对策

| 风险 | 对策 |
|------|------|
| HDR / cubemap 加载失败 | 回退 P3 常数 ambient；日志 Warning |
| 纹理单元冲突 | 文档固定 IBL 单元；Material 纹理从 N+3 起 |
| 预滤波 mip 与 `roughness` 映射不准 | 首版用 LearnOpenGL 同一 `MAX_REFLECTION_LOD` |
| IBL 过亮 | 曝光系数 `u_EnvIntensity` uniform（默认 1.0） |
| Translucent + IBL | 首版 Opaque 验收；半透明 IBL 与 P2 排序问题同列 |

---

## 10) Phase 4 完成标准

```text
P4.1 环境 cubemap + BRDF LUT 加载与 Pass 绑定 验收
AND P4.2 MaterialIBL.glslinc 验收
AND P4.3 PBR 模板间接光 + 目视 验收
AND P4.4 material-ir-test + 文档更新
→ Backlog：Parallax / WPO / 编辑器（按用户优先级）
```

---

## 11) 参考

- LearnOpenGL: [IBL](https://learnopengl.com/PBR/IBL/IBL) · [Specular IBL](https://learnopengl.com/PBR/IBL/Specular-IBL)
- Epic: [Physically Based Materials](https://dev.epicgames.com/documentation/en-us/unreal-engine/physically-based-materials-in-unreal-engine)
- [MATERIAL_SYSTEM_PHASE3.md](./MATERIAL_SYSTEM_PHASE3.md) — P3 ambient 分离、AO 语义
