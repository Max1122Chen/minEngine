# MAT-F08 — Sprite/UI 材质编译风暴（共享 program + instance 缓存）

## Meta
- **ID:** MAT-F08
- **Type:** Fix（P0 性能缺陷）
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-10-19
- **Related:**
  - [MAT-F01 运行时产品化路线](./MAT-F01_MATERIAL_RUNTIME_PRODUCTIZATION_ROADMAP.md)
  - [MAT-F04 MaterialInstance](./MAT-F04_MATERIAL_INSTANCE_DESIGN.md) · [MAT-F05 MID](./MAT-F05_MATERIAL_INSTANCE_DYNAMIC_DESIGN.md) · [MAT-F02 Proxy](./MAT-F02_MATERIAL_RENDER_PROXY_DESIGN.md)
  - [RND-F16 2D 渲染基础](../RND-F16_2D_RENDERING_FOUNDATION_DESIGN.md)
  - 消费方反馈（只读）：`minCraft-ui/docs/ai/INFRA/ENGINE_FEEDBACK_PROMPT_SPRITE_MATERIAL_COMPILE_CACHE.md`
- **UE 锚点：** `UMaterial::GetMaterialResource` 共享 shader map + `UMaterialInstance` 只覆盖参数；`FMaterialRenderProxy` 按 `MaterialRenderProxy` 缓存 program。本 Feature 把「一个 Material 对象 = 一次完整编译」收敛为「一个 variant = 一次编译，实例只是参数视图」。

## TL;DR

ScreenUI 每个 Image widget 各自 `CreateInstance` 一份 `SpriteUnlit` Material，而该工厂**每次调用都重建图 + 走完整 MaterialCompiler 链路 + 两次 glslang 子进程 + 一次 GL program link**；`ShaderCompiler` 的磁盘缓存因 `SetCacheDirectory` **从无生产调用者**而永久失活，且没有内存级 bytecode 缓存。结果：N 个 widget = 2N 次 glslang 调用（每条约 100 ms）≈ 消费方看到的 200 条日志 / 80 秒。

本 Feature 落地三层缓存：**SPIR-V bytecode（进程级）→ GPU program（每 RHI 实例）→ 编译好的 Material 模板（每 variant）**，widget 只「派生」参数独立的实例。实测 32 widget：**64 次编译 / 32 次 link / 6695 ms → 2 / 1 / 260 ms**；512 widget 首次加载 **2 / 1 / 232 ms**，第二次加载 **0 / 0 / 0.4 ms**。

## Problem（file:line 证据）

### 1. 每个 widget 一份 Material
| 事实 | 位置 |
|------|------|
| 每个 `WidgetComponent` 独占一个 `std::shared_ptr<Material> m_RuntimeMaterial` | `Runtime/Function/Framework/Components/WidgetComponent.h:120` |
| `FillSceneProxy` 对可绘制 Image **每 widget** 调 `EnsureRuntimeMaterial()` + `SyncMaterialParameters()` | `WidgetComponent.cpp:356-366`（调用点 `363-364`） |
| `EnsureRuntimeMaterial` 只在自身为空/未编译时创建 —— 即「每 widget 一次」 | `WidgetComponent.cpp:291-305` |
| ScreenUI 材质工厂直接转发到 Sprite 工厂（translucent） | `Runtime/Function/Render/ScreenUI/ScreenUIMaterialFactory.cpp:18-21` |
| **`CreateInstance` 每次：NewObject → `PopulateSpriteMaterialGraph` → `MaterialCompiler::Compile`**（无任何缓存） | `Runtime/Function/Render/Sprite/SpriteMaterialFactory.cpp:129-147`（修复前） |
| `TextComponent` 同一路径（每 Text widget 一份） | `Framework/Components/TextComponent.cpp:118-140` |

### 2. 编译链路：一次 Material = 2 次 glslang + 1 次 GL link
| 事实 | 位置 |
|------|------|
| `MaterialCompiler::Compile` → `CommitCompileResult` → `EngineShaderUtils::CreateShaderFromSpirvSources` | `Runtime/Function/Render/Material.cpp:317-334` |
| 该函数**先编译 vertex、再编译 fragment**，各发一次 `ShaderCompiler::Compile` | `Runtime/Function/Render/EngineShaderUtils.cpp:143-173`（修复前） |
| `Compile` 每次实际拉起 glslangValidator 子进程 | `ShaderCompiler.cpp:682-688` → `InvokeGlslang` `ShaderCompiler.cpp:516-616` |
| 成功即打日志 `ShaderCompiler: compiled <name>` —— **每条日志 = 一次真实 glslang 调用** | `ShaderCompiler.cpp:730-734` |
| 最后 `RHICreateShader` 为每个 Material 建一个新 GL program，并打 `OpenGLRHIShader: loaded SPIR-V program` | `OpenGL/OpenGLRHIResources.cpp:697-702` |

