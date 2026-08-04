# RND-F05 — Vulkan backend, SPIR-V, modern RHI completion

## Meta
- **ID:** `RND-F05`
- **Type:** Feature
- **Status:** In Progress
- **Owner:** project maintainer
- **Last updated:** 2026-08-04
- **Depends on:** `RND-F03` **Done** · `RND-F04` **Done**
- **Related:** [Implementation](./RND-F05_VULKAN_MODERN_RHI_COMPLETION_IMPLEMENTATION.md) · [RND-F02](./RND-F02_MODERN_RHI_DESIGN.md) · [RND-F03](./RND-F03_LEGACY_RHI_REMOVAL_DESIGN.md) · [RND-F04](./RND-F04_MODERN_RHI_EVOLUTION_DESIGN.md)

> **Legacy mapping：** 曾登记为 RND-F04；Vulkan 顺延为 F05。

## TL;DR

在**已清洁的现代 RHI 调用面**上引入 **Vulkan 第二后端**，并把着色器交付改为 **GLSL 源 → SPIR-V**（**OpenGL 与 Vulkan 都吃 SPIR-V**）。终态：上层 Pass/RDG/材质/阴影/IBL **同一套管线**可在 GL/VK 间切换；光追等 API 限制除外。本 Feature **多刀竖切**，禁止「一次写完整个 Vulkan 引擎」。

## Scope
- **In:**
  - Vulkan 实例/设备/队列/表面/交换链；`VulkanRHI` 实现同一 `RHI` / `RHICmd*` 契约（按切片扩覆盖面）。
  - **ShaderCompiler（引擎侧）**：GLSL → SPIR-V（依赖本机 Vulkan SDK / `glslangValidator`；已确认 `VULKAN_SDK=…\1.4.350.0`）。
  - **双后端 SPIR-V 消费**：VK `VkShaderModule`；GL `glShaderBinary` + `glSpecializeShader`（需 **OpenGL 4.6** / `GL_ARB_gl_spirv`）。
  - 着色器源迁移到 **可双编译** 的绑定方言（见 §3.3）；`RHICreateShader` 契约改为以 **bytecode** 为主。
  - 窗口/Present 抽象：GL 上下文 vs `GLFW_NO_API` + VK surface；**启动参数 `--rhi`**（默认 OpenGL）。
  - Vulkan 同步原语（semaphore/fence/acquire）**仅后端内部**；公共面不暴露。
  - 现代 RHI 在 VK 上补齐的语义（transition、真 PSO、descriptor），随切片推进，不一次做完。
- **Out（近端）:**
  - 单切片全管线 GL/VK parity。
  - DebugDrawing（`RND-F11`）。
  - PHYS-F03 / 玩法。
  - 光追、多队列通用计算、绑定无关（bindless）大改。
  - 把 glslang **源码** vendoring 进仓库（优先 **SDK 工具链**；若日后离线 CI 需要再议 submodule）。
  - 立刻删除所有运行时 `glCompileShader(GLSL)`（允许迁移窗内 fallback，有删除切片）。

## Reader quick start
1. 本文件 — 地基评估结论、SPIR-V/绑定策略、架构、切片原则。
2. [Implementation](./RND-F05_VULKAN_MODERN_RHI_COMPLETION_IMPLEMENTATION.md) — S00–S0n 可执行切片。
3. 代码入口：`Render/RHI/RHI.h`、`OpenGL/OpenGLRHI*`、`EngineShaderUtils`、`EngineShaderBindings.h`、`GLFWWindowSystem`、`RenderSystem.cpp`。

---

## 0) Pre-flight（2026-08-04）

