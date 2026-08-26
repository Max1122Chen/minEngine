# ED-F01 — Vulkan Editor Parity

## Meta
- **ID:** `ED-F01`
- **Type:** Feature
- **Status:** In Progress
- **Owner:** project maintainer
- **Last updated:** 2026-08-17（S01–S04 实现落地）
- **Depends on:** `RND-F05` S01–S07d **Done**（VKRHI 核心 + Forward Base smoke）
- **Related:** [Implementation](./ED-F01_VULKAN_EDITOR_PARITY_IMPLEMENTATION.md) · [RND-F05 Design](../Render/RND-F05_VULKAN_MODERN_RHI_COMPLETION_DESIGN.md) · [RND-F05 Impl](../Render/RND-F05_VULKAN_MODERN_RHI_COMPLETION_IMPLEMENTATION.md) · [Editor Shell](./EDITOR_SHELL_DESIGN.md)

## TL;DR

`RND-F05` S07d 已证明 VK Forward 能出 mesh，但 Editor 仍走 **无 ImGui 的 smoke 分叉**（全窗口 `PresentToBackBuffer`、无 navigation）。本 Feature 在 **不推翻 F05 RHI 成果** 的前提下，把 `--rhi vulkan` 的 Editor **逐步恢复到与 OpenGL 相同的产品形态**：ImGui-Vulkan → 统一主循环 → 内嵌 scene viewport + navigation → 再在同一 Editor 里完成 shadow / sky / IBL 等剩余渲染能力（承接原 F05 S07e/f）。

## Scope
- **In:**
  - 从同级 `../imgui` clone 引入 `imgui_impl_vulkan`（与仓内 ImGui **1.92.7** 对齐）。
  - `Editor` 在 Vulkan 下走与 OpenGL **同一套** Shell 主循环（ImGui + GUIManager + InputHub + SubModule）。
  - **删除**长期维护的 `m_VulkanSceneSmokeMode` / 独立 Run 循环；smoke 验收保留在 **tests** 或 debug 开关（非日常 Editor 路径）。
  - Scene viewport：**离屏 RT** + ImGui 内嵌显示 + `SceneEditingViewportClient` navigation（与 GL 同架构：`SetPresentPassEnabled(false)`）。
  - Editor 侧 **RHITexture → ImGui** 绑定抽象（VK 用 `VkDescriptorSet`；GL 继续 `GLuint` handle）。
  - `VulkanRHI` 与 ImGui 的 **Editor 专用桥接**（device/swapchain/render pass/descriptor pool/frame 顺序）；**不**把 Semaphore/Fence 类型暴露到公共 `RHI`。
  - 原 `RND-F05-S07e`（Shadow + 场景 `set=`）与 `S07f`（Sky / IBL / Bake）在本 Feature 内、**完整 Editor 环境**下继续推进。
  - 每刀 GL Editor 回归 + `test smoke`（双后端）。
- **Out（本 Feature 或近端不做）:**
  - ImGui **multi-viewport / 独立 OS 窗口**（可后置；首阶段仅 main viewport + dock）。
  - 一次性「全 Editor 像素级 GL parity」（Material 预览、缩略图、Inspector 纹理等 **分切片** 跟进，见 Implementation）。
  - `RND-F11` DebugDrawing。
  - 公共 `RHI` 暴露 Vulkan 同步原语。
  - 重写 Editor Shell / SubModule 架构（沿用现有 `EDITOR_SHELL_DESIGN`）。

## Reader quick start
1. 本文件 — 架构、帧顺序、与 F05 边界。
2. [Implementation](./ED-F01_VULKAN_EDITOR_PARITY_IMPLEMENTATION.md) — S01–S08 切片与验收。
3. 代码入口：`Editor/src/Editor.cpp`、`Editor/src/UI/EditorWindows/EditorViewportWindow.cpp`、`Render/Vulkan/VulkanRHI.*`、`../imgui/backends/imgui_impl_vulkan.*`。

---

## 0) Pre-flight（2026-08-17）

