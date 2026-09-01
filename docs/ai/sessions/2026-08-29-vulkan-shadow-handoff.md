# Agent 转交 — Vulkan 阴影（BUG-RENDER-010）分层回退后重启

Last updated: 2026-08-29  
Status: **活跃 — 供干净上下文 Agent 首条消息引用**  
分支：`feat/render`  
基线 commit：`217cf1e`（TD-025 基础设施）+ **分层回退**（shader/flip/injection → `bbdcdcab` 语义）  
工作区：`d:\Dev\GitRepo\minEngine`

---

## 0) 一句话任务

在 **保留 TD-025 Capabilities 基础设施** 的前提下，从 **无 FlipY/MinEngineShadowProject 注入** 的干净 shader 基线出发，**闭合 Vulkan 三种阴影（Dir CSM / Spot / Point）的写→读 clip-space 链**，使 VK 与 GL 在 `test` 场景目视一致。BUG-RENDER-010 仍为 **Open**。

---

## 1) 必读（按顺序）

1. `docs/ai/PROJECT_CONTEXT.md`
2. `docs/ai/Render/RND-TD025_CLIP_SPACE_CAPABILITIES_DESIGN.md` — 三层约定 A/B/C
3. `docs/ai/Render/RND-TD025_CLIP_SPACE_CAPABILITIES_IMPLEMENTATION.md` — 切片状态（S08 已移除）
4. `docs/ai/bugs/BUG-RENDER-010.md` — 症状与回归清单（状态应改为 Open）
5. 本文件 §4–§7

**Tier C：** 以 **代码 + 用户目视** 为准；文档里写「Fixed」不可信。

---

## 2) 分层回退已完成（Plan A）

### 保留（来自 `217cf1e` / 当前 HEAD 基础设施）

| 区域 | 路径 / 行为 |
|------|-------------|
| Capabilities | `RHIClipSpaceCapabilities.{h,cpp}` — ZO/N1、TextureOriginY、Scene/Shadow/Cube viewport flip 表 |
| 矩阵 helper | `RHIClipSpace.{h,cpp}` — `MakePerspective` / `MakeOrthographic`（RH_ZO on VK） |
| Viewport API | `RHICommandList::SetViewport(..., RHIViewportConvention)` |
| ShadowPass CPU | `ShadowPass.cpp` — `ShadowMap2D` / `CubeMapFace` convention；`GetEffectiveCullMode()`；depth bias 来自 caps |
| 灯光矩阵 | `ForwardRenderer.cpp` — CSM/Spot/Point `lookAt` + `RHIClipSpace` 投影；18 槽 shadow RDG |
| 绑定 | `EngineSceneBindingSets` — shadow 槽清空、dir shadow index |
| Editor | ImGui scene color UV（`GetImGuiSceneColorUv`） |
| 材质门控 | `MaterialPhongLighting.glslinc` — `dirShadowIndex >= 0` 才采样（S06，**未回退**） |

### 已回退到 `bbdcdcab` 语义

| 文件 | 变化 |
|------|------|
| `MaterialSceneShadows.glslinc` | 移除 `MinEngineShadowProject`；恢复 `projCoords = xyz/w; *0.5+0.5` 直读 depth |
| `Phong.frag` | 同上（legacy 内联 shadow 路径） |
| `ShadowPass.vert` / `ShadowPass.frag` | **勿回退 bbdcdc** — 须保留 `set=0, binding=0/1/2`（Vulkan pipeline layout）；仅 lit 采样 shader 回退 |
| `ShaderCompiler.{cpp,h}` | 移除 `InjectClipSpaceDefines` / `MINENGINE_SHADOW_MAP_SAMPLE_FLIP_Y` / ZO define 注入 |
| `ShaderCompilerTest.cpp` | 移除 flip 注入单测 |
| `MaterialIRTest.cpp` | 移除 `InjectClipSpaceDefines` 相关断言 |

### 已删除（用户要求）

- `ShadowDebugPass`、F9 切换、`ShadowDebug.frag`、`SceneDrawDesc::ShadowDebugMapView` 等 TD025-S08 脚手架

### 当前 VK ShadowPass caps（scheme A，写路径）

```cpp
// RHIClipSpaceCapabilities.cpp — kVulkanShadowPass
ViewportFlipY = false;
ReceiverFacingCullMode = Front;
DepthBiasSlopeScale = 1.5f;
DepthBiasConstant = 0.0f;
```