### 3. 缓存为何 miss
| 事实 | 位置 |
|------|------|
| key 本身没问题：FNV-1a over (source, stage, target, entryPoint) | `ShaderCompiler.cpp:425-448` |
| 但磁盘缓存整段被 `if (!m_CacheDirectory.empty())` 守卫 | `ShaderCompiler.cpp:648-661`、`710-722` |
| `SetCacheDirectory` **全仓只有测试一处调用**，生产路径（Engine/Editor/Game 启动）从不设置 → `m_CacheDirectory` 恒为空 → 磁盘缓存永不生效 | `Runtime/Function/Render/ShaderCompiler/ShaderCompiler.h:53`；grep 结果仅 `Tests/Suites/ShaderCompilerTest.cpp:81` |
| 进程内无 bytecode 缓存（`m_MemoryCache` 修复前不存在）→ 同一份 GLSL 反复编译 | 修复前 `ShaderCompiler.h:104-105` 只有 glslang 路径与 cache 目录 |
| 生成 GLSL 对同一 variant 是**确定性**的（参数是 uniform，不是字面量），所以 miss 不是「源码不同」而是「缓存没开」 | `SpriteMaterialFactory.cpp:16-126`（参数全部走 Scalar/Texture parameter）+ `MIRBuilder`/`MaterialTranslator` |
| 每个 Material 各自持 program，且无 program 级复用 | `Material.h:115-117`（`m_GPUShader` 每对象一份） |

**结论**：根因确实在材质工厂 + shader/program 缓存层，不在 PSO / descriptor / 反射 codegen。数字自洽：消费方 200 条 `compiled` ≈ 100 个可绘制 widget × 2 stage，每条约 100 ms。

## Scope

**In**
- `ShaderCompiler`：进程内 SPIR-V bytecode 缓存 + 可观测计数器（`ShaderCompileStats`）。
- `RenderSystem::Shutdown()`：**进程级 GPU 持有缓存的唯一释放点**（见 §GPU 资源生命周期与释放点）。
- `ScreenUIMaterialFactory`：既有静态白纹理改为可释放（`ClearCachedResources()`，顺带修既有缺陷）。
- `EngineShaderUtils`：GPU program 缓存（同源码 → 同 `RHIShader`），按 RHI 实例隔离。
- `RHI`：`GetInstanceSerial()`（GPU 资源归属标识，供上述缓存分段）。
- `Material::CreateRuntimeInstance()`：复用已编译 program/layout 的运行时兄弟实例（不编译）。
- `SpriteMaterialFactory`：每 blend mode 一份编译好的模板 + 实例派生；缓存统计 + 显式清理。
- 测试套件 `material-compile-cache`（oracle + 反向验证）。

**Non-goals**
- 不改 `WidgetComponent`/`SpriteComponent`/`TextComponent` 的持有点（仍各持一份 Material 实例），见 Deferred。
- 不引入异步/多线程编译调度（当前编译仍在首帧同步路径，只是次数降为常数）。
- 不改 `PopulateSpriteMaterialGraph` 的图语义、uniform slot 布局、SpriteUnlit 输出（视觉零变化）。
- 不做 MIC/MID 的 per-widget 化重构（MAT-F04/F05 面保持现状）。

## How

### 三层缓存与所有权

```
CreateInstance(rhi, translucent)          [SpriteMaterialFactory]
  └─ GetOrCreateTemplate(rhi, blend)      ── 每 (RHI serial, blend) 一份 compiled Material
       └─ CompileTemplate()               ── 冷启动执行一次：建图 + MaterialCompiler::Compile
            └─ CreateShaderFromSpirvSources()   [EngineShaderUtils]
                 ├─ program cache hit?      ── 同源码 → 直接复用 RHIShader*
                 └─ ShaderCompiler::Compile(vert/frag)  ── bytecode cache hit? 否则 glslang
  └─ template->CreateRuntimeInstance()    ── NewObject + 拷贝 program/layout/参数表（不编译）
  └─ ApplyColorTextureAndUVRect(instance) ── 只写本实例参数表
```

