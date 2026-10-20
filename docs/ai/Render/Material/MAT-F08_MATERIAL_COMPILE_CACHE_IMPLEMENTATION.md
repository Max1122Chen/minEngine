# MAT-F08 — Sprite/UI 材质编译缓存 实现记录

对应设计：[MAT-F08_MATERIAL_COMPILE_CACHE_DESIGN.md](./MAT-F08_MATERIAL_COMPILE_CACHE_DESIGN.md)
状态：**Done**（代码 + 8 用例 + 两级反向验证；未 commit）

## 1. 改动清单

| 文件 | 改动 |
|------|------|
| `ShaderCompiler.h/.cpp` | 进程内 SPIR-V bytecode 缓存（`m_MemoryCache` + mutex）+ `ShaderCompileStats` 计数器；`MakeCacheFilePath` 接收已算好的 sourceKey |
| `EngineShaderUtils.h/.cpp` | GPU program 缓存（同 vert/frag 源码 → 同 `RHIShaderRef`）、`ClearProgramCache()`、`GetProgramCacheEntryCount()`、`GetProgramCacheHits()/ResetProgramCacheStats()` |
| `RHI/RHI.h` | `RHI::GetInstanceSerial()`（单调串行号），供 GPU 资源缓存分段 |
| `Material.h/.cpp` | `Material::CreateRuntimeInstance()`：复用已编译 program/layout/graph/参数表的运行时兄弟实例，不编译 |
| `SpriteMaterialFactory.h/.cpp` | `GetOrCreateTemplate()/ClearTemplateCache()/GetTemplateCacheEntryCount()/SpriteMaterialCacheStats`；`CreateInstance` = 取模板 → 派生实例 |
| `ScreenUI/ScreenUIMaterialFactory.h/.cpp` | 静态白纹理提升为可释放缓存：`ClearCachedResources()`、`HasCachedWhiteTexture()`（**顺带修既有缺陷**，见 §10） |
| `RenderSystem.cpp` | `Shutdown()` 首行释放进程级 GPU 持有缓存；renderer 关停后调 `SpriteQuadMesh::Get().Shutdown()`（**关停所有权修正**，见 §9/§10） |
| `Tests/Suites/MaterialCompileCacheTest.h/.cpp` | 新套件 `material-compile-cache`（8 用例） |
| `Tests/TestSuiteRegistration.cpp` | 注册 `material-compile-cache`（该文件另有他人 WIP，仅追加） |

未改（硬边界）：`ScreenUICoords.{h,cpp}`、`SpriteQuadMesh.{h,cpp}`（只调其公开 `Shutdown()`）、`WindowSystem.h`、`GLFWWindowSystem.*`、`Editor/**`、`FEATURE_REGISTRY.md`、`docs/ai/bugs/*`、`CORE-F38_*`、`CORE-F42_*`、`Framework/Components/*`、`RenderScene`、`minCraft*`。

## 2. 关键实现点

### 2.1 ShaderCompiler bytecode 缓存（`ShaderCompiler.cpp:700-790`）
```cpp
const std::string sourceKey = HashSourceKey(effectiveRequest);   // 与磁盘缓存同一 key
if (TryReadMemoryCache(sourceKey, result.SpirvWords))            // 命中 → 不拉子进程
{
    result.Success = true;
    result.Log = "ShaderCompiler: memory cache hit " + sourceKey;
    return result;
}
...
{ std::lock_guard<std::mutex> lock(m_StatsMutex); ++m_Stats.GlslangInvocations; }
const bool ok = InvokeGlslang(...);
...
StoreMemoryCache(sourceKey, result.SpirvWords);
```
`"ShaderCompiler: compiled ..."`（`ShaderCompiler.cpp:730-734`）仍只在真实调用 glslang 时打印 → 日志行计数仍是可信 oracle。

