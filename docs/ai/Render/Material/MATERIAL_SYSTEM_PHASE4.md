# Material System — Phase 4 详细设计（IBL）

Last updated: 2026-05-23  
Status: **✅ Phase 4 关门**（首版 IBL；irradiance 卷积 / 独立 prefilter pass 延后）  
前置：[MATERIAL_SYSTEM_PHASE3.md](./MATERIAL_SYSTEM_PHASE3.md) ✅ · [RESOURCE_PIPELINE_PLAN.md](../RESOURCE_PIPELINE_PLAN.md) R2 ✅

---

## 0) Phase 4 目标与边界

| 做 | 不做（显式延后） |
|----|------------------|
| **PBR 环境光（IBL）**：split-sum 间接光，替换常数 ambient | Parallax / POM |
| HDR / PNG / fallback **环境 cubemap** + **BRDF LUT** | 独立 irradiance 卷积 pass |
| `MaterialIBL.glslinc` + `PBR.frag.template` | 专用 prefilter GPU 滤波（暂 HDR mip） |
| BasePass / TranslucencyPass **绑定** unit 4–6 | Skybox 背景绘制 |
| 间接光 × **AO**（直接光不乘 AO） | Translucent IBL 专项验收 |
| `--material-ir-test` IBL 子集 | Specular-Glossiness |

---

## 1) 实现摘要（关门状态）

### 1.1 PBR 片段合成（当前）

```glsl
vec3 direct = dir + point + spot;
vec3 indirect = CalcIndirectPBR(..., u_EnvIrradianceMap, u_EnvPrefilterMap, u_EnvBrdfLUT) * u_EnvIntensity;
vec3 result = direct + indirect + emissive;
```

### 1.2 引擎组件

| 组件 | 状态 |
|------|------|
| `EnvMapCapture` | HDR equirect → RGB16F cubemap + mip |
| `EngineIBLEnvironment` | 加载顺序 + `BindForPBRDraw` |
| `BrdfLutGenerator` | 无 `brdf_lut.png` 时 CPU 生成 |
| `MaterialIBL.glslinc` | `CalcIndirectPBR` |
| `BasePass` / `TranslucencyPass` | `pipeline` 指针 + 材质贴图后 bind IBL |
| 纹理单元 | 材质 0–3，IBL 4–6，阴影 8+ |

---

## 2) 资源加载顺序

```text
Irradiance:  irradiance_*.png → *.hdr capture → validation 6-color cube
Prefilter:   prefilter_*.png → else 同 environment cubemap（HDR 时有 mip）
BRDF LUT:    brdf_lut.png → else CPU IntegrateBRDF（256²）
```

路径：`Assets/EngineDefault/Textures/IBL/` · 说明见同目录 `README.md`。

---

## 3) 验收清单（全部完成）

### P4.1 资源与绑定

- [x] `TextureCubeLoader` / `OpenGLTextureCube`
- [x] PBR uniform + Pass bind unit 4–6
- [x] fallback cubemap + 日志
- [x] 目视环境反射（2026-05-23）

### P4.2 Shader

- [x] `MaterialIBL.glslinc` / `CalcIndirectPBR`
- [x] 移除 `kMaterialPBRAmbientStrength` 路径

### P4.3 模板

- [x] `PBR.frag.template` indirect + direct 分离
- [x] 编译器 include IBL

### P4.4 测试与文档

- [x] `VerifyPBRWorkflow`（IBL 采样符号 + GPU link）
- [x] `VerifyEngineIBLEnvironmentInit`
- [x] `VerifyIBLEnvironmentFallbackChain`
- [x] `PROGRESS_LOG` / 本文件 / IBL README

### 目视（用户自测参考）

- [x] 旋转相机 → 环境色/天空在光滑表面变化
- [ ] Metallic≈1 → 暗部仍有环境高光（拉高 Metallic 自测）
- [ ] AO 贴图 → 缝隙间接光变暗（连接 AO 自测）

---

## 4) Phase 4 完成后你应看到的效果

见 [§5 目视效果指南](#5-目视效果指南)。

---

## 5) 目视效果指南

在 **PBR 材质** + **EngineDefault IBL**（或项目内 HDR）下，应具备：

| 现象 | 原因 |
|------|------|
| **不再是均匀灰 ambient** | 常数 0.03×albedo 已换成 cubemap 间接光 |
| **旋转相机时，球/金属上环境色在变** | specular 采样 `prefilter` + 反射向量 R |
| **粗糙度低更锐利、高更糊** | `textureLod(prefilter, R, roughness × MAX_LOD)` |
| **金属（Metallic→1）暗部有亮斑/天空色** | kS 高 + specular IBL |
| **非金属以柔和环境色为主** | diffuse IBL × albedo × (1−metallic) |
| **法线朝上的面更亮** | diffuse 采样 `texture(irradiance, N)` |
| **AO 低的区域间接光更暗** | `CalcIndirectPBR` 最后 × AO |
| **直接光（太阳/点光）不受 AO 影响** | 仅 `direct` 项，P3 语义保持 |
| **无 IBL 资源时仍有彩色环境** | 6 色 validation cube（调试用） |

**尚不明显或首版简化（正常）：**

- Diffuse 与 specular 共用同一 cubemap（未做 irradiance 卷积）→ 漫反射可能偏亮/偏锐
- Prefilter 无离线六面时 = HDR cubemap + mip，非 Epic 预滤波
- 无 skybox 网格 → 看不到背景天空，只有物体上的反射
- Translucent 物体 IBL 未单独调

---

## 6) 延后 → Phase 5

详见 [MATERIAL_SYSTEM_PHASE5.md](./MATERIAL_SYSTEM_PHASE5.md)（**仅** irradiance / prefilter / skybox）。Parallax/WPO → Backlog。仍不在 P5：`u_EnvIntensity` 编辑器暴露、编辑器 Undo。

---

## 7) 参考

- [LearnOpenGL IBL](https://learnopengl.com/PBR/IBL/IBL) · [Specular IBL](https://learnopengl.com/PBR/IBL/Specular-IBL)
- [MATERIAL_SYSTEM_PHASE3.md](./MATERIAL_SYSTEM_PHASE3.md)