| 项 | 结论 |
|----|------|
| 扫描 | F03/F04 Done；调用面现代；无 VK/SPIR-V 代码；CMake 未链 Vulkan |
| 前置 | 现代 RHI **sound**；窗口仍 **GL-only**；着色器仍 **运行时 GLSL 字符串** → SPIR-V **missing** |
| 债风险 | **medium** — 绑定表「逻辑 set」vs「GL 扁平 binding」与 SPIR-V 双端不一致，必须先定方言 |
| WIP | 主线空出；物理冷冻 — **proceed** |
| 建议 | **Go with scope cut**：先 **ShaderCompiler + GL 上 SPIR-V 竖切**，再 **VK Present**，再扩管线 |
| 性质 | **真 Feature**（新后端 + 着色器交付重构），非 band-aid |

---

## 1) 背景与目标

### Pain
- 仅 OpenGL 后端；无法验证 RHI 契约是否真正后端中立。
- `RHICreateShader(std::string vs, std::string fs)` + `glShaderSource`/`glCompileShader` 把 **GLSL 文本**钉死在 GL 路径（`OpenGLRHIResources.cpp`、`EngineShaderUtils.cpp`）。
- 无 Vulkan SDK 集成；`RenderSystem` 写死 `OpenGLRHI`；`GLFWWindowSystem` 创建 GL 上下文；ImGui 走 `imgui_impl_opengl3`。

### Goals
- **双后端**：同一上层可切换 OpenGL / Vulkan。
- **SPIR-V 为 GPU 着色器交付物**（GL + VK）；GLSL 仍为**人写/材质生成**的源语言。
- **多刀竖切**可验证；每刀有演示或自动化门禁。
- 机器已具备 SDK（`glslangValidator` / `spirv-opt` / `vulkaninfo`）— 设计按 **本机 SDK** 假设，不阻塞开发。

### Success（Feature 级，非单切片）
- 启动参数可选 `OpenGL` | `Vulkan`（**默认 OpenGL**，无需重编译）。
- 主路径着色器（至少 Present → 逐步到 Forward）以 SPIR-V 加载；GL 不再依赖热路径字符串 compile（迁移完成后）。
- VK 上至少跑通 **交换链 Clear/Present + 一条与 GL 对照的简单图形路径**，再扩 Shadow/Base/IBL。

---

## 2) 地基评估（代码真值）

### 2.1 已具备（对 F05 友好）

| 能力 | 位置 / 说明 |
|------|-------------|
| 现代 RHI 虚表 | `RHI.h`：`RHICreate*` / `RHICmd*`；无 Legacy 公共面 |
| CommandList 包装 | `RHICommandList.h` |
| PSO / PipelineLayout / BindingSet | `RHIGraphicsPipelineState.h`、`RHIPipelineLayout.h`、`RHIShaderBinding.h` |
| 引擎绑定表 | `EngineShaderBindings.h`：**逻辑 (set, slot)** + **GL 扁平 `kGL_*`** 双轨 |
| Pass 无 glad | 生产 Pass 经 RHI；glad 关在 `OpenGL/` |
| Forward / RDG / Shadow / EnvMap | F06–F10 已在现代路径上 |

### 2.2 缺口与耦合（必须进设计）

| 缺口 | 现状 | F05 含义 |
|------|------|----------|
| **无 Vulkan** | CMake 无 `Vulkan`/`glslang`；无 `Vulkan/` 目录 | 新后端 + find_package / SDK |
| **Shader API 绑死字符串** | `RHI::RHICreateShader(vertexSource, fragmentSource)` | 改为 SPIR-V desc；字符串仅作编译器输入 |
| **运行时 GLSL compile** | `OpenGLRHIResources.cpp` `glCompileShader` | GL 改 specialize SPIR-V；需 **GL 4.6**（测试里仍有 **3.3** hint） |
| **GLSL 无 `set=`** | `#version 420` + `layout(binding = N)` 扁平（`Present.frag`、`Material*.glslinc`） | VK 需要 `layout(set=X, binding=Y)`；与现表双轨必须统一策略（§3.3） |
| **窗口 = GL 上下文** | `GLFWWindowSystem` + glad；`RenderSystem` 固定 `OpenGLRHI` | VK 需 `GLFW_NO_API` + surface；Present/Clear 归属 RHI/Swapchain |
| **ImGui** | `imgui_impl_opengl3` | VK 后端另接；可后置到「Editor 可切换」切片 |
| **Transition** | GL 多为 no-op | VK 切片必须认真做 image layout |
| **EngineShaderUtils** | 读文件 → `RHICreateShader(string)`；`TryCompileSourcesOnGpu` 直构 `OpenGLRHIShader` | 改为 Compiler → bytecode → RHI；测具勿再依赖 GL 字符串 compile 为唯一路径 |