| 层 | 持有者 | 生命周期 | key |
|----|--------|----------|-----|
| SPIR-V bytecode | `ShaderCompiler` 单例 | 进程；`ClearMemoryCache()` 显式清 | `HashSourceKey`（source+stage+target+entry） |
| GPU program | `EngineShaderUtils` 静态表 | 进程；换 RHI serial 自动整表清除 | FNV-1a(vertSrc, fragSrc, spirvTarget) |
| Material 模板 | `SpriteMaterialFactory` 静态表 | 进程；`ClearTemplateCache()` 显式清 | (RHI serial, translucent) |
| Material 实例 | 组件（widget/sprite/text） | 组件生命周期 | 不共享（参数独立，永不串色） |

### key 设计
- **bytecode key**：沿用既有 `HashSourceKey` = FNV-1a 64 over（已 flatten/inject 的最终源码, stage, target, entryPoint）。**stage 必须参与**，否则 vert/frag 会撞车（见反向验证 RV2/RV3）。
- **program key**：FNV-1a 64 over（vertex 源码, `'\0'` 分隔符, fragment 源码, spirvTarget）。不掺 DebugName —— 同代码不同标签应共享 program。
- **模板 key**：`(RHI::GetInstanceSerial(), bool translucent)`。GPU 对象 binding 到具体 RHI 实例，跨实例共享是未定义行为，因此**串行号变化即整表失效**。
- **浮点/纹理**：本 Feature 不把 tint/texture/uvRect 纳入 key —— 它们全部是 uniform 参数而非编译期常量（`SpriteMaterialFactory.cpp:16-126` 的图里没有任何参数被烘焙成字面量）。因此**没有浮点比较问题**：同 variant 的任意参数组合都命中同一 program，差异只落在实例的参数表里。这也消除了「tint 变化 → 新 key → 重编译」的风险。
- **纹理/GUID**：纹理只经 `SetTextureParameter("BaseColor", …)` 进入实例参数（`SpriteMaterialFactory.cpp:163`），不参与编译，故无需 GUID 参与 key。

### 失效策略
| 触发 | 处理 |
|------|------|
| shader 源码改动（热重编） | 源码进 key → 自然 miss；显式路径：`ShaderCompiler::Get().ClearMemoryCache()` + `EngineShaderUtils::ClearProgramCache()` + `SpriteMaterialFactory::ClearTemplateCache()` |
| 材质资产重载 / 图变更 | 同上三连清（模板缓存必须清，否则仍用旧 program） |
| 参数回退（tint 回退） | 无需失效：参数不是 key 的一部分，直接 `SetScalarParameter` |
| RHI 实例重建（PIE 换 device、测试换 context） | 自动：serial 不匹配时 program 表与模板表整体丢弃 |
| 泄漏回收 | 模板与 program 由 `shared_ptr` 持有，进程级上限 = variant 数（本仓 2）× RHI 实例数；实例 Material 仍随组件销毁 |

## GPU 资源生命周期与释放点

### 缺陷形状（评审指出的既有/新增风险）
静态缓存持有 GPU 对象，却在 context 销毁**之后**才析构，析构函数会调用 GL 删除接口：

| 事实 | 位置 |
|------|------|
| program 表是名字空间作用域静态，持 `RHIShaderRef` | `EngineShaderUtils.cpp:21-22` |
| 模板表是函数内静态，持 `shared_ptr<Material>` → `RHIShaderRef` + `RHIShaderBindingSetLayoutRef` | `SpriteMaterialFactory.cpp:148`、`Material.h:115-117` |
| `OpenGLRHIShader` 析构**无条件** `glDeleteProgram` | `OpenGL/OpenGLRHIResources.cpp:600-604` |
| `OpenGLRHITexture` 析构 `glDeleteTextures` | `OpenGL/OpenGLRHIResources.cpp:528-534` |
| 修复前 `RenderSystem::Shutdown()` 只做 renderer/RHI 清理，**不清任何静态缓存** | `RenderSystem.cpp:116-135`（修复前） |

### 释放点与顺序（确定性论证）
`RenderSystem::Shutdown()` **首行**释放：`ClearProgramCache` / `ClearTemplateCache` / `ClearCachedResources` / `ClearMemoryCache`；在 `m_SceneRenderer->Shutdown()` 之后、`m_RHI->Shutdown()` 之前调 `SpriteQuadMesh::Get().Shutdown()`。

