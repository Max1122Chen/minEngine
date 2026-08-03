# EnvironmentMap Asset + Sky / IBL wiring — Design Spec (Draft)

## Meta
- **ID:** `RND-F10`
- **Type:** Feature
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-08-03
- **Related:** [Implementation](./RND-F10_ENVIRONMENT_MAP_ASSET_IMPLEMENTATION.md)（切片草稿）, [TECH_DEBT TD-015](../TECH_DEBT.md), [F03 §16.5](./RND-F03_LEGACY_RHI_REMOVAL_DESIGN.md), [Material Phase 4 IBL](./Material/MATERIAL_SYSTEM_PHASE4.md), [F09](./RND-F09_RHI_HYGIENE_SWEEP_DESIGN.md)

## TL;DR

用尽引擎全局 `EngineIBLEnvironment` + Pass 自加载目录的模型；引入 **EnvironmentMap Asset**，由 `SkyBoxComponent` 引用，驱动 SkyPass 与场景 IBL。一期优先 **磁盘已烘焙贴图 + 引用接线**；**GPU Bake** 另切片，按现代 RHI（CommandList / PSO / SRV）重写旧 `EnvMapCapture`，不进每帧 RDG。

## Scope
- **In:** EnvironmentMap Asset 形状；SkyBoxComponent / SceneProxy 引用；SkyPass 与 Set1 IBL 从场景数据取图；付清 TD-015 中「引擎层 OpenGL SRV + 旁路 capture」；可选 Editor/工具 Bake。
- **Out:** 动态时间天空 / Atmosphere；每帧 RDG 内 equirect 捕获；多天空混合；Vulkan；改阴影算法。

## Reader quick start
1. 本文件 §3（易难边界 + 方案）
2. Implementation 切片顺序
3. 代码入口：`SkyBoxComponent`、`SkyBoxPass`、`EngineSceneBindingSets`、`Environment/EnvMapCapture.*`（当前未编进目标）

---

## 1) 背景与目标

**Pain：** IBL/天空曾是引擎固有单份资源（固定 `EngineDefault/Textures/IBL`）；F03-M4 整段停用后，Set1 IBL 槽恒 null，Sky 仍靠 Pass `Initialize` 自加载。与「场景决定画什么」不一致，且旧 capture 是现代 RHI 反例。

**Done 长什么样：**

- 场景里 `SkyBoxComponent` 引用一个 EnvironmentMap Asset（可指向引擎默认 Content）。
- SkyPass 画该 Asset 的 environment cube；PBR Set1 采同一 Asset 的 irradiance / prefilter（+ 引擎共享 BRDF LUT）。
- 无 `EngineIBLEnvironment` 全局单例外挂；无引擎层 `OpenGLRHIShaderResourceView` new。
- Bake（若启用）是 **离线/Editor one-shot**，契约与主帧 Pass 一致（现代语义），但不塞进帧图。

## 2) 现状

| 项 | 事实 |
|----|------|
| 构建 | `CMakeLists.txt` 排除 `EnvMapCapture.cpp` / `EngineIBLEnvironment.cpp` |
| 加载 | `ForwardRenderer::LoadEngineRenderingAssets` 仅 SkyBoxPass 自加载 cube |
| Set1 | IBL 三槽显式置 null（F03-M4 P0） |
| 组件 | `SkyBoxComponent` 仅有 Enabled / Intensity，**无 Asset ref** |
| 资产目录 | `Assets/EngineDefault/Textures/IBL/` + README 加载顺序仍有效（产品意图） |
| TD-015 | `EnvMapCapture::CreateSourceBinding` → `make_shared<OpenGLRHIShaderResourceView>`；glad / 手搓 mip；文件含半截坏代码，不能直接复开编译 |

## 3) 方案

### 3.0 易难判断（共识）

| 工作 | 难度 | 说明 |
|------|------|------|
| Asset 类型 + 序列化字段 | **中低** | 有 `Material` / `Texture2D` 先例；字段是若干纹理引用 + 元数据 |
| Component → Proxy → Pass/Set1 引用接线 | **中低** | 模式与 Mesh/Material 相同；Sky 场景已单 proxy |
| **GPU Bake（equirect→cube / irradiance / prefilter）** | **高** | 必须碰 GPU；旧实现是旁路语义，要按 **新 RHI 契约** 重写 |

**对「bake 要用 GPU → 碰渲染系统 → 要用新语义」的校正：**

- **对：** Bake 必须走 RHI（CreateTexture、BeginRenderPass 按 face/mip、PSO、BindingSet、`RHICreateShaderResourceView`），不能再 `#include glad` / 引擎层 `OpenGL*`。
- **不全对：** 不需要、也不应该把 Bake 嵌进 **每帧 ForwardRenderer / RDG**。它是 **工具路径**：拿到 `RHI&` + 临时 `RHICommandList`，跑完写出 `TextureCube` / 写回 Asset。阴影图是「帧内图拥有」；环境 Bake 是「资产生产」。

因此切片上：**先接线磁盘图（无 Bake 也能验收天空 + IBL）→ 再现代 Bake 工具付清 TD-015。**

### 3.1 数据模型（Draft）

**Asset：`EnvironmentMap`（名可再定）**

建议持有（一期全可磁盘引用，Bake 后回填）：

| 字段 | 用途 |
|------|------|
| Source HDR / equirect（可选） | Bake 输入 |
| Environment cube | SkyPass + prefilter 源 |
| Irradiance cube | diffuse IBL |
| Prefilter cube + mips | specular IBL |
| （不持有）BRDF LUT | **引擎共享**一张即可 |