### 2.3 本机环境（已探测）

- `VULKAN_SDK=C:\VulkanSDK\1.4.350.0`
- `glslangValidator.exe`、`spirv-opt.exe`、`vulkaninfo.exe` 可用  
→ **开发期**可直接调 SDK；CMake 用 `find_package(Vulkan)` / `Vulkan::glslang` 或调用 `glslangValidator` 自定义命令。

### 2.4 关键硬约束：OpenGL SPIR-V 与 Descriptor Set

Khronos **GL_ARB_gl_spirv / GL 4.6**：SPIR-V 中 **`DescriptorSet` 必须为 0**（OpenGL 无 VK 式多 set 硬件模型）。

因此：**不能**把「多 set 的 Vulkan SPIR-V」原样 `glSpecializeShader` 给 GL。

**结论（拍板推荐）：** 同一 GLSL **源**，经编译器产出 **两份** SPIR-V：

| 目标 | 装饰 | 消费方 |
|------|------|--------|
| **Vulkan** | `set=N, binding=M`（逻辑表） | `VulkanRHI` |
| **OpenGL** | **仅 set=0**，`binding = EngineShaderBindings` 的 **`kGL_*` 扁平号**（与今日 GLSL 一致） | `OpenGLRHI` |

实现上可用：（1）编译前宏/改写生成 GL 方言再 `glslang -G`；（2）或两套 layout 注入。**禁止**幻想「一份多 set SPIR-V 通吃 GL+VK」。

---

## 3) 方案

### 3.1 模块边界

```text
Runtime/Function/Render/
  RHI/                    # 契约（扩展 ShaderCreateDesc；保持后端中立）
  ShaderCompiler/         # NEW: GLSL → SPIR-V（调 glslang；缓存；目标 GL|VK）
  OpenGL/                 # 现有；改加载 SPIR-V；上下文升 4.6
  Vulkan/                 # NEW: VulkanRHI + 资源/命令/交换链
  EngineShaderUtils.*     # 读源 → Compiler → RHICreateShader(bytecode)
  EngineShaderBindings.h  # 仍为逻辑真源；文档化 set/slot ↔ GL flat 映射
  RenderSystem / Window   # 后端选择；Swapchain 抽象
```

上层 Pass / Material / RDG：**不** `#include` Vulkan/OpenGL 头；只碰 `RHI` + Compiler 产出的模块句柄。

### 3.2 着色器交付流水线

```text
  [手写 GLSL / MaterialCompiler 生成 GLSL]
              │
              ▼
     ShaderCompiler (glslang)
         │              │
         ▼              ▼
   SPIR-V (Vulkan)  SPIR-V (GL, set=0 flat)
         │              │
         ▼              ▼
    VulkanRHI       OpenGLRHI
   VkShaderModule   glSpecializeShader
```

- **开发默认：** 启动时或首次使用时编译 + 磁盘缓存（`Saved/ShaderCache/` 或等价）；可选 CMake 预编译热点 shader。
- **材质：** `MaterialCompiler` 仍输出 GLSL 文本，再进同一 Compiler（不要让材质直接 `RHICreateShader(string)` 绕过）。

### 3.3 绑定方言迁移

**目标源（Vulkan 主方言，示意）：**