- context 的唯一销毁点是 `GLFWWindowSystem::Shutdown()`（`GLFWWindowSystem.cpp:170-196`：`glfwDestroyWindow` + `glfwTerminate`）。
- 进程关闭顺序（`Runtime/Engine.cpp`）：`m_SceneManager->Shutdown()` **:500** → `m_RenderSystem->Shutdown()` **:542** → `m_WindowSystem->Shutdown()` **:549**。释放点位于 542 内部，早于 549。
- `OpenGLRHI::Shutdown()`（`OpenGLRHI.cpp:73-79`）只清 sampler / 解绑 window 指针，**不销毁 context**。
- 场景先于 RenderSystem 销毁（500 < 542），组件持有的 Material 此时已释放 → 静态缓存是最后的 GPU 持有者。
- `RenderSystem::Shutdown()` 的唯一调用者是 `Engine.cpp:542`。

### 清空时的 refcount 语义（为什么不会悬空）
- 派生实例在 `Material::CreateRuntimeInstance` 里**拷贝** `RHIShaderRef`（`Material.cpp:63-66`），模板被清后 program 仍由活 Material 持有；最后一个持有者释放时才 `glDeleteProgram`。
- 白纹理同理：widget Material 绑定自己的 `shared_ptr<Texture2D>`。
- **残留风险（既有、非本 Feature 引入）**：若外部在 `RenderSystem::Shutdown()` 之后仍持有 Material，其 program 析构会落在死 context 上。修复前**每个 widget 的 Material 都是这样的持有者**（`WidgetComponent.h:120`），风险面大于本 Feature 新增的两层缓存；现由「场景先销毁」的顺序兜住。本 Feature 不扩大改动面去改组件持有点（见 Deferred）。

### 幂等与重建
- `RenderSystem::Shutdown()` 重复调用安全：清空是对空表/空 shared_ptr 的幂等操作，RHI/renderer 分支各有 null 守卫。
- 释放后可惰性重建：`SpriteQuadMesh::EnsureInitialized` 有 `if (IsReady()) return;`（`SpriteQuadMesh.cpp:35-38`）；`GetOrCreateWhiteTexture` 用 `!s_WhiteTexture || GetRHITexture()==nullptr` 重建（`ScreenUIMaterialFactory.cpp:18-26`）。
- **本仓不存在 Shutdown→Initialize 循环**（证据：`RenderSystem::Initialize` 全仓唯一调用者 `Engine.cpp:455`，由 `Engine::Initialize` 调用一次；Editor 只 `Get().GetRHI()/PresentFrame()/SetPresentPassEnabled()`，无重新 Initialize）。即便如此仍加了一条用例覆盖「释放后 quad/白纹理/材质可重建且仍可编译」，避免将来引入 device rebuild 时 UI 全消失。

## 静态 GPU 持有者审计（全仓 src + Editor）
| 位置 | 持有 | 处置 |
|------|------|------|
| `EngineShaderUtils.cpp:21-22` `g_ProgramCache` | `RHIShaderRef` | 本 Feature 新增 → 纳入释放点 |
| `SpriteMaterialFactory.cpp:137-149` `s_Cache` | `shared_ptr<Material>` | 本 Feature 新增 → 纳入释放点 |
| `ScreenUIMaterialFactory.cpp:11-15` `s_WhiteTexture` | `shared_ptr<Texture2D>` → GPU 纹理 | **既有缺陷** → 新增 `ClearCachedResources()` 纳入释放点 |
| `SpriteQuadMesh.cpp:25` `static SpriteQuadMesh instance` | `RHIBufferRef`×2 + `RHIVertexInputLayoutRef`（`SpriteQuadMesh.h:47-49`） | **既有缺陷**（它自带 `Shutdown()` 但全仓无调用者）→ 从 `RenderSystem.cpp` 调用，零改动该文件 |
| `SceneViewport.cpp:50` `static const RHITextureRef s_Empty` | 恒为 null 哨兵 | 安全，不改 |
| `Material.cpp:85` `static MaterialEdGraph emptyGraph` | 纯 CPU 图对象 | 安全，不改 |
| `ShaderCompiler.cpp:253` 单例 | 路径字符串 + SPIR-V 字（CPU） | 安全（释放点仍一并清） |
| `OpenGLRHI.h:97` `m_SamplerCache` | RHI 成员 | 随 RHI 在 `OpenGLRHI::Shutdown()` 释放，context 仍活 |
| Editor/src 全量 grep | 无静态 GPU 持有者 | — |

