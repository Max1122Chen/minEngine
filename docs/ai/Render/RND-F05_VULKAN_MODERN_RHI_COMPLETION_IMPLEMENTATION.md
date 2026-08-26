# RND-F05 — Implementation Plan

## Meta
- **ID:** `RND-F05`
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-08-17
- **Related:** [Design](./RND-F05_VULKAN_MODERN_RHI_COMPLETION_DESIGN.md) · [ED-F01](../Editor/ED-F01_VULKAN_EDITOR_PARITY_IMPLEMENTATION.md)

## TL;DR

S01–S07d Done。**S07e/f 迁至 ED-F01**（在完整 Vulkan Editor 里验收 shadow / sky / IBL）。

## Scope
- **In:** Design §Scope In；S00–S06 Done；**S07a–S07f** 场景管线 VK 扩覆盖；含 CLI `--rhi`。
- **Out:** F11、PHYS-F03、单切片全管线 parity、ImGui-Vulkan Editor（→ **ED-F01**）、公共 Semaphore API、glslang 源码进仓。

## Reader quick start
1. [Design](./RND-F05_VULKAN_MODERN_RHI_COMPLETION_DESIGN.md) §3.5–§3.9、§7。
2. 本表切片顺序（尤其 **S07a–S07f**）。
3. 验证：各切片 Verify 行。

---

## 1) 切片总览

| Slice ID | 内容 | 状态 | 验证 |
|----------|------|------|------|
| `RND-F05-S00` | Design 定稿（§7） | **Done** | Meta Planned |
| `RND-F05-S01` | CMake/Vulkan SDK + `ShaderCompiler`（双目标 SPIR-V）+ 缓存 | **Done** | `test shader-compiler` PASSED |
| `RND-F05-S02` | GL 4.6；bytecode `RHICreateShader`；Present 走 SPIR-V | **Done** | `test shader-compiler`（含 GL specialize）PASSED；smoke PASSED |
| `RND-F05-S03` | CLI `--rhi`；`VulkanRHI` 设备/交换链 Clear/Present（semaphore/fence **内部**） | **Done** | `minEngineTests.exe --rhi vulkan test smoke` PASSED；Editor `--rhi vulkan` clear/present smoke PASS |
| `RND-F05-S04` | VK 最小图形 + SPIR-V | **Done** | Editor `--rhi vulkan` 彩色三角形 smoke PASS |
| `RND-F05-S05` | PresentPass / 中立 `BeginFrame`·`Present`（若需要）对齐双后端 | **Done** | `PresentFrame` → `RHIPresent`；上层无 `vulkan.h` |
| `RND-F05-S06` | 更多引擎 shader + MaterialCompiler `set=` 分批 | **Done** | SkyBox+EnvMapCapture SPIR-V；材质 `set=` + SPIR-V；`test smoke` / `material-ir` / `shader-compiler` PASSED |
| `RND-F05-S07a` | VK 资源：Buffer / Texture2D / SRV / VertexInputLayout + upload | **Done** | Editor `--rhi vulkan` buffer/texture/SRV/layout probe OK；GL/VK smoke PASS |
| `RND-F05-S07b` | VK Descriptor：SetLayout / PipelineLayout / BindingSet + bind | **Done** | Descriptor pool；set bind；validation 无致命错误（S07d 冒烟） |
| `RND-F05-S07c` | VK PSO from desc + `Begin/EndRenderPass` + Transition；退役硬编码三角 | **Done** | 经 RHICmd 路径 Present；lazy PSO per RenderPass |
| `RND-F05-S07d` | 启用 `ForwardRenderer`（VK）：无阴影 Base | **Done** | Editor `--rhi vulkan` Unlit mesh smoke（**人工目视已见 mesh 输出**）；GL/VK `test smoke` PASS |
| `RND-F05-S07e` | ShadowPass + 场景 shadows include `set=` | **Deferred → ED-F01-S06** | 见 [ED-F01 Impl](../Editor/ED-F01_VULKAN_EDITOR_PARITY_IMPLEMENTATION.md) |
| `RND-F05-S07f` | SkyBox + IBL/EnvMapCapture 热路径；关账 S07 | **Deferred → ED-F01-S07** | 见 ED-F01 Impl |

> 历史占位名 `RND-F05-S07+` 由上表取代。**RHI 竖切以 S07d 关账**；shadow/sky parity 由 **ED-F01** 承接。

---

## 2) 切片详情

### RND-F05-S00 — Design 定稿
- **DoD:** [x] 用户确认 Design §7（含 `--rhi`、同步内聚）