### 2.2 GPU program 缓存（`EngineShaderUtils.cpp:192-252`）
- 先查 program 表，命中即返回同一 `RHIShaderRef`，连 SPIR-V 都不编译。
- 表按 `RHI::GetInstanceSerial()` 分段：serial 变化整表丢弃，绝不把已死 context 的 GL program 交给新 RHI。
- **`ClearProgramCache()` 不清命中计数**（`g_ProgramCacheHits` 为进程生命周期诊断量）；需归零时显式调 `ResetProgramCacheStats()`。此前实现顺手把 hits 归零，会污染"清一次缓存后命中率"读数，已修正。

### 2.3 模板 + 派生实例（`SpriteMaterialFactory.cpp:137-256`、`Material.cpp:45-71`）
```cpp
std::shared_ptr<Material> SpriteMaterialFactory::CreateInstance(RHI& rhi, bool translucent)
{
    std::shared_ptr<Material> templateMaterial = GetOrCreateTemplate(rhi, translucent); // 每 variant 编译一次
    if (!templateMaterial) return nullptr;
    std::shared_ptr<Material> instance = templateMaterial->CreateRuntimeInstance();      // 不编译
    ...
}
```
`CreateRuntimeInstance` 拷贝：shading/blend、`m_Graph`（shared_ptr）、`m_ParameterLayout`、`m_TextureParameters`/`m_ScalarParameters`、`m_GPUShader`/`m_GPUShaderSkinned`、`m_MaterialShaderBindingSetLayout`、`m_CompileGeneration`，再 `EnsureRenderProxy()`。
**program 共享、参数不共享** → tint/opacity/UV 各归各，不存在串色。

## 3. 计数来源（可复现命令）

```bash
cd D:/Dev/GitRepo/minEngine/minEngine/bin
./minEngineTests.exe test material-compile-cache > /tmp/matcc.log 2>&1
grep "material-compile-cache]" /tmp/matcc.log
grep -c "ShaderCompiler: compiled" /tmp/matcc.log        # 本次会话真实 glslang 调用数
grep -c "loaded SPIR-V program"    /tmp/matcc.log        # 本次会话真实 GL program link 数
```

测试内部双轨计数：`CompileCountingSink`（匹配上两条日志）+ `ShaderCompiler::GetCompileStats()` / `SpriteMaterialFactory::GetCacheStats()`。

## 4. before / after 实测

### 4.1 before（修复前，同一套件同一命令）
```
[material-compile-cache] widgets=32 spirvArtifacts=64 programLinks=32 elapsedMs=6695.5

D:\Dev\GitRepo\minEngine\minEngine\Tests\Suites\MaterialCompileCacheTest.cpp:219:
TEST CASE:  material-compile-cache: one widget batch compiles each stage once [full]

D:\...\MaterialCompileCacheTest.cpp:257: ERROR: CHECK( spirvArtifacts == 2 ) is NOT correct!
  values: CHECK( 64 == 2 )
```
→ 32 widget = 64 次 glslang（209 ms/widget），与消费方 100 widget ≈ 200 条 `compiled` 完全吻合。

### 4.2 after（最终，8 用例全绿）
```
[material-compile-cache] widgets=32 spirvArtifacts=2 programLinks=1 elapsedMs=277.6
[material-compile-cache] reload widgets=32 spirvArtifacts=0 programLinks=0 elapsedMs=0.4
[material-compile-cache] runtimeTintChange spirvArtifacts=0 redTintR=1.000 greenTintR=0.000
[material-compile-cache] mixedVariants spirvArtifacts=3
[material-compile-cache] warmDerive widgets=512 elapsedMs=7.03 perWidgetUs=13.72 artifacts=0; batchTint widgets=512 elapsedMs=1.84 perWidgetUs=3.60 artifacts=0
[material-compile-cache] hudWidgets=512 spirvArtifacts=2 programLinks=1 elapsedMs=275.0 perWidgetUs=537.1 templateCompiles=1 derivations=512
[material-compile-cache] afterRenderSystemShutdown programCache=0 templateCache=0 whiteTexture=0 quadMeshReady=0
[material-compile-cache] afterRebuild programCache=1 templateCache=1 whiteTexture=1 quadMeshReady=1 materialCompiled=1
```
```
[doctest] test cases:    8 |    8 passed | 0 failed | 220 skipped
[doctest] assertions: 2948 | 2948 passed | 0 failed |
[doctest] Status: SUCCESS!
[12:56:48] [Test] TestRunner: suite 'material-compile-cache' PASSED.
```