**`SkyBoxComponent`**

- `AssetRef` / `std::shared_ptr<EnvironmentMap>`（与项目既有 Asset 引用风格对齐）
- 保留 `m_Enabled` / `m_SkyIntensity`
- `SkyBoxSceneProxy` 增加：环境纹理指针或已解析的 RHITexture*/SRV 句柄（由 Prepare 阶段从 Asset 解析）

**项目自有（硬约束）：** `AssetManager::NormalizeProjectRelativeAssetPath` **拒绝注册** `EngineDefaultAssetsRoot` 下路径。因此：

- EnvironmentMap **必须**落在 **Project Content**（如 `MyMEProject/Assets/...`），由 GUID/项目相对路径引用。
- `Assets/EngineDefault/Textures/IBL/` 只作 **种子/模板**（复制进项目后再注册），**禁止** Scene/`SkyBoxComponent` 直接挂引擎 Default 路径当 Asset。
- 一期可提供「从 EngineDefault 复制 IBL 目录到项目」的文档或后续 Editor 动作；运行时加载只认项目 Asset。

### 3.2 运行时数据流

```text
SkyBoxComponent.EnvironmentMap
        │
        ▼
SkyBoxSceneProxy（每帧/脏时同步）
        │
        ├─► SkyBoxPass：environment cube SRV
        └─► EngineSceneBindingSets::BuildSceneSet1：irradiance / prefilter / shared BRDF LUT
```

无 Sky 或 Asset 未就绪：SkyPass 可 clear-only；IBL 槽保持 null（与今类似），PBR 退化为无间接光。

### 3.3 Bake 子系统（后置切片）

- **入口：** Editor 菜单 / 资产导入管线 / 测试 CLI（择一，Impl 定）
- **实现：** 重写 `EnvMapCapture`（或新名 `EnvironmentMapBaker`）为：
  - 仅依赖 `RHI` / `RHICommandList` / 引擎 ShaderUtils
  - SRV 一律 `cmdList.CreateShaderResourceView` 或 ViewCache
  - 按 face（× mip）`BeginRenderPass` → SetPSO → SetBindingSet → Draw
- **禁止：** 引擎层 `OpenGLRHI*`、glad、与主帧共享可变 GL 状态假设
- **产出：** 写回 EnvironmentMap 的 GPU 纹理，并可选落盘 PNG/HDR 旁路文件

### 3.4 删除 / 退役

- 退役全局 `EngineIBLEnvironment` 作为运行时入口（逻辑可迁入 Asset loader / Baker）
- `SkyBoxPass::Initialize(engineDefaultRoot)` 不再「自己找图」；最多加载 shader/几何
- 付清 **TD-015**

### 3.5 与 F03 / F09 关系

- 关闭 F03「EnvMap 停用」尾项的一部分；F03 其它债务不在本 Feature 强绑。
- F09 已清 Set0/Material/Clear；本 Feature 受益于现代 SRV/Clear，不重复 F09 范围。

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A 仅恢复全局 EngineIBL | 快 | 违背「场景 Asset」目标 | 拒绝 |
| B Asset + 磁盘接线，Bake 后置 | 早验收；风险可控 | 无 HDR 时需预烘焙资源 | **选用（分期）** |
| C Asset + 每帧 RDG Bake | 概念统一 | 过重；静态环境浪费 | 拒绝 |
| D Sky 与 IBL 两个 Asset | 灵活 | 一期复杂度高 | Deferred |

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| Asset 引用风格不统一 | 序列化坑 | 对齐 Material/Texture 现有 ref 模式后再写 |
| Bake 半开污染主路径 | 编译/链接回归 | Bake 代码可独立目标或明确 `#if`；主路径只读磁盘 |
| 单元常量 / Set1 再纠缠 | 绑定错乱 | 继续只用 `EngineShaderBindings`；Slot 语义学 F08 |
| 无默认 Asset 的空场景 | 黑天 | 引擎默认 Content + 文档 |

## 6) 验收标准

- [x] `FEATURE_REGISTRY` / Design Status → 实施后 Done
- [x] SkyBoxComponent 可引用 EnvironmentMap；换 Asset 换天空
- [x] Set1 IBL 来自同一 Asset（有图时）；BRDF LUT 共享
- [x] 无引擎层 `OpenGLRHIShaderResourceView` 构造（grep 门禁）
- [x] `EnvMapCapture` / Baker 若编入：仅现代 RHI API
- [x] `verify.ps1` smoke PASS；黄金场景 PBR+天空目视（有默认 Asset 时）
- [x] TD-015 → Done（Bake 切片完成时；若仅接线则 Notes 标明 Bake 仍 Open 子项）

S06 Editor Bake UX → **TD-021**（不挡 Feature Done）。

## 7) Status note

（无）

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-03 | Draft：登记 F10；Asset+Component 模型；Bake 难、接线易；磁盘优先 |
| 2026-08-03 | Planned：项目自有 EnvMap（不可注册 EngineDefault）；开始 S01–S03 接线 |
| 2026-08-03 | S01–S03 Done：Asset/Loader/Sky/Set1；项目种子；Bake 仍 Open |
| 2026-08-03 | S05：项目 HDR bake 通；`CreateShaderResourceView`；TD-015 留 glad mip |
| 2026-08-03 | TD-015 Done：`RHICmdGenerateMips`；S04 删 EngineIBL/BrdfLut；S06→TD-021 |
| 2026-08-03 | Done：验收勾选；S06 挂 TD-021 |