### RND-F05-S01 — ShaderCompiler 地基
- **Goal:** `Render/ShaderCompiler/`；SDK glslang；VK/GL 两份 SPIR-V；磁盘缓存。
- **DoD:**
  - [x] Present 可编译两份 bytecode
  - [x] CMake 发现 `glslangValidator` + Vulkan package
  - [x] `test shader-compiler` PASSED；`test smoke` PASSED
- **Verify:** `minEngineTests.exe test shader-compiler`；`test smoke`

### RND-F05-S02 — OpenGL 消费 SPIR-V（Present）
- **Goal:** GL **4.6**；Present SPIR-V specialize。
- **DoD:**
  - [x] GLFW / 测试上下文 **4.6**
  - [x] `RHIShaderCreateDesc` + `RHICreateShader(bytecode)`；OpenGL `glShaderBinary` + `glSpecializeShader`
  - [x] `EngineShaderUtils::CreateShaderFromSpirvFiles`；PresentPass 热路径走 SPIR-V（无 GLSL 字符串 compile）
  - [x] `test shader-compiler`（编译 + GL load）PASSED；`test smoke` PASSED
- **Verify:** `minEngineTests.exe test shader-compiler`；主视口 Present（手动）

### RND-F05-S03 — CLI + Vulkan 交换链
- **Goal:** `--rhi opengl|vulkan`（默认 opengl）；`VulkanRHI` Clear/Present；**frame sync 仅内部**。
- **Touch:** `ApplicationCommandLine` / `CommandLineResult`；`RenderSystem`；`Render/Vulkan/*`；Window `NO_API`。
- **DoD:**
  - [x] 不传参 = OpenGL
  - [x] `--rhi vulkan` 无需重编译即可跑清屏 Present
  - [x] 公共头无 `VkSemaphore` / `VkFence`
- **Verify:** `minEngineTests.exe --rhi opengl test smoke`；`minEngineTests.exe --rhi vulkan test smoke`；`Editor.exe --rhi vulkan --project ...`（clear/present smoke）

### RND-F05-S04 — Vulkan 最小绘制
- **Goal:** SPIR-V + PSO + draw。
- **DoD:**
  - [x] `VulkanRHI` render pass + graphics pipeline（内嵌 triangle SPIR-V）
  - [x] Editor `--rhi vulkan` 可见彩色三角形
- **Verify:** Editor `--rhi vulkan` smoke PASS

### RND-F05-S05 — PresentPass 对齐
- **Goal:** 同一 PresentPass/CommandList；必要时中立 `BeginFrame`/`Present`。
- **DoD:** [x] 上层 Present 无 `vulkan.h`（`RenderSystem::PresentFrame` → `RHIPresent`；Editor/Engine 走该路径）
- **Verify:** `--rhi` 切换对照（OpenGL Editor + Vulkan smoke）

### RND-F05-S06 — 着色器方言分批
- **Goal:** includes / MaterialCompiler 跟进 `set=`；双目标 SPIR-V。
- **Done:**
  - [x] SkyBox `background` → SPIR-V
  - [x] EnvMapCapture（equirect / irradiance / prefilter）→ SPIR-V
  - [x] MaterialCompiler 材质参数 `layout(set=kSetMaterial, binding=…)`；`ShaderCompiler` OpenGL 路径 flatten `set=`
  - [x] `Material::CommitCompileResult` → `CreateShaderFromSpirvSources`
- **Verify:** Editor OpenGL 天空/IBL bake + 材质；`test smoke` / `material-ir` / `shader-compiler`

### RND-F05-S07a — Vulkan 资源对象
- **Goal:** 让 `VulkanRHI` 能创建并上传 **Buffer / Texture2D / SRV / VertexInputLayout**，为后续 draw 提供数据面。
- **现状（代码真值）：** `VulkanRHIBuffer`（host-visible map）、`VulkanRHITexture`（Texture2D + staging upload）、`VulkanRHIShaderResourceView`、`VulkanRHIVertexInputLayout`；`VulkanRHIAllocator` 共享分配。
- **Touch:** `Vulkan/VulkanRHI*.cpp/.h`；`VulkanRHIResources.*`。
- **DoD:**
  - [x] `RHICreateBuffer`（Vertex/Index/Uniform）+ host→device 上传路径可用
  - [x] `RHICreateTexture2D` + `RHICreateShaderResourceView`（Texture2D color；depth 格式可创建）
  - [x] `RHICreateVertexInputLayout` 可描述引擎 mesh 布局
  - [x] 生命周期：Shutdown / 资源析构不泄漏 Vk 对象
  - [x] **Out：** Descriptor / PSO / 启用 ForwardRenderer（留给后续切片）
- **Verify:** Editor `--rhi vulkan`：S07a buffer/texture/SRV/layout probe OK；`minEngineTests.exe --rhi opengl|vulkan test smoke` PASS。