| 项 | 结论 |
|----|------|
| 扫描 | F05 S07d Done；Editor 仅 GL 有 ImGui；VK 为 smoke 分叉；Third-Party 无 `imgui_impl_vulkan`；同级 `../imgui` 有官方 backend（1.92.7 对齐） |
| 前置 | VKRHI 资源/descriptor/PSO/cmd/Forward Base **sound**；OpenGL Editor **regression baseline** 可用 |
| 债风险 | **medium** — 帧同步（`TD-024`）、swapchain resize、动态 RT 的 ImGui descriptor 生命周期 |
| WIP | `feat/render` 上 F05 文档/代码已提交 S07d；无并行 Editor-VK 大改 |
| 建议 | **Go** — 以 Editor parity 为主轴，比继续扩 smoke 更符合「逐渐恢复 GL 同等能力」 |

---

## 1) 背景与目标

### Pain
- `--rhi vulkan` 无法使用 dock UI、Inspector、scene viewport navigation。
- Smoke 路径与 GL Editor **两套行为**，每加 shadow/sky 都要在 smoke 里单独补观察手段。
- `GetRHINativeTextureHandle` + `ImGui::Image(GLuint)` 无法直接用于 Vulkan。

### 成功长什么样
- `--rhi vulkan --project …` 打开 **完整 Editor**（首阶段可先 gated 部分窗口）。
- Scene 视口：orbit/fly、选择、gizmo（与 GL 同输入模型）。
- 渲染特性（shadow、sky、IBL）在 **同一 Editor** 里开发与目视验收。
- `--rhi opengl` 行为不变，作为每刀回归基准。

---

## 2) 现状

### OpenGL Editor（目标参照）

```text
Initialize: ImGui + OpenGL3 backend + RegisterModules + OpenProject
Run loop:
  PollEvents → LogicalTick → GUIManager.Tick → InputHub
  → viewport EndFrame → SubmitSceneDraw(离屏 RT, shadows/sky flags)
  → TickRendererFrame → ImGui::Render → ImGui_ImplOpenGL3_RenderDrawData
  → PresentFrame / SwapBuffers
PresentPass: disabled（scene 不进 swapchain PresentPass）
```

### Vulkan smoke（待退役）

```text
Initialize: 跳过 ImGui；m_VulkanSmokeViewport；固定相机；m_VulkanSceneSmokeMode=true
Run loop:
  PollEvents → LogicalTick → SubmitSceneDraw(PresentToBackBuffer) → PresentFrame
PresentPass: enabled
```

关键代码：`Editor.cpp` 中 `RHIBackendSelection::IsVulkan()`  early-return 与独立 `Run()` 分支；`Editor.h` 中 `m_VulkanSceneSmokeMode` / `m_VulkanSmokeViewport`。

### 资产来源
- 仓内：`minEngine/Third-Party/imgui/`（1.92.7，仅 `imgui_impl_glfw` + `imgui_impl_opengl3`）。
- 同级 clone：`../imgui/backends/imgui_impl_vulkan.{h,cpp}` — **按需复制**进 Third-Party（不 submodule 整个 imgui，避免与 vendored 核心版本漂移）。

---

## 3) 方案

### 3.1 总原则

1. **架构对齐 GL，不发明第三套 Editor。**
2. **Smoke 分叉删除**，不是「smoke + 一点 ImGui」。
3. **Editor ↔ Vulkan 细节** 经 **桥接层**（`Editor/` 或 `Render/Vulkan/Editor/`），公共 `RHI.h` 保持 API 中立。
4. **能力分层恢复**：ImGui 空壳 → viewport → navigation → shadow → sky → 其他 ImGui 纹理消费者。

### 3.2 帧顺序（Vulkan Editor 目标）

```text
┌─────────────────────────────────────────────────────────────┐
│ 1. Acquire swapchain image (VulkanRHI 内部，已有)            │
│ 2. Scene draws → SceneViewport offscreen color/depth RT      │
│    (ForwardRenderer, PresentPass OFF, 与 GL 相同 flags)      │
│ 3. ImGui::NewFrame … UI … ImGui::Render                      │
│ 4. ImGui_ImplVulkan_RenderDrawData → swapchain image RP      │
│ 5. EndCommandBuffer → Submit → Present                       │
└─────────────────────────────────────────────────────────────┘
```

与 smoke 的差异：**scene 不再 `PresentToBackBuffer`**；swapchain 仅承载 ImGui 合成结果（与 GL「ImGui 画在 default FBO」同角色）。

### 3.3 模块边界

