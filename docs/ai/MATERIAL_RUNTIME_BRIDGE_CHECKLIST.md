# Material Runtime Bridge — 接入检查清单 & 推进计划

Last updated: 2026-05-19

## 0) 目标（什么叫「接通了」）

在 **不改材质图编辑器 UI** 的前提下，满足：

1. 用代码构建与 `MaterialIRTest` 相同的 smoke 图（或硬编码 `MaterialEdGraph`）。
2. `MaterialCompiler::Compile` → 得到 `MaterialCompiledShader`（含 `FullVertexShader` / `FullFragmentShader`）。
3. GPU 上 `glCompile` + `glLink` 成功（非仅字符串子串测试）。
4. 场景中 **至少一个 StaticMesh** 使用该材质，Viewport 可见：
   - 纹理采样正确（UV0 → `u_Texture0`）。
   - Scalar uniform 生效（`u_ScalarParam0` 写入 Metallic，虽 Unlit 不显示，但绑定链路要通）。
   - Unlit 合成：`FragColor.rgb ≈ Albedo + Emissive`（白纹理 + smoke 常数 → 约 `(1.2, 0.8, 0.2)`）。

**不在本阶段范围：** DefaultLit/PBR、材质编辑器 UI、阴影与多 pass 与图材质完全统一。

---

## 1) 现状快照（与 S0–S2 的边界）

| 层 | 已有 | 缺失 |
|----|------|------|
| MIR 编译 | `MaterialCompiler`、Unlit Assembler、`MaterialParameters.TexCoords` | — |
| 测试 | `--material-ir-test` 字符串断言 + 写 `Saved/Materials/*.glsl` | 无 `glCompileShader` |
| `Shader` / `RHIShader` | 从 **文件路径** 加载 GLSL（`OpenGLShader(path, path)`） | 从 **内存字符串** 创建 |
| `Material` 资产 | `m_Shader` + `m_Diffuse`/`m_Specular`/`m_Normal` + `BindTextures()` | 图编译结果、slot→纹理映射、scalar uniform 表 |
| 网格布局 | Assimp：`a_Position@0`, `a_TexCoord@1`, `a_Normal@2` | 与生成 shader 的 `layout(location=…)` **一致** |
| 生成 Vertex | `a_TexCoord` → `MaterialParameters` → varying | **无** `PerFrameData` / `u_Model` / 正确 `gl_Position` |
| `BasePass` | Phong + UBO + 阴影 + `u_Material.DiffuseMap` | 与 Unlit 图材质 **不兼容** |

**命名注意：** C++ `Material::MaterialParameters`（反射字段）与 GLSL `MaterialParameters`（TexCoords 结构体）是不同概念，文档/代码里要区分「资产参数」与「shader 内置符号」。

---

## 2) 接入检查清单（按层）

### A. 编译产物 → GPU Program

- [ ] **A1** `RHIShader` / `OpenGLShader` 增加「源码字符串」构造路径（或 `CreateRHIShaderFromSource(vert, frag)`），保留现有文件路径 API。
- [ ] **A2** 编译/链接失败时：日志输出 `infoLog`，并写入 `MaterialCompiledShader::Diagnostics`（不要只 `std::cout`）。
- [ ] **A3** 封装 `MaterialCompiledShader → std::shared_ptr<Shader>`（或 `RHIShader`）工厂，例如 `CreateShaderFromCompiled(const MaterialCompiledShader&)`。
- [ ] **A4** 在 `MaterialIRTest` 或独立 `--material-gl-compile-test` 中对 `FullVertexShader`/`FullFragmentShader` 调用 GPU 编译（与 A1 同 PR 或紧跟）。

### B. 材质资产与参数绑定

- [ ] **B1** 扩展 `Material`（或新建 `GraphMaterial` / `MaterialInstance`）持有：
  - `MaterialEdGraph` 或已烘焙的 MIR 图引用；
  - `MaterialCompileEnvironment`（先固定 `Unlit`）；
  - 编译缓存：`MaterialCompiledShader` + `std::shared_ptr<Shader>`；
  - **纹理槽表**：`slotIndex → shared_ptr<Texture2D>`（对应 `u_TextureN`）；
  - **标量槽表**：`slotIndex → float`（对应 `u_ScalarParamN`）。
- [ ] **B2** `Recompile()`：图或参数变更时重新 `MaterialCompiler::Compile` + A3。
- [ ] **B3** `BindForDraw()`（替代或扩展 `BindTextures()`）：
  - `texture unit 0..N-1` 绑定 `u_Texture0..`（与 MIR `TextureSlotIndex` 一致）；
  - `UploadUniformFloat("u_ScalarParam0", value)` 等；
  - 不绑定图里未使用的 slot（可选：编译结果记录 used slots）。