### RND-F05-S07b — Descriptor / BindingSet
- **Goal:** 实现 `RHICreateShaderBindingSetLayout` / `RHICreatePipelineLayout` / `RHICreateShaderBindingSet` + `RHICmdSetShaderBindingSet`，按 **逻辑 (set, binding)** 绑资源（`EngineShaderBindings` 真源）。
- **Touch:** `VulkanRHI*`；对照 `OpenGLRHI` 绑定契约（行为对齐，非抄 GL API）。
- **DoD:**
  - [x] Descriptor pool + set 分配；更新 UBO/SRV 绑定
  - [x] PipelineLayout 由多个 SetLayout 组成；Layout API 可用（为 S07f 消除 EnvMap `PipelineLayout is null` 铺路）
  - [x] Frame-in-flight 下不每帧无限泄漏 set（「每帧 reset pool」或「缓存 set」——实现时写清）
  - [x] **Out：** 完整 BasePass；场景 include 全量改 `set=`（与 S07e 合并）
- **Verify:** S07d Editor smoke + `VK_LAYER_KHRONOS_validation` 无致命错误；GL 回归。

### RND-F05-S07c — PSO + RenderPass 命令路径
- **Goal:** `RHICreateGraphicsPipelineState(desc)` 与 `RHICmdBegin/EndRenderPass` / `SetViewport` / `Draw*` / `Transition` 真正驱动命令缓冲；**Present/绘制走同一 `RHICmd*` 契约**，不再依赖「仅内部硬编码三角」作为唯一出图路径。
- **拍板默认（见 Design §3.9）：** 首版用 **经典 `VkRenderPass` + Framebuffer** 映射现有 `RHIRenderPassInfo`；**不**首刀上 dynamic rendering。
- **Touch:** `VulkanRHI.cpp`；`RHIGraphicsPSODesc` 消费；Present 帧录制与 `RHIPresent` 提交顺序。
- **DoD:**
  - [x] 从 `RHIGraphicsPSODesc`（含 `VulkanRHIShader` modules）创建 graphics pipeline
  - [x] `BeginRenderPass` / `EndRenderPass` 写命令缓冲；`Draw` / `DrawIndexed` 生效
  - [x] `RHICmdTransition` 对 color/depth 做真 image layout（覆盖本切片用到的路径）
  - [x] 硬编码 S04 三角：**降级为 debug-only 或删除**（推荐验收后删除，避免双路径）
  - [x] **Out：** 启用 `ForwardRenderer`；Shadow / IBL
- **Verify:** Editor `--rhi vulkan` 经 RHICmd 路径出图；`test smoke` GL + VK。

### RND-F05-S07d — ForwardRenderer on Vulkan（无阴影 Base）
- **Goal:** `RenderSystem` 在 `--rhi vulkan` 下 **创建并 Initialize `ForwardRenderer`**（今日 Vulkan 分支直接 `return` 跳过）；跑通 **无阴影** Base（优先 Unlit 或关闭 ShadowPass 的简易 Lit）。
- **Touch:** `RenderSystem.cpp`；必要时 Pass 仅做能力 gated（禁止上层 `#include vulkan.h`）；场景 RT/depth 经 S07a–c。
- **验证方式（见 Design §3.9）：** ImGui-Vulkan 仍 Out → Editor 保持 VK smoke（无完整 UI），但驱动 `ForwardRenderer` 并把 scene color Present；**和/或** Engine/测试一帧 mesh 烟测。二者有一即可验收。
- **DoD:**
  - [x] VK 下 `ForwardRenderer` 初始化成功；关键资源创建不 stub-fail
  - [x] 至少一条 mesh draw（材质 SPIR-V / Unlit）可见 — Editor smoke 强制 Unlit 并 `SubmitSceneDraw(PresentToBackBuffer)`；**请人工目视 mesh**
  - [x] ShadowPass **可跳过或 noop**（本切片不要求阴影正确）
  - [x] `--rhi opengl` Editor 全功能回归（`test smoke` PASS；Editor GL 未在本批重跑 UI）
- **Verify:** `Editor.exe --rhi vulkan --project …\MyMEProject.meproject`（人工目视 mesh 可见）；`minEngineTests.exe --rhi opengl|vulkan test smoke` PASS。
- **Notes:** Lit 材质仍依赖场景 include flat `binding=`（set0）→ **S07e**；depth RT 不用默认 `SAMPLED`；`DEPTH24STENCIL8`→`D32_SFLOAT_S8_UINT`；`PerFrame` stageFlags=`All`。后续收口见 `TD-023`（scene pass / clear contract）与 `TD-024`（Vulkan frame sync + debug leftovers）。