## 预算（可测数字，非估计）

| 场景 | 编译次数（glslang 调用） | GPU program link | 说明 |
|------|--------------------------|------------------|------|
| 单界面首次加载（纯 UI，任意 widget 数，任意 tint/纹理/UV 组合） | **2**（SpriteUnlit.vert + .frag） | **1** | 实测 512 widget：2 / 1 |
| 单界面首次加载（同时用到 opaque + translucent sprite） | **3**（translucent vert+frag + opaque frag；顶点源码字节相同 → 命中 bytecode） | **2** | 实测 mixedVariants |
| 同一界面第二次加载 | **0** | **0** | 实测 32 widget reload = 0 / 0，0.4 ms |
| 运行期改 tint（单个） | **0** | **0** | 只写实例参数表 |
| 运行期批量改 tint（512 widget × 4 参数） | **0** | **0** | 实测 1.8 ms → 3.5 µs/widget |
| 首次加载耗时 | 232 ms（512 widget，其中约 224 ms 为 2 次 glslang 子进程） | — | 修复前同规模外推约 105 s |
| 派生一个 widget 实例 | 0 | 0 | 实测 13 µs/widget（512 个 6.7 ms） |

**上限论证**：编译次数上界 = (blend 变体数 × stage 数) 与「源码种类数」的较小者。SpriteUnlit 只有 2 个变体、片段源码的差异仅由 `translucent` 决定（`SpriteMaterialFactory.cpp:29` 的 `uvSlotBase` 与 106-121 的透明分支），顶点源码两变体完全一致，因此**上界恒为 3 次 glslang、2 个 program，与 widget 数量无关**。

## Failure Model
| 情况 | 行为 |
|------|------|
| glslang 缺失/失败 | `CompileTemplate` 返回 nullptr → `CreateInstance` 返回 nullptr（与修复前一致），诊断日志保留 |
| 模板编译成功后 RHI 被销毁 | serial 变化 → 下一次调用整表清空重建；不会复用已死 GPU 句柄 |
| 调用方误写模板参数 | 文档禁止（`GetOrCreateTemplate` 注释）；`CreateInstance` 返回的实例才是可写对象 |
| program key 碰撞（FNV-1a 64） | 概率 2^-64 量级；与既有 `HashSourceKey` 同口径，未额外加宽 |
| 多线程同时首次创建 | 模板/program/bytecode 三处均有 mutex；首次编译在锁内串行，只发生一次 |

## Verification（可观察 oracle）
- 套件 `material-compile-cache`，8 个用例，全部 `[full]`：
  1. 32 widget 一批 → `spirvArtifacts == 2`、`programLinks == 1`、所有实例 `GetGPUShader()` 指针相同。
  2. 第二次加载 → 增量 `0 / 0`（**oracle：同界面第二次加载编译次数 ≈ 0**）。
  3. 运行期 tint → 增量 0，且红/绿两实例 `TintR` 互不污染（**共享 instance 串色风险的正向反证**）。
  4. opaque+translucent 冷启动 → 3 次 artifact / 2 个 program。
  5. 512 widget 首次加载 → 2 / 1，`TemplateCompiles == 1`、`InstanceDerivations == 512`。
  6. 热缓存派生 + 批量 tint → 0 compile，并输出 µs/widget 供预算复核。
  7. **真实 shutdown 路径**：`RenderSystem renderSystem; renderSystem.Shutdown();` 后 program/template/白纹理/quad mesh 全归零，context 仍活且 `glGetError()==GL_NO_ERROR`，活 Material 仍持同一 `RHIShader*`。
  8. **幂等 + 惰性重建**：重复 `Shutdown()` 安全；释放后 quad/白纹理/材质可重建且仍可编译（防"设备重建后 UI 全消失"）。
- 计数来源（双轨，任一可独立复核）：
  - 日志行：`ShaderCompiler: compiled ` (`ShaderCompiler.cpp:730-734`) 与 `OpenGLRHIShader: loaded SPIR-V program` (`OpenGLRHIResources.cpp:701`)，测试用 `LogSystem::AddSink` 计数。
  - 计数器：`ShaderCompiler::GetCompileStats()`（`CompileRequests / MemoryCacheHits / DiskCacheHits / GlslangInvocations`）与 `SpriteMaterialFactory::GetCacheStats()`（`TemplateCompiles / InstanceDerivations`）。