- [ ] **B4** 默认纹理：未指定 slot 时绑定 **1×1 白色** 2D 纹理，保证 smoke 预期 `Albedo≈(1,1,1)`。

### C. Vertex Shader 与场景空间（关键缺口）

当前生成 vertex：

```glsl
gl_Position = vec4(a_Position, 1.0);  // 物体空间直接进裁剪，无相机
```

- [ ] **C1** Unlit Assembler 注入与 Phong 兼容的 **最小变换**（二选一，需先定案）：
  - **方案 C1a（推荐 MVP）：** 在 Assembler 模板中加入 `#version` 后相同的 `PerFrameData` std140 + `uniform mat4 u_Model`，`gl_Position = ViewProj * u_Model * vec4(a_Position,1)`；fragment 仍 Unlit。
  - **方案 C1b：** 独立 `UnlitMeshPass`，不绑光照 UBO，仅 `u_ViewProj` + `u_Model` 两个 uniform（改动 BasePass 更大）。
- [ ] **C2** 确认 `layout(location=0/1)` 与 Assimp 网格一致（已一致，回归测一项即可）。
- [ ] **C3** `UsesTexCoord0 == false` 的材质：Assembler 不生成 TexCoord attribute / varying（已有逻辑，需无 UV 网格用例验证）。

### D. Draw Pass 集成

- [ ] **D1** `BasePass` 按材质类型分支，或引入 `IMaterialShaderSetup`：
  - **Legacy Phong：** 现有 `PerFrameData`、`LightsData`、阴影采样；
  - **Graph Unlit：** 仅 `PerFrameData` + `u_Model` + B3 的 texture/scalar，**跳过** Phong/阴影 uniform。
- [ ] **D2** `TranslucencyPass` 同步 D1（若图材质可能半透明，读 `FragmentMaterialInputs.Opacity`；MVP 可先做 opaque-only）。
- [ ] **D3** `MeshDrawCommand` / `StaticMeshComponent`：能挂接 **图材质** 实例（Inspector 或 Playground 硬编码均可）。
- [ ] **D4** Shadow pass：MVP **仍用原材质深度** 或强制 depth-only shader；图材质 cast shadow 标为 **后续**（见风险 R4）。

### E. 端到端验证

- [ ] **E1** Playground 或 Editor 启动路径：加载/构建 smoke 图材质，赋给场景内一个 mesh。
- [ ] **E2** 目视：带 UV 的 mesh + 白纹理 → 偏黄绿 `(~1.2, 0.8, 0.2)`。
- [ ] **E3** 日志：编译失败、link 失败、uniform 找不到（若做 location 查询）可定位。
- [ ] **E4** 文档：`PROGRESS_LOG.md` 记录 milestone 与已知限制。

---

## 3) 与 S0/S1/S2 设计的一致性检查

| S 阶段承诺 | Runtime 接入要求 |
|------------|------------------|
| S0 多阶段 `Stages[]` + 完整 shader 字符串 | A1–A3 消费 `FullVertexShader`/`FullFragmentShader` |
| S1 `FragmentMaterialInputs` + Unlit 合成 | 无需改 MIR；确保 D1 不覆盖 `FragColor` |
| S2 `MaterialParameters.TexCoords` + varying | C2 + 网格 UV；B3 不破坏 TexCoord 链 |
| 无 MaterialInterpolator IR | Assembler 仍负责 varying；Runtime 不另插 UV |
| DefaultLit 未实现 | `env.ShadingMode = Unlit` 固定，直到 P9 |

---

## 4) 已知风险（接入前心里有数）

| ID | 风险 | 缓解 |
|----|------|------|
| R1 | 生成 VS 无相机变换 → 黑屏/裁切 | **C1 为 blocker**，先于 E2 |
| R2 | `BasePass` 硬编码 Phong uniform → link OK 但运行错误 | D1 分支或专用 pass |
| R3 | `OpenGLShader` 失败仅打印，测试检测不到 | A2 + A4 |
| R4 | 图材质 vs Shadow depth shader 不一致 | MVP 不 cast shadow 或仍用默认 Phong depth |
| R5 | 多 `MaterialOutput` 节点语义未定义 | 接入期禁止多 Output；编译期报错 |
| R6 | `UsesTexCoord0` 与手写 UV 脱节 | 仅支持 `TextureCoordinate` 节点路径 |
| R7 | C++ `MaterialParameters` 与 GLSL 同名混淆 | 新 API 命名如 `GraphMaterialBinding` |

---

## 5) 推进计划（建议顺序）