`GetShadowMapSampleFlipY()` 仍存在（VK→true），但 **shader 侧已不再使用**。

---

## 3) 问题症状（BUG-RENDER-010）

**环境：** Editor，`MyMEProject` → `test` 场景，`--rhi vulkan`

| 现象 | 备注 |
|------|------|
| 100×100 plane 巨大假自阴影 | 关 plane Cast Shadow 后巨斑消失 → 与 caster/深度写/读有关 |
| 三种光型阴影均异常 | Dir CSM、Spot、Point 都有问题 |
| OpenGL 同场景正常 | `--rhi opengl` 对照 |
| Spot 有时像「全立方体投影」、随相机旋转恶化 | 可能是 clip 闭合问题，不单是 spot `lookAt` |
| Point 用方向向量 + linear depth | **不经过** `SpotLightViewProj` / `MinEngineShadowProject` |

**已排除（多轮审计）：**

- `lookAt` GL/VK 公式相同
- CSM / Spot / Point 的 CPU `ViewProj` 构建公式与 GL 路径一致
- 根因倾向 **Layer B（viewport raster）与 Layer C（shader 采样）未与 Layer A（ZO 矩阵）闭合**，而非 PCF 核大小

---

## 4) 已尝试且未解决的方向（勿重复踩坑）

### TD-025 单体提交 `217cf1e`

- 引入 `RHIClipSpaceCapabilities` + `MinEngineShadowProject` + Vulkan `SAMPLE_FLIP_Y=1` 注入
- Shadow scheme A：`ViewportFlipY=false` + Front cull
- **结果：** 构建通过，但用户目视 VK 三种阴影仍错；GL 仍 OK

### Shader 注入修补

- `InjectClipSpaceDefines` 对 OpenGL 也注入（曾误伤）
- 修复「`#if defined(MINENGINE_...)` 导致跳过注入」的 false skip
- Vulkan 材质统一 `MINENGINE_CLIP_DEPTH_ZERO_TO_ONE` + `SAMPLE_FLIP_Y 1`
- **结果：** 仍无法闭合；用户怀疑多层 flip 互相抵消/叠加，要求分层回退

### Shadow depth 可视化（TD025-S08）

- F9 切换 raw depth 视图；稳定 RDG 拓扑、`NeedRenderPass` gating 等
- **GL：** 仍显示 rock/albedo，未成功替换 SceneColor
- **VK：** F9 崩溃（帧中 `BuildFrameRenderGraph` / attachment discard）
- **用户决定：** 全部拆除，不在闭合前再做 debug pass

### 其他

- Plane cast off 作为诊断（确认巨斑来自 plane 自写深度）
- `dirShadowIndex` 门控（保留，与 flip 无关）
- BUG 文档曾标 Fixed — **错误**，需 Open

---

## 5) 回退后现状（2026-08-29）

- **构建：** `cmake --build minEngine/build --target Editor minEngineTests` ✅
- **ShaderCompiler 单测：** smoke 通过（flip 测试已随回退移除）
- **MaterialIR smoke：** 失败于 UBO layout 字符串 golden（`layout (std140, binding = 0) uniform PerFrameData`）— **与阴影回退无关**，可能是并行 descriptor 变更；修 golden 或单独排期
- **目视：** 尚未重新验收；预期 GL 阴影仍 OK，VK 仍错（但错误形态可能不同于 `217cf1e` 的 flip 叠加态）

**Shader 采样现状（bbdcdc 语义）：**

```glsl
vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
projCoords = projCoords * 0.5 + 0.5;
float currentDepth = projCoords.z;  // 无 ZO 分支；VK 上 depth 读可能系统性偏差
```

**CPU 矩阵现状：** VK 已用 `RHIClipSpace::MakePerspective` / `MakeOrthographic`（ZO）。  
→ **已知张力：** A 层 ZO，C 层仍 NDC `*0.5+0.5` 且 `z` 未做 ZO 映射。GL 上 N1+该式历史正确；VK 需重新闭合 C（或临时只改 C，勿同时乱改 B）。

---

## 6) 推荐修复顺序（新思路）

按 TD-025 设计 **一次只动一层**，每步 GL 回归 + VK `test` 目视：

### Step 1 — 只修 Layer C（读）

在 `MaterialSceneShadows.glslinc`（及 Material IR 共用路径）引入 **单一** 投影 helper，例如：