- GPU 资源释放点 oracle（`RenderSystem` shutdown 用例）：`RenderSystem::Shutdown()` 之后 `GetProgramCacheEntryCount()==0`、`GetTemplateCacheEntryCount()==0`、`HasCachedWhiteTexture()==false`、`SpriteQuadMesh::IsReady()==false`，且 context 仍当前、`glGetError()==GL_NO_ERROR`、活 Material 仍共享同一 `RHIShader*`。
- 运行命令：
  ```
  cd D:\Dev\GitRepo\minEngine\minEngine\bin
  .\minEngineTests.exe test material-compile-cache
  ```
- 无窗口/无 GPU 环境的退化：本套件用 1×1 隐藏 GL 4.6 context + `OpenGLRHI`，断言落在 **句柄同一性 + 计数器** 层（不依赖像素）；若环境连 GL context 都拿不到，`REQUIRE(glContext.IsReady())` 会显式失败而不是静默跳过。

## 兼容影响
| 面 | 影响 |
|----|------|
| MAT-F01..F07（Material/Instance/MID/Proxy/Interface） | 无行为变化。新增 `Material::CreateRuntimeInstance()` 是加法 API，不改任何既有签名；`MaterialInstance`/`MID` 仍是参数覆盖的正规路径（本 Feature 不走它们，因为组件持有的是 `shared_ptr<Material>`） |
| SpriteUnlit 图 | 图内容、uniform slot、opaque/translucent 分支、输出连线完全未改；生成 GLSL 与修复前**逐字节一致**，因此 program 缓存可命中 |
| SpriteComponent / TextComponent | 语义不变（仍每组件一份 Material），但不再触发编译；opaque/translucent 切换仍走 `EnsureRuntimeMaterial` 重建 |
| 消费方 minCraft | Play 首次加载从「百次编译 / 数十秒」降为「2 次编译 / 一次 ~230 ms 抖动」；「懒建面板」的规避可以撤掉（仍可保留作加载策略）。纹理换图只需 `SetTextureParameter`，零编译 |
| RHI | 新增 `GetInstanceSerial()`（非虚、无 ABI 变更语义）；`RHICreateShader` 调用次数大幅下降 |
| 内存 | 新增：每 variant 1 个模板 + 每 source 1 份 bytecode（约 5 KB）+ 每 program 1 个 `RHIShader`；每 widget 仍 1 个 Material 对象（未增加） |
| RenderSystem 关停 | `Shutdown()` 现在显式释放静态 GPU 缓存；之后再 `CreateInstance` 会重新编译（2 次 glslang），语义仍是「可按需重建」 |
| 静态 GPU 持有者 | 本 Feature 之前有 2 处既有静态持有者（白纹理、`SpriteQuadMesh`）在 context 死后续存；现已一并纳入释放点（见上审计表） |

## Deferred
1. **组件级实例去重**：`WidgetComponent`/`SpriteComponent` 仍各持一份 Material。若改为「参数键 → 共享实例」可进一步减少 Material 对象与 UBO 数量（500 widget 同色可降到 1 个 UBO），但需改 `Framework/Components/*` 与 `RenderScene` 队列，超出本工作项边界。实测收益：派生本身已只 13 µs/widget。
2. **异步/预热编译**：首次 `CreateInstance` 仍同步吃掉约 224 ms（2 次 glslang）。可在 PIE/引擎启动时预热模板（`GetOrCreateTemplate` 已是公开入口，预热只需一行调用）。
3. **bytecode 磁盘缓存默认开启**：`SetCacheDirectory` 仍无生产调用者。开启需决定缓存目录策略（`Saved/ShaderCache`）与版本失效策略，留作后续；本 Feature 的进程内缓存已消除重复编译。
4. **program key 加宽**：如需抵御对抗性碰撞，可改为 SHA-256 前缀。
5. **device rebuild / Shutdown→Initialize 循环**：本仓当前不存在该路径（证据见上）。若将来引入，建议在 `RenderSystem::Initialize` 里预热模板，避免首个 widget 承担 ~230 ms 编译。
6. **TextComponent atlas 换图**：当前每 Text widget 一份 Material 已零编译，但 atlas rebind 仍走 `SetTextureParameter`（零编译），无需改动。