```glsl
#version 450
layout(set = 0, binding = 0) uniform PerFrameData { ... };
layout(set = 1, binding = 0) uniform sampler2DArray u_DirLightShadowMap;
```

**GL 编译输入（由工具从逻辑表改写/宏展开）：**

```glsl
#version 450  // or 460 for GL SPIR-V path
layout(binding = 0) uniform PerFrameData { ... };           // kGL_PerFrameUBO
layout(binding = 8) uniform sampler2DArray u_DirLightShadowMap; // kGL_DirShadowTextureUnit
```

- `EngineShaderBindings` 继续是 **唯一** slot 真源；MaterialCompiler / includes **禁止**再写死魔法数（已有规则，迁移时强制 `set=`+逻辑名）。
- OpenGL 运行时 **继续**用 `entry.ShaderBinding` → `glBindBufferBase` / `glActiveTexture`（已有 `OpenGLRHI::ApplyShaderBindingSetResources`）——与 **GL SPIR-V 的 flat binding** 对齐。
- Vulkan 运行时用 **(setIndex, slot)** → descriptor set；`ShaderBinding` 字段对 VK **可忽略**或改为与 `Slot` 同义。

### 3.4 RHI 契约演进（着色器）

**现：**

```cpp
RHICreateShader(const std::string& vertexSource, const std::string& fragmentSource, ...);
```

**目标：**

```cpp
struct RHIShaderStageBytecode {
    RHIGraphicsShaderStage Stage;
    std::vector<uint32_t> Spirv; // or blob + size
};

struct RHIShaderCreateDesc {
    std::vector<RHIShaderStageBytecode> Stages;
    // optional debug name
};

virtual RHIShaderRef RHICreateShader(const RHIShaderCreateDesc& desc, std::string* outLog) = 0;
```

迁移期可保留 deprecated 字符串重载：**内部**走 Compiler，或 `#if` 仅测试；**有删除切片**。

`RHICommandList::CreateShader(string…)` 同步改。

### 3.5 窗口 / Present / 后端选择（含启动参数）

**后端选择（拍板）：** 程序启动参数选择 RHI，**默认 OpenGL**，可选 Vulkan——验证时**无需重编译**。

| 项 | 方向 |
|----|------|
| CLI | 全局选项，例如 `--rhi opengl` / `--rhi vulkan`（大小写不敏感；别名 `gl` / `vk` 可选）。写入 `CommandLineResult`，`RenderSystem::Initialize` 读入。 |
| 默认 | **OpenGL**（未传参 = GL）。 |
| 优先级 | CLI >（可选）`EngineConfig` 字段 > 默认 OpenGL。首版可 **仅 CLI**，Config 后补。 |
| 非法值 | 启动失败 + 用法提示（走现有 CLI UsageError 路径）。 |
| GL | GLFW 创建 **OpenGL 4.6 core** 上下文；SPIR-V specialize。 |
| VK | `GLFW_CLIENT_API = GLFW_NO_API`；`glfwCreateWindowSurface`；交换链属 `VulkanRHI`。 |
| Present | **推荐**：`RHI` 负责 acquire/submit/present；`WindowSystem` 只负责窗口/事件/resize 通知。 |
| Clear 回缓冲 | 已有 `RHIClearBackbuffer` / `RHISetBackbufferClearColor`；VK 对 swapchain image 实现。 |
| ImGui | 第一阶段 **不**要求 Vulkan Editor；GL 默认路径保持现 Editor。 |

示例：

```text
Editor.exe --project MyMEProject.meproject
Editor.exe --rhi vulkan --project MyMEProject.meproject
minEngineTests.exe --rhi opengl test smoke
```

（`test` 子命令下 `--rhi` 仍为全局 flag，实现时挂在根 `CLI::App`，而非仅 editor。）

### 3.6 Vulkan 如何接入（概念对齐）

