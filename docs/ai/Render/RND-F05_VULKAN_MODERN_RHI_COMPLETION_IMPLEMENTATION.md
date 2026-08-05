# RND-F05 — Implementation Plan

## Meta
- **ID:** `RND-F05`
- **Status:** In Progress
- **Owner:** project maintainer
- **Last updated:** 2026-08-04
- **Related:** [Design](./RND-F05_VULKAN_MODERN_RHI_COMPLETION_DESIGN.md)

## TL;DR

S01–S06 Done（含 SkyBox / EnvMapCapture SPIR-V、MaterialCompiler `set=` + 材质走 SPIR-V）。**下一刀：S07+** — 为 Vulkan 场景管线补子切片表（资源/PSO → Base → Shadow…）。

## Scope
- **In:** Design §Scope In 对应切片 S00–S06（后续可加 S07+）；含 CLI `--rhi`。
- **Out:** F11、PHYS-F03、全管线一次性 parity、ImGui-Vulkan（单独后置）；公共 Semaphore API。

## Reader quick start
1. [Design](./RND-F05_VULKAN_MODERN_RHI_COMPLETION_DESIGN.md) §3.5–§3.6、§7。
2. 本表切片顺序。
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
| `RND-F05-S07+` | Base/Shadow/IBL… VK 扩覆盖 | Planned | 子切片另开 DoD |

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

### RND-F05-S07+ — 场景管线扩 VK
- **Goal:** Base → Shadow → IBL…；`RHICmdTransition` 真语义。
- **DoD:** 开干前补子表（资源 stub 填实 → 最小 forward → …）

---

## 3) 非本计划

- RND-F11 / PHYS-F03
- ImGui Vulkan Editor
- 公共 RHI 暴露 Semaphore/Fence/Queue
- glslang 源码进仓库

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