| 模块 | 职责 |
|------|------|
| `Editor.cpp` | 按 `--rhi` 选 ImGui renderer backend；**单一** Run 循环 |
| `EditorImGuiBackend`（新，Editor 内） | 封装 GL/VK ImGui init/shutdown/newframe/render |
| `EditorRHIImGuiTexture`（新，Editor 内） | `RHITexture*` → `ImTextureID`；VK 注册/注销 `ImGui_ImplVulkan_AddTexture` |
| `VulkanRHI` | 继续负责 device/swapchain/cmd；**新增** Editor 桥接访问（swapchain RP、image index、与 ImGui 共用的 render pass 或 dynamic rendering 参数） |
| `EditorViewportWindow` | 继续 `ImGui::Image`；纹理 ID 改经 `EditorRHIImGuiTexture` |
| `SceneEditingViewportClient` | **无 RHI 分叉**；EndFrame 提交离屏 draw（与 GL 相同） |

**禁止：** 在 `SceneEditingViewportClient` / Pass 里 `#ifdef VULKAN` 特判 smoke。

### 3.4 ImGui-Vulkan 接入要点

参考 `../imgui/examples/example_glfw_vulkan/main.cpp` 与 `imgui_impl_vulkan.h`：

| 项 | 做法 |
|----|------|
| Platform | 已有 `ImGui_ImplGlfw_InitForVulkan`（Third-Party 已有 glfw backend） |
| Renderer | 复制 `imgui_impl_vulkan.cpp/.h`；CMake 仅在 `MINENGINE_HAS_VULKAN` 时编译 |
| `ImGui_ImplVulkan_InitInfo` | 从 `VulkanRHI` 桥接填充：Instance、PhysicalDevice、Device、Queue、MinImageCount、MSAA、**RenderPass**（与 swapchain 兼容） |
| Descriptor pool | ImGui 要求 `FREE_DESCRIPTOR_SET_BIT`；可与引擎 pool **分离**（推荐 ImGui 专用 pool，避免与 scene descriptor 争用） |
| User texture | `ImGui::Image` 的 `ImTextureID` = `VkDescriptorSet`；scene RT resize 时 **invalidate 并重建** ImGui 注册 |
| Loader | 与 F05 一致：Vulkan SDK + 可选 `IMGUI_IMPL_VULKAN_NO_PROTOTYPES` / 项目现有 loader 策略 |
| 版本 | 必须与仓内 ImGui **1.92.7** 同步；升级 ImGui 时同步 backend |

### 3.5 RHITexture → ImGui 抽象

```cpp
// Editor 层（示意）
struct EditorImGuiTextureBinding
{
    ImTextureID TextureId = ImTextureID_Invalid;
    void Invalidate(); // viewport resize / RT 重建
};

EditorImGuiTextureBinding RegisterEditorImGuiTexture(RHI* rhi, RHITexture* texture);
void UnregisterEditorImGuiTexture(RHI* rhi, EditorImGuiTextureBinding& binding);
```

- **OpenGL：** 内部仍用 `GetRHINativeTextureHandle` → `GLuint`。
- **Vulkan：** `VulkanRHITexture` 暴露 image view + sampler（或 SRV 对象），调用 `ImGui_ImplVulkan_AddTexture`；**Editor 专用头**，不进 `RHI.h`。
- **消费者迁移顺序：** 主 Scene viewport（S04）→ Material viewport（S08）→ Inspector 预览 / 缩略图（S09+）。

### 3.6 Editor 主循环统一

- `Initialize`：Vulkan 分支 **不再 early-return**；与 GL 相同路径初始化 ImGui（backend 不同）、`RegisterModules`、`OpenProject`。
- `Run`：**删除** Vulkan 专用 `while`；统一循环内 `ImGui_Impl*_{NewFrame,RenderDrawData}` 由 `EditorImGuiBackend` 派发。
- `SetPresentPassEnabled(false)`：**双后端 Editor 统一**（VK 与 GL 一致）。
- `Shutdown`：按 backend 对称 shutdown。

### 3.7 自 F05 迁入的渲染切片

原 `RND-F05-S07e/f` 内容 **不变更技术目标**，仅 **变更验收环境**：