## 5. 反向验证

### 5.1 缓存层（三步递进，均已还原）
- **RV1 旁路模板缓存** → `ERROR: CHECK( stats.TemplateCompiles == 1 )`，`values: CHECK( 0 == 1 )`（reload 0.4 ms → 61.3 ms）。
- **RV2 再关 bytecode 缓存** → `ERROR: CHECK( spirvArtifacts == 3 )`，`values: CHECK( 4 == 3 )`。
- **RV3 再关 program 缓存** → `widgets=32 spirvArtifacts=64 programLinks=32 elapsedMs=7246.8` + `CHECK( spirvArtifacts == 2 )`，`values: CHECK( 64 == 2 )`（逐位复现修复前）。

### 5.2 关停释放点（两级，均已还原，`grep REVERSE-VALIDATION` 无残留）

**R1：注释 `RenderSystem::Shutdown()` 里 4 行清空**
```
[material-compile-cache] afterRenderSystemShutdown programCache=1 templateCache=1 whiteTexture=1 quadMeshReady=0

D:\...\MaterialCompileCacheTest.cpp:590:
TEST CASE:  material-compile-cache: RenderSystem shutdown releases GPU caches while the context is alive [full]

D:\...\MaterialCompileCacheTest.cpp:641: ERROR: CHECK( EngineShaderUtils::GetProgramCacheEntryCount() == 0 ) is NOT correct!
  values: CHECK( 1 == 0 )
```

**R2：只注释 `SpriteQuadMesh::Get().Shutdown();`（其余 4 行保留）**
```
[material-compile-cache] afterRenderSystemShutdown programCache=0 templateCache=0 whiteTexture=0 quadMeshReady=1

D:\...\MaterialCompileCacheTest.cpp:644: ERROR: CHECK_FALSE( SpriteQuadMesh::Get().IsReady() ) is NOT correct!
  values: CHECK_FALSE( true )
```
→ 每条断言对应一个持有者，不是一条大断言蒙过去；还原后 8/8 全绿。

**关于 UB 的诚实标注**：`glDeleteProgram` 打在已死 context 上这一 UB 在本环境**无法确定性观测**（无 GL 调试层 / 无 ASAN-GL）。因此 R1/R2 断言的是"让 UB 不可能发生"的不变量 —— 静态持有者在 context 死亡**之前**归零，且清空后 context 仍当前、`glGetError()==GL_NO_ERROR`。文档不声称"抓到了 UB"。

## 6. 构建与回归

```bash
cmake --build D:/Dev/GitRepo/minEngine/minEngine/build --target minEngineTests -j 8
```
尾行：
```
[ 88%] Linking CXX executable D:\Dev\GitRepo\minEngine\minEngine\bin\minEngineTests.exe
[100%] Built target minEngineTests
```

回归（每条 `./minEngineTests.exe test <suite>`，均 exit=0、`Status: SUCCESS!`、0 FAILED）：
`smoke`（5 支：object-manager / serialization-archive / asset-manager / reflection-function / material-ir）、`material-ir`、`sprite-translucency`、`sprite-atlas`、`sprite-runtime-consume`、`sprite-authoring`、`screen-ui-coords`、`screen-ui-orientation`、`screen-ui-image`、`screen-ui-text`、`screen-ui-hit`、`screen-ui-pointer`、`screen-ui-button`、`screen-ui-drag`、`ui-layout`、`shader-compiler`。