```glsl
// 伪代码 — 用 Caps 或编译期 define，不要读写双 flip
float ShadowDepthFromNdc(float ndcZ) {
#if MINENGINE_CLIP_DEPTH_ZERO_TO_ONE  // 仅当重新引入注入或 glslinc 常量
    return ndcZ;
#else
    return ndcZ * 0.5 + 0.5;
#endif
}
```

- **先不要** `SAMPLE_FLIP_Y`
- **先不要** shadow viewport flip
- 验证 Dir + Spot PCF 是否改善

### Step 2 — 若 depth 对了、UV 仍上下颠倒

二选一（**禁止同时**）：

- **B：** `GetShadowPassCapabilities().ViewportFlipY = true`（写路径 flip），或
- **C：** `GetShadowMapSampleFlipY()` → shader `uv.y = 1.0 - uv.y`

用同一 helper 闭合，记录哪一侧生效。

### Step 3 — Point cube

Point 不用 `ViewProj` 采样；查 `SamplePointShadowPCF` 与 cube face viewport / 方向向量是否与 VK `TextureOriginY=Top` 一致。

### Step 4 — 文档与 BUG

目视 OK 后更新 `BUG-RENDER-010`、TD-025 → Done；**不要**在未闭合前标 Fixed。

### 禁止

- 恢复 ShadowDebugPass 直到 Lit 采样基本正确
- 同时改 viewport flip + sample flip + 改 `lookAt`
- 全仓 `git reset` 到 `bbdcdc`（会丢掉 Capabilities）

---

## 7) 代码地图

| 职责 | 路径 |
|------|------|
| Caps 真相表 | `minEngine/.../RHI/RHIClipSpaceCapabilities.{h,cpp}` |
| 矩阵 | `minEngine/.../RHI/RHIClipSpace.{h,cpp}` |
| Dir/Spot/Point 矩阵 | `minEngine/.../ForwardRenderer.cpp` — `BuildDirectionalShadowDrawCommands` 等 |
| Shadow 写 | `minEngine/.../RenderPasses/ShadowPass.cpp`；`Shaders/ShadowPass.{vert,frag}` |
| Shadow 读 | `Assets/.../GLSL/MaterialSceneShadows.glslinc`；`MaterialPhongLighting.glslinc` |
| 绑定 | `minEngine/.../EngineSceneBindingSets.cpp` |

---

## 8) 验证命令

```powershell
cd d:\Dev\GitRepo\minEngine\minEngine\build
cmake --build . --target Editor minEngineTests

cd ..\bin
.\minEngineTests.exe test smoke

# 目视 — 先 GL 再 VK
.\Editor.exe --rhi opengl --project ..\MyMEProject\MyMEProject.meproject
.\Editor.exe --rhi vulkan --project ..\MyMEProject\MyMEProject.meproject
```

`test` 场景：plane + cube；切换 Dir/Spot/Point Cast Shadow；orbit 相机看 spot/point 稳定性。

---

## 9) 约束

- **不要 commit** 除非用户明确要求
- C++：优先成员函数，少匿名命名空间 helper（见 workspace rules）
- 别模块 drive-by fix 先报 BUG
- Pre-flight：改 shader 闭合前确认是否需恢复 **受限版** `InjectClipSpaceDefines`（仅 ZO define，flip 单独决策）

---

## 10) 给 Agent 的首条提示词（可直接复制）

```
你在 minEngine 仓库 feat/render 分支。BUG-RENDER-010：Vulkan Editor 三种阴影错误，OpenGL 正常。

已完成分层回退（Plan A）：
- 保留 TD-025：RHIClipSpaceCapabilities、RHIClipSpace、RHIViewportConvention、ForwardRenderer ZO 矩阵、ShadowPass convention/cull、EngineSceneBindingSets。
- 回退 bbdcdc 语义：MaterialSceneShadows.glslinc、Phong.frag、ShadowPass shaders、ShaderCompiler（无 InjectClipSpaceDefines/FLIP_Y）、相关单测。
- 已删除 ShadowDebugPass/F9。

请先读：
docs/ai/sessions/2026-08-29-vulkan-shadow-handoff.md
docs/ai/Render/RND-TD025_CLIP_SPACE_CAPABILITIES_DESIGN.md
docs/ai/bugs/BUG-RENDER-010.md

任务：按 handoff §6 从 Layer C（ZO depth 读）开始闭合 VK shadow 写→读链；一次只动一层；每步 build + GL/VK test 场景目视。不要恢复 shadow debug。不要 commit 除非我要求。
```

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-29 | 分层回退执行；S08 移除；本 handoff 创建 |