上层只认现有现代词汇；**Vulkan 特有对象默认不进公共 `RHI` 虚表**。

```text
  ForwardRenderer / Pass / RDG
            |  RHICreate* / RHICmd* / RHICommandList
            v
         class RHI          <- 后端中立契约（现有 + 少量补全）
         /         \
  OpenGLRHI      VulkanRHI
                    |
                    +- VkInstance / Device / Queue
                    +- Swapchain + image views
                    +- Frame sync: Semaphore / Fence   <- 仅后端内部
                    +- CommandBuffer 录制（映射到 RHICmd*）
                    +- RenderPass / Framebuffer（或 dynamic rendering）
                    +- DescriptorPool / Set
                    +- Pipeline / ShaderModule (SPIR-V)
```

#### 3.6.1 公共 RHI ↔ Vulkan 概念映射

| 公共 / 引擎概念 | Vulkan 侧（后端内部） | 对齐说明 |
|-----------------|----------------------|----------|
| `RHI` 实例 | `VkInstance` + `VkDevice` + 队列族 | `Initialize`/`Shutdown` 内创建销毁 |
| `RHITexture` | `VkImage` + memory + 默认 view | 交换链 image 可特化为「外部拥有」纹理 |
| `RHIShaderResourceView` | `VkImageView`（+ sampler 策略） | 与现 SRV desc 对齐；sampler 可先写死或挂 layout |
| `RHIBuffer` | `VkBuffer` + memory | UBO/VB/IB 按 usage 分 |
| `RHIShader` | `VkShaderModule`（每 stage） | 只吃 SPIR-V |
| `RHIShaderBindingSetLayout` | `VkDescriptorSetLayout` | `Slot` ↔ binding；`setIndex` ↔ set 号 |
| `RHIPipelineLayout` | `VkPipelineLayout` | 多 set layout 组合 |
| `RHIShaderBindingSet` | `VkDescriptorSet`（+ 更新） | 池化在后端；防每帧泄漏 |
| `RHIGraphicsPSO` | `VkPipeline` | 与 render pass / 动态渲染兼容 |
| `RHIVertexInputLayout` | pipeline vertex input state | 可打进 PSO 创建 |
| `RHICmdBegin/EndRenderPass` | `vkCmdBegin/EndRendering` 或 legacy RenderPass | **优先** dynamic rendering（若设备允许），减少 VkRenderPass 对象爆炸 |
| `RHICmdSet*` / `Draw*` | `vkCmd*` 写入当前 frame 的 command buffer | 一帧 1+ primary CB |
| `RHICmdTransition` | `vkCmdPipelineBarrier` / image layout | **GL no-op；VK 必须实现**——语义在公共 API、实现后端化 |
| `RHIClearBackbuffer` / Present | acquire → 录制 → submit → present | 见下节帧循环 |
| `EngineShaderBindings` (set, slot) | descriptor set = set，binding = slot | GL 仍用 `ShaderBinding`→`kGL_*` |

#### 3.6.2 Vulkan 特有概念：公共 API 还是后端内部？

| Vulkan 概念 | 放哪 | 理由 |
|-------------|------|------|
| **Semaphore**（图像可用 / 渲染完成） | **仅 `VulkanRHI` 内部** | 上层无「跨队列显式同步」需求；暴露会污染 GL 与 Pass 代码 |
| **Fence**（CPU 等 GPU） | **仅后端内部**（按 frame-in-flight） | 对应帧结束 / 资源回收；不必新虚函数暴露 VkFence |
| **Queue** / 队列族 | **仅后端内部** | 首阶段单 Graphics(+Present) 队列 |
| **Swapchain** / `vkAcquireNextImage` | **仅后端内部**；对外仍是 Clear/Present/回缓冲尺寸 | 避免 Pass 写 acquire |
| **VkRenderPass / Framebuffer** | **仅后端内部**（或 dynamic rendering 替代） | 公共面保持现有 `RHIRenderPassInfo` |
| **DescriptorPool** | **仅后端内部** | 分配策略随切片进化 |
| **Pipeline cache** | **仅后端内部**（可选） | 加速 PSO；不进公共 API |
| **Validation layers** | **仅后端 + 可选 CLI**（如 `--vk-validate`） | 开发用 |
| **Image layout / barrier** | 通过已有 **`RHICmdTransition`** 表达意图；VK 落地 barrier | **不**新增 `RHICmdWaitSemaphore` |
| **多队列 / 异步计算 / 时间线信号量** | **Out（本 Feature 近端）** | 需要时另开演进；再评估是否升高公共同步 API |