未跑全量 `verify.ps1`（按任务书留协调者统一执行；协调者已单独跑过）。

## 7. 与任务书的偏差

1. **改了 4 个边界外文件**（均为必需，逐条理由）：
   - `Render/Material.h/.cpp`（`Render/` 根，不在 `Render/Material/**` 下）：`CreateRuntimeInstance` 必须访问 `Material` 私有成员。加法 API，无签名变更。
   - `Render/RHI/RHI.h`：`GetInstanceSerial()`，GPU program 缓存必须按 RHI 实例分段（否则跨 context 复用已删 GL program）。
   - `Render/EngineShaderUtils.h/.cpp`：program 缓存落点（任务书明确允许 program 缓存相关）。
   - `RenderSystem.cpp`：关停释放点（协调者后续明确放行）。
2. **未做组件级「同参数共享同一 instance」**：`Framework/Components/*` 硬边界外，组件持 `shared_ptr<Material>`；改为「每 widget 一个派生实例（共享 program）」，永不串色，每实例 13.7 µs。方案与前置改动见设计 Deferred §1。
3. **未实机 Play/PIE 目视**：无编辑器权限 + 不跑全量；oracle 落在「句柄同一性 + 计数器 + 释放点不变量」层（任务书允许的退化层），并用 512 widget 压测。
4. bytecode 磁盘缓存仍未启用（`SetCacheDirectory` 无生产调用者）；进程内缓存已消除重复编译。

## 8. 未完成项
- 组件级实例去重（设计 Deferred §1，需改 `Framework/Components/*` + `RenderScene`）。
- 启动期模板预热 / 异步编译（Deferred §2）：首次加载仍有约 230 ms（2 次 glslang）抖动。
- bytecode 磁盘化（Deferred §3）。
- device rebuild / Shutdown→Initialize 循环（Deferred §5）：本仓当前不存在该路径，已用「释放后可惰性重建」用例兜底。

## 9. GPU 资源释放点与关停所有权（评审后追加）

### 9.1 释放点
`RenderSystem::Shutdown()` 首行：
```cpp
EngineShaderUtils::ClearProgramCache();
SpriteMaterialFactory::ClearTemplateCache();
ScreenUIMaterialFactory::ClearCachedResources();
ShaderCompiler::Get().ClearMemoryCache();
```
`m_SceneRenderer->Shutdown()` 之后、`m_RHI->Shutdown()` 之前：
```cpp
SpriteQuadMesh::Get().Shutdown();
```

### 9.2 为什么在 context 存活期内
- context 唯一销毁点：`GLFWWindowSystem::Shutdown()`（`GLFWWindowSystem.cpp:170-196`）。
- 关停顺序（`Runtime/Engine.cpp`）：`m_SceneManager->Shutdown()` **:500** → `m_RenderSystem->Shutdown()` **:542** → `m_WindowSystem->Shutdown()` **:549**。
- `OpenGLRHI::Shutdown()`（`OpenGLRHI.cpp:73-79`）不销毁 context。
- `RenderSystem::Shutdown()` 唯一调用者：`Engine.cpp:542`。

### 9.3 清空 vs 活 shared_ptr（谁最后释放）
- `RHIShaderRef` 是 `shared_ptr`；派生 Material 拷贝了同一 ref（`Material.cpp:63-66`）→ 清缓存只减计数，活 Material 仍持有 program，最后一个持有者释放时才 `glDeleteProgram`。
- 模板清空不影响派生实例：它们持有 `shared_ptr<MaterialEdGraph>` 与 `RHIShaderRef` 的拷贝。
- 白纹理同理。
- **残留（既有）风险**：外部在 `Shutdown()` 后仍持有 Material 时，program 析构会打在死 context 上；修复前每个 widget 的 Material 都是这类持有者（`WidgetComponent.h:120`），风险面大于本次新增缓存。本 Feature 不加宽改动面，靠「场景先销毁（500 < 542）」兜住并在设计 §refcount 记录。