| 原 Slice | 迁入 ED-F01 | 验收环境 |
|----------|-------------|----------|
| S07e Shadow + 场景 `set=` | S06 | VK Editor viewport + GL 阴影对照 |
| S07f Sky / IBL / Bake | S07 | VK Editor 天空/IBL 目视 + GL 回归 |

### 3.8 Smoke 验收保留策略

| 用途 | 做法 |
|------|------|
| CI / 快速 RHI 回归 | 继续 `minEngineTests.exe --rhi vulkan test smoke` |
| 无 UI 的一帧 mesh | 可选保留 `Engine` 级 debug 或 test fixture，**不**作为 Editor 主路径 |
| Editor `--rhi vulkan` | **即完整 Editor**（本 Feature 交付物） |

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. 继续扩 smoke + fly cam | 改动小 | 第三套行为；无法验 Inspector/材质/ dock | **拒绝** |
| B. GL Editor 开发，VK 仅 tests | 零 ImGui-VK 成本 | 无法在日常 Editor 验 VK 渲染 | 作 **并行回归**，不作主路径 |
| C. 本 Feature：ImGui-VK + 统一 Editor + 分层 parity | 对齐目标；后续 shadow/sky 在真实环境开发 | 工作量中等偏大 | **选用** |
| D. 公共 RHI 暴露 `GetImGuiTextureHandle` | 调用简单 | 污染 RHI；GL/VK 语义不同 | **拒绝**；Editor 桥接 |
| E. ImGui 独立 swapchain / 完全分离 Present | 与官方 example 同构 | 与现有 `VulkanRHI` 帧模型重复 | **拒绝**；ImGui 画进现有 swapchain RP |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| ImGui render pass 与 swapchain 不兼容 | 黑屏 / validation error | S02 专刀对齐 RP format/load/store；对照 `example_glfw_vulkan` |
| 帧同步顺序（scene + ImGui 同 cmd buffer 或分 buffer） | 闪烁、TD-024 类问题 | 首版：**同一 command buffer、scene 先于 ImGui**；resize 走完整 teardown |
| Viewport RT resize 导致 ImGui descriptor 泄漏/陈旧 | 花屏、crash | `EditorRHIImGuiTexture` 显式 Invalidate；viewport `ApplyPendingResize` 挂钩 |
| Descriptor pool 耗尽（多 viewport / 缩略图） | 运行时 VK error | ImGui 独立 pool；动态纹理池 sizing 写进 S04 DoD |
| 双后端 CMake / 条件编译漂移 | 一方编不过 | CI 或本地 habit：`--rhi opengl\|vulkan` 各编 Editor |
| F05 文档与实现分叉 |  agent/人误读 backlog | Registry + F05 明确 S07e/f **迁至 ED-F01**；ACTIVE_WORK 更新 |

---

## 6) 验收标准（Feature 级）

- [ ] `--rhi vulkan` 启动完整 Editor（dock + 菜单 + Scene 模块），**无** smoke 专用循环。
- [ ] Scene viewport：离屏 RT 显示在 ImGui 内；orbit/fly navigation 与 GL 同等可用。
- [ ] `--rhi opengl` Editor 全功能无回归（`test smoke` + 人工 UI 抽查）。
- [ ] Shadow（原 S07e）在 VK Editor viewport 可辨；GL 阴影仍正确。
- [ ] Sky / IBL / Bake（原 S07f）在 VK Editor 热路径可用或明确 gated；GL 回归绿。
- [ ] 公共 `RHI` 无 Semaphore/Fence；上层 Pass 无 `vulkan.h`。
- [ ] `FEATURE_REGISTRY` / `ACTIVE_WORK` / `PROGRESS_LOG` 随切片更新。

---

## 7) 与 RND-F05 的关系

| 项目 | 结论 |
|------|------|
| F05 范围 | **S01–S07d Done** = VKRHI 竖切 + Forward Base smoke **已达成** |
| F05 S07e/f | **迁至 ED-F01-S06/S07**；不在 smoke 路径验收 |
| F05 Status | 建议 Registry 标记 **Done**（RHI 核心）；Editor/scene parity 由 **ED-F01** 承接 |
| Design §7 #6「第一阶段不要 ImGui」 | **仍有效 historically**；S07d 后进入 ED-F01 阶段 |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-17 | 初稿：F05 S07d 后 Editor-VK parity；S07e/f 迁入；smoke 分叉计划删除 |