**原则（拍板）：**  
公共 `RHI` 保持 **「资源 + 录制意图 + 提交帧」** 模型。  
Vulkan 的 **semaphore / fence / acquire / queue submit 细节全部藏在 `VulkanRHI` 的帧循环**里。  
只有当 **两个后端都必须表达同一意图**（如纹理从 RT → Sampled）时，才用公共命令（已有 Transition）；**不为 VK 独有机制加虚函数**。

#### 3.6.3 建议的 Vulkan 帧循环（内部，示意）

```text
BeginFrame (内部):
  wait fence[frame]
  acquire next image -> signal imageAvailableSemaphore

上层 / Pass:
  RHICmdBeginRenderPass(swapchain或离屏) ...
  RHICmd* draws ...
  RHICmdEndRenderPass

EndFrame / Present (内部或 RHIPresent):
  submit command buffer
    wait: imageAvailableSemaphore
    signal: renderFinishedSemaphore
    fence[frame]
  vkQueuePresentKHR(wait renderFinishedSemaphore)
  frame = (frame+1) % kFramesInFlight   // 建议起步 = 2
```

`RenderSystem` / `WindowSystem` **不**直接碰 semaphore。若现有 `SwapBuffers` 难抽象，可增加后端中立：

- `RHI->BeginFrame()` / `RHI->EndFrame()` / `RHI->Present()`  
  （GL：Present/EndFrame ≈ `glfwSwapBuffers`；VK：上述 submit+present）

这比把 `VkSemaphore` 抬进公共头更干净。**S03/S05 实现时写入切片 DoD**。

### 3.7 Vulkan 竖切策略（与上层对接）

1. **设备 + 交换链 + Clear/Present**（可不经 ForwardRenderer）；CLI `--rhi vulkan`。
2. **全屏三角/Present 等价**：SPIR-V、最小 PSO、1 个 descriptor set。
3. **接入现有 `PresentPass` / CommandList**（同一 Pass 代码路径）。
4. 再按风险扩：Post → Base（无阴影）→ Shadow → IBL → 全 Forward。

每步保持 **GL 回归**（默认 `--rhi opengl` / 不传参）不被破坏。

### 3.8 现代 RHI 语义在 VK 上补全（随场景 Pass 切片）

- `RHICmdTransition` 真 image layout（场景 RT 接入时列为 DoD）。
- Descriptor pool / set 生命周期；避免每帧 leak。
- PSO 与 rendering info 兼容（dynamic rendering vs 离屏一致）。
- Frame-in-flight = 2 起步；与 semaphore/fence 内部方案锁定。