### 9.4 幂等性与重建（含"本仓不支持 Shutdown→Initialize 循环"的证据）
- 重复 `Shutdown()` 安全（对空表/空 ref 幂等，RHI/renderer 各有 null 守卫）——用例 `shutdown is idempotent and GPU caches rebuild lazily` 连调两次并断言缓存仍为空。
- 释放后惰性重建：`SpriteQuadMesh::EnsureInitialized` 有 `if (IsReady()) return;`（`SpriteQuadMesh.cpp:35-38`）；`GetOrCreateWhiteTexture` 有 `!s_WhiteTexture || GetRHITexture()==nullptr` 重建分支（`ScreenUIMaterialFactory.cpp:18-26`）。用例实测 `afterRebuild programCache=1 templateCache=1 whiteTexture=1 quadMeshReady=1 materialCompiled=1`。
- **本仓不存在 Shutdown→Initialize 循环**：`RenderSystem::Initialize` 全仓唯一调用者为 `Engine.cpp:455`（`Engine::Initialize` 内一次）；Editor 只用 `Get().GetRHI() / PresentFrame() / SetPresentPassEnabled()`；无 PIE / 设备重建重新 Initialize 的路径。故未把它当作受支持路径，但仍加重建用例，防止将来引入该路径时 UI 全消失。

## 10. 顺带发现的既有问题（非本次引入，本次顺手修）

| # | 缺陷 | 证据 | 本次处置 |
|---|------|------|----------|
| 1 | 静态白纹理持 GPU 纹理，随进程静态析构在 context 死后 `glDeleteTextures` | `ScreenUIMaterialFactory.cpp:11-15`（修复前为函数内静态）、`OpenGLRHIResources.cpp:528-534` | 提为可清缓存 + `ClearCachedResources()` 纳入 `RenderSystem::Shutdown()`；**文件本就在允许改动范围** |
| 2 | `static SpriteQuadMesh instance` 持 `RHIBufferRef`×2 + `RHIVertexInputLayoutRef`，其自带 `Shutdown()` **全仓无调用者** | `SpriteQuadMesh.cpp:25`、`SpriteQuadMesh.h:34/47-49` | 从允许改的 `RenderSystem.cpp` 调用 `SpriteQuadMesh::Get().Shutdown()`（renderer 关停后、RHI 关停前）；**零改动被禁文件** |
| 3 | 测试 fixture 在 context 销毁后才让 GPU 静态析构 → 下一个用例继承死 GL 名字 | `MaterialCompileCacheTest.cpp` `ScopedHeadlessGl46Context::~ScopedHeadlessGl46Context` | fixture 在销毁 window **之前**释放同一批静态，镜像生产顺序 |

这三条与 MAT-F08 主缺陷（编译风暴）无关，是审计同类静态持有者时的附带收益，单列以便 review 区分"本次引入"与"顺手修"。

## 11. 碰过的文件清单
`Runtime/Function/Render/RenderSystem.cpp`、`Runtime/Function/Render/EngineShaderUtils.{h,cpp}`、`Runtime/Function/Render/Material.{h,cpp}`、`Runtime/Function/Render/RHI/RHI.h`、`Runtime/Function/Render/ShaderCompiler/ShaderCompiler.{h,cpp}`、`Runtime/Function/Render/ScreenUI/ScreenUIMaterialFactory.{h,cpp}`、`Runtime/Function/Render/Sprite/SpriteMaterialFactory.{h,cpp}`、`Tests/TestSuiteRegistration.cpp`（纯追加）、`Tests/Suites/MaterialCompileCacheTest.{h,cpp}`（新）、`docs/ai/Render/Material/MAT-F08_*_{DESIGN,IMPLEMENTATION}.md`（新）。
未 commit / 未 push。