### Phase R0 — 编译可信（~0.5–1 天）

**目标：** 字符串 shader 能在 GPU 上编译通过。

1. A1 + A2：内存源码 + 诊断回传  
2. A4：`--material-ir-test` 内或子命令 GPU compile smoke shaders  
3. （可选）断言 `FragmentMaterialInputs` 在 `void main` 之前  

**完成标准：** CI/本地 test exit 0，失败时能看到 GL info log。

---

### Phase R1 — 最小 Unlit 网格 Pass（~1–2 天）【Blocker: C1】

**目标：** 屏幕上能看到 **正确变换** 的 Unlit 单色/常数材质（先不纹理）。

1. C1a：Assembler 注入 `PerFrameData` + `u_Model` + 标准 `gl_Position`  
2. D1：`BasePass` Graph-Unlit 分支（只绑 PerFrameData + u_Model）  
3. 硬编码 `Material` + 常数 Albedo 图（无纹理）→ 验证 E2 简化版（纯色）

**完成标准：** 场景 mesh 随相机移动正确，纯色 Unlit 可见。

---

### Phase R2 — Smoke 全链路（~1–2 天）

**目标：** 与 `MaterialIRTest` 等价的 **视口** 结果。

1. B1–B4：slot 纹理 + scalar + 白纹理默认  
2. A3：编译缓存到 `m_Shader`  
3. D3：Playground 一个 mesh 挂 smoke 材质  
4. E2 目视验收  

**完成标准：** 纹理 + Emissive 常数颜色符合预期。

---

### Phase R3 — 工程化收口（~1 天）

1. `Recompile()` 与简单参数 API（改 scalar/换纹理触发）  
2. 保存 `Saved/Materials/` 可选 dump（调试）  
3. PROGRESS_LOG + 简短 session note  
4. 单测：DefaultLit 编译失败；无 TexCoord 图（若支持）  

---

### Phase R4 — 并行轨道（接入 **之后** 再选）

| 轨道 | 内容 | 依赖 |
|------|------|------|
| **P9 DefaultLit** | `IMaterialShadingModel` 实现 + 消耗 Metallic/Roughness | R2 完成 |
| **Editor 图 UI** | 节点/引脚/预览 | R2 + 资产序列化 |
| **深度/阴影** | 图材质 depth pass / MP_WPO | C1 + 阴影架构 |
| **多 UV / CustomizedUV** | `UsesTexCoord` 位掩码、attribute 扩展 | S2 扩展设计 |

---

## 6) 推荐 PR 切分（便于 review）

1. `feat(rhi): compile shader from source string + diagnostics`  
2. `feat(material): GraphMaterial compile cache + BindForDraw`  
3. `feat(shader-assembler): Unlit vertex transform with PerFrameData`  
4. `feat(render): BasePass graph-unlit branch`  
5. `feat(playground): smoke graph material on test mesh`  

---

## 7) 决策点（开工前 5 分钟可定）

1. **C1a vs C1b：** 是否复用 Phong 的 `PerFrameData` block？（推荐 **C1a**，与现有相机 UBO 一致。）  
2. **扩展 `Material` vs 新类 `GraphMaterial`：** MVP 可 `Material` 加可选 `m_CompiledGraph` 字段；长期建议子类或组合。  
3. **第一个验收场景：** Playground 硬编码 vs Editor 默认场景改材质？（推荐 **Playground**，少动 Editor。）  

---

## 8) 相关文件索引

| 用途 | 路径 |
|------|------|
| 编译入口 | `MaterialCompiler.cpp` |
| 组装模板 | `MaterialShadingModel.cpp` |
| 测试 smoke | `MaterialIRTest.cpp` |
| 生成样例 | `bin/Saved/Materials/Generated*.glsl` |
| RHI 创建 shader | `OpenGLRHI.cpp`, `OpenGLShader.cpp` |
| Draw | `BasePass.cpp`, `MeshDrawCommand.h` |
| 网格 UV | `AssetManager.cpp`（Assimp 顶点布局） |
| 组件挂材质 | `StaticMeshComponent.cpp` |
| Phong 参考 VS | `Shaders/Phong.vert` |

---

## 9) 下一步行动（本周）

1. **你确认 C1a**（Unlit VS 接 `PerFrameData` + `u_Model`）。  
2. 实现 **R0**（GPU compile test）— 低成本，立刻暴露未来问题。  
3. 实现 **R1**（变换 + BasePass 分支）— 解锁「能看见」。  
4. 实现 **R2**（smoke 全链路）— 闭环 S0–S2 学习成果。  

P9 / Editor UI 在 R2 目视通过后再开，避免同时改着色模型与 UI。