进入 BasePass 级切片前，若动态渲染 vs legacy render pass 有争议，再补短 ADR。

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| A. GLSL 源 → 双目标 SPIR-V（VK 多 set / GL flat） | 双端真 SPIR-V；符合要求 | 要编译器/改写层 | **选用** |
| B. 仅 VK 用 SPIR-V；GL 永远运行时 GLSL | 实现快 | 双路径永久分裂 | 拒绝作终态；最多极短迁移桥 |
| C. 一份多 set SPIR-V 通吃 GL+VK | 理想 | **违反 GL SPIR-V 规范** | **不可行** |
| D. SPIRV-Cross：VK SPIR-V → GLSL 再给 GL | 少维护 GL 方言 | GL 仍非 SPIR-V 消费 | 备选急救，不作主路径 |
| E. glslang 源码进 Third-Party | 可离线复现 | 体积/构建重 | 延期；先用 SDK |
| F. 公共 RHI 暴露 Semaphore/Fence | 与 VK 教程同构 | GL 无对应；Pass 被污染 | **拒绝**；同步留后端 |
| G. 仅宏/重编译切后端 | 实现少 | 验证要重编译 | **拒绝**；用 CLI `--rhi` |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| GL 上下文仍 3.3 | 无法 specialize SPIR-V | 升 4.6；驱动检查；失败时明确日志 |
| 绑定改写 bug | GL/VK 黑屏/采样错 | 黄金 Present + 对照；绑定表单测 |
| 材质仍输出旧 binding= | 半迁移 | MaterialCompiler / includes 随后切片改方言 |
| VK 与瞬态 FBO 模型摩擦 | Pass 难迁 | 先 Present/全屏；场景 Pass 前定 dynamic rendering |
| ImGui/Editor 绑 GL | 拖慢 VK | VK 竖切不堵在 ImGui；默认仍 GL Editor |
| SDK 路径差异 | 他人编不过 | CMake find_package(Vulkan) + 文档 |
| CLI 与 test 子命令 | `--rhi` 吃不到 | 挂在根 `CLI::App`，两模式共用 |

---

## 6) 验收标准（Feature 级）

- [x] Design → **Planned**（§7 已确认）+ Implementation 切片可执行。
- [ ] `--rhi opengl|vulkan`（默认 opengl）可切换，无需重编译。
- [ ] ShaderCompiler 可对引擎 shader 产出 VK/GL 两份 SPIR-V；缓存策略写明。
- [ ] OpenGL 热路径至少 **Present** 以 SPIR-V 加载；上下文 ≥ 4.6。
- [ ] Vulkan：交换链 Clear/Present 可运行；随后至少一条 SPIR-V 图形路径与 GL 对照。
- [ ] 公共 `RHI` **无** Semaphore/Fence/Queue 类型；VK 同步仅后端内部。
- [ ] `RHICreateShader` 以 bytecode 为正式契约；字符串路径有删除计划或已删。
- [ ] 上层仍无 `vulkan.h` / `glad.h`（除后端目录与窗口后端）。
- [ ] Registry / ACTIVE_WORK / PROGRESS 随切片更新。

---

## 7) 拍板项（已确认 2026-08-04）

| # | 问题 | 结论 |
|---|------|------|
| 1 | GL+VK 都上 SPIR-V，且 **两份** SPIR-V（VK 多 set / GL flat）？ | **是** |
| 2 | glslang 来源？ | **本机 Vulkan SDK**；不 vendor 源码 |
| 3 | 第一刀实现顺序？ | **S01 Compiler → S02 GL Present SPIR-V → S03 VK 设备/交换链 → S04 VK 绘制** |
| 4 | GL 上下文升到 **4.6**？ | **是** |
| 5 | 材质/set= 与 Present 同批？ | **分批**（先 Present + 工具链） |
| 6 | VK 第一阶段 Editor+ImGui？ | **不要** |
| 7 | 启动参数选后端？ | **是**：`--rhi`，**默认 OpenGL**，可选 Vulkan（不重编译） |
| 8 | Semaphore/Fence/Queue 进公共 RHI？ | **否**；仅 `VulkanRHI` 内部；公共最多 `BeginFrame`/`Present` 级中立 API |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-06-01 | 初稿占位（F04 号） |
| 2026-06-11 | 顺延 F05 |
| 2026-08-04 | 地基评估 + SPIR-V 双端策略；Status Draft |
| 2026-08-04 | 拍板确认 → Planned；补 CLI `--rhi`；补 §3.6 VK 接入与特有概念内聚 |