### RND-F05-S07e — Shadow + 场景 binding 方言
- **Goal:** ShadowPass 在 VK 上产出 shadow map；主光采样正确；**场景 includes**（`MaterialSceneShadows.glslinc` 等）补齐 `layout(set=…, binding=…)`，GL 继续 flatten。
- **Touch:** `ShadowPass.cpp`（仅 RHI）；`MaterialSceneShadows.glslinc` / Phong|PBR includes；`EngineShaderBindings` 文档一致。
- **DoD:**
  - [ ] Directional（CSM）阴影在 VK 可辨（允许与 GL 略有差，但无「全黑/全亮」回归）
  - [ ] 场景 set1 阴影/灯光与 set2 材质不冲突
  - [ ] OpenGL：`material-ir` / smoke / 主视口阴影仍正确
- **Verify:** Editor GL 阴影对照；VK smoke/测试对照；`test material-ir`。

### RND-F05-S07f — Sky / IBL / Bake 收口
- **Goal:** SkyBoxPass + IBL 采样 + EnvMapCapture bake 在 VK 热路径可用；消除 bake 路径 `PipelineLayout is null` 类 WARN。
- **Touch:** `SkyBoxPass`、`EnvMapCapture`、IBL 绑定；必要时 cubemap / `GenerateMips` VK 实现。
- **DoD:**
  - [ ] VK 天空盒可见；IBL 对现有路径有贡献或明确 gated
  - [ ] EnvMapCapture bake 在 VK 不因 null layout 静默失败
  - [ ] 上层仍无 `vulkan.h`；公共 RHI 无 Semaphore
  - [ ] 文档：S07a–f Done；Registry / ACTIVE_WORK / Progress 更新
- **Verify:** GL Editor 全功能回归；VK 天空/IBL 手动；相关 tests。

---

## 3) 非本计划

- RND-F11 / PHYS-F03
- ImGui Vulkan Editor → **[ED-F01](../Editor/ED-F01_VULKAN_EDITOR_PARITY_DESIGN.md)**
- 公共 RHI 暴露 Semaphore/Fence/Queue
- glslang 源码进仓库
- 单切片「一次写完」全 Forward↔VK parity（Editor 环境见 ED-F01）

---

## 4) 依赖与顺序（历史 — S07d 前）

```text
S07a 资源 ──► S07b Descriptor ──► S07c PSO/Cmd ──► S07d Forward(Base)  [Done]
                                                      │
                                                      └──► ED-F01（Editor + S07e/f）
```

- **禁止**跳过 S07a–c 直接改 `ForwardRenderer`「特判 Vulkan」。
- **GL 回归**是每刀硬门槛。
- **S07d 起**才改 `RenderSystem` 取消「Vulkan 跳过 ForwardRenderer」。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-04 | 初稿 S00–S07+ |
| 2026-08-04 | S00 Done；S03 纳入 `--rhi` 与内部帧同步；对齐 Design §3.5–§3.6 |
| 2026-08-04 | **S01 Done**：`ShaderCompiler` + Present location 修正；`test shader-compiler` / smoke PASS |
| 2026-08-04 | **S02 Done**：GL 4.6；bytecode `RHICreateShader`；Present SPIR-V hot path；suite 含 GL specialize |
| 2026-08-04 | **S04 Done**：Vulkan 最小 graphics pipeline + SPIR-V triangle smoke |
| 2026-08-04 | Post-process FXAA/Sharpen → OpenGL SPIR-V；`layout(location)` 修正 |
| 2026-08-04 | **S05 启动**：`RenderSystem::PresentFrame()`；Editor OpenGL 走 RHI present |
| 2026-08-04 | `CreateShaderFromSpirvFiles` 按 `--rhi` 选 VK/GL SPIR-V；`VulkanRHIShader` bytecode；ShadowPass SPIR-V |
| 2026-08-04 | **S05 Done**：上层无 `vulkan.h`；Present 经 `PresentFrame` |
| 2026-08-04 | **S06 启动**：SkyBox `background` → SPIR-V（首批） |
| 2026-08-04 | **S06 Done**：EnvMapCapture SPIR-V；Material `set=` + SPIR-V；OpenGL flatten（含 std140）；material-ir/smoke/shader-compiler PASS |
| 2026-08-05 | **S07 子切片表起草（待审）**：S07a–S07f；替换笼统 S07+ |
| 2026-08-05 | **S07 表批准**；**S07a Done**：VK Buffer/Texture2D/SRV/VertexInputLayout + init probes |
| 2026-08-17 | **S07d 目视验收通过**：Vulkan Editor smoke 已见 mesh 输出；登记 `TD-023` / `TD-024` 跟踪 scene pass 契约与 VulkanRHI 收口债 |
| 2026-08-17 | **F05 关账**；S07e/f Deferred → **ED-F01** |
