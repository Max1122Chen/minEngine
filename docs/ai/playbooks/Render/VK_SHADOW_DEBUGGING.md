# Playbook — Vulkan shadow debugging

**Type:** Reference playbook (Tier B)  
**Last updated:** 2026-08-31 (handoff §4.5–4.6, §7)  
**Related bugs:** [BUG-RENDER-013](../../bugs/BUG-RENDER-013.md) (closed), [BUG-RENDER-010](../../bugs/BUG-RENDER-010.md) (closed)  
**Related design:** [RND-F14](../../Render/RND-F14_SHADOW_PASS_UBO_LIFETIME_DESIGN.md), [RND-F13 ManualRenderer](../../Render/RND-F13_MANUAL_RENDERER_DESIGN.md), [Piccolo reference](../../Render/VK_SHADOW_PICCOLO_MINENGINE_REFERENCE.md)

---

## 1) Symptom → hypothesis quick map

| Symptom | Likely class | First check |
|---------|--------------|-------------|
| Spot-only ≈ OK; full-map (Dir+Spot+Point) broken | Shared shadow UBO / binding lifetime | §2 UBO pattern |
| All cascades show same mesh silhouette | ViewProj overwritten at offset 0 | §2 UBO pattern |
| Dir shadow changes when toggling Point Cast Shadow | Stale UBO or wrong matrix slot | §2 + `BuildShadowDrawCommands` bindings |
| Manual == Forward wrong (same scene) | **Not** RDG-primary | Use `--renderer manual` to confirm |
| Receiver self-shadow / acne on large plane | Map content vs sampling (§4) | §3 cull audit; DIR_SHADOW_DEBUG_MODE 5/6 |
| Dir **and** Spot self-shadow; Point OK; object varies by light | Winding / write path (§4.5) | §4.6 bias layers; Debug 5/6 |
| Shadow map empty / partial | Layer/face index, clear, layout | RenderDoc depth pass per slice |

---

## 2) Host-visible UBO lifetime (fixed 2026-08-31)

### Pattern

Vulkan records all shadow draws into **one command buffer** per frame submit. If C++ writes the same host-visible UBO at **offset 0** before each draw, the GPU reads **final** contents at execution time → every draw sees the last matrix/params.

### Precedent

`EngineSceneBindingSets::WriteNextPerObjectModel` — ring buffer + per-draw `BufferOffset` (commit `bbdcdca`, BUG-RENDER-005/006).

### Fix (RND-F14)

- `ShadowUniformBuffers`: Dir/Spot fixed array slots; Point ViewProj ring; Params ring.
- `ShadowDrawCommand` carries `ViewProjUniformBuffer/Offset` and `ParamsUniformBuffer/Offset`.
- `ShadowPass::DrawOpaqueMeshes` binds descriptor range per draw.
- **Trap:** set uniform bindings on commands **before** inserting into `ctx.ShadowDrawCommands` (directional loop).

### Isolation experiments

| Config | Purpose |
|--------|---------|
| `MAX_*_SHADOW_MAPS=0` | Dir-only shadow budget |
| `MAX_CASCADES=1` | Single cascade |
| `--renderer manual` | Bypass RDG; shared ShadowPass only |

---

## 3) Face culling audit (next round — receiver self-shadow)

**User observation (2026-08-31):** VK still shows receiver self-shadow; suspected missing Front cull.

### Code audit result (2026-08-31)

**Front cull is configured** on the shadow path — not absent in source.

| Layer | Value (VK) |
|-------|------------|
| `RHIClipSpaceCapabilities.cpp` | `kVulkanShadowPass.ReceiverFacingCullMode = Front` |
| `GetEffectiveCullMode()` | Returns `Front` (`ViewportFlipY = false`) |
| `ShadowPass::Initialize()` | `bCullEnabled = true`, `CullMode = GetEffectiveCullMode()` |
| `VulkanRHIResources.cpp` PSO | `VK_CULL_MODE_FRONT_BIT`, `VK_FRONT_FACE_COUNTER_CLOCKWISE` |

OpenGL shadow path uses the same `GetShadowPassCapabilities()` → `GL_FRONT` when enabled.

**VK bias trial (2026-08-31):** `kVulkanShadowPass` slope `2.0`, constant `2.0` (was `1.5` / `0`). GL remains `2.0` / `4.0`. If self-shadow unchanged, bias is unlikely the root cause — prioritize §3.2 cull A/B.

Lit passes (`EnginePipelineLayouts` scene mesh PSO) leave **`bCullEnabled = false`** (default) — only shadow pass enables cull.

### Cull A/B (if bias trial inconclusive)

Temporarily set `kVulkanShadowPass.ReceiverFacingCullMode = RHICullMode::None` in `RHIClipSpaceCapabilities.cpp`, rebuild, compare ground-plane shadow. If **no visible change**, suspect PSO not applied or receiver geometry not in shadow pass; use RenderDoc.

### If self-shadow persists — investigate next

1. **RenderDoc:** Confirm shadow draw calls use `VK_CULL_MODE_FRONT_BIT` (not leaked PSO with cull off).
2. **Winding / frontFace:** Meshes may be CW-outward while PSO assumes CCW front — Front cull would cull the wrong side. Try `VK_FRONT_FACE_CLOCKWISE` on shadow PSO only (A/B).
3. **Depth bias:** VK uses `slope=1.5`, `constant=0`; GL uses `slope=2`, `constant=4`. Flat receivers may need non-zero constant bias on VK (acne vs true self-shadow).
4. **Shader PCF bias:** `MaterialSceneShadows.glslinc` — separate from raster bias.
5. **Cast vs receive:** Disable plane **Cast Shadow** — if blob disappears, geometry is still writing into the map (cull/winding); if blob remains, sampling/bias issue.

### Suggested next-round order (§3 — cull path)

```
RenderDoc cull mode → frontFace A/B → CullMode::None A/B
```

---

## 4) GL 正常 / VK 自阴影 — 换角度排查（2026-08-31）

**前提：** Log 确认 `cull=Front`；VK bias 提高无改善；**OpenGL 无此问题**。  
→ 不宜再盯「C++ 没开 Front」，应先二分 **shadow map 里有没有接收体** vs **采样比较错了**。

### 4.1 二分：生成 vs 采样

| 问题 | 若「是」 | 若「否」 |
|------|----------|----------|
| Debug 5：shadow map 里是否能看到地面深度？ | 写路径：cull 未生效、**frontFace 与 winding 反了**、双面几何 | 写路径基本 OK → **读路径 / depth 比较** |
| 关接收体 **Cast Shadow** 后 map 里该处深度是否消失？ | 确认是几何写入 | 可能是采样假影 |
| Debug 6：`projCoords.z - sampledDepth` 在接收体 ≈0？ | 在比自己的深度 | 别物遮挡或 UV 偏移 |

`DIR_SHADOW_DEBUG_MODE`：在 include `MaterialSceneShadows.glslinc` 之前 `#define`（5=sampled depth，6=delta）。

### 4.2 高概率 VK 特异原因

| # | 假设 | 为何 GL 可过 | 验证 |
|---|------|--------------|------|
| **P1** | `gl_FragDepth = gl_FragCoord.z` **绕过** polygon offset | 显式写深度时 VK raster bias 可能不生效 | Dir/Spot 分支去掉 `gl_FragDepth`，用固定管线 depth+bias |
| **P2** | 读路径 `ndc.z` 与 map 深度语义不一致 | ZO define 未覆盖某材质路径；`ndc.z` ≠ `gl_FragCoord.z` | Debug 4/6；VK/GL 并排 |
| **P3** | Shadow **screen winding** GL≠VK：同配 Front 剔不同面 | GL viewport 左下原点；VK shadow 不 flip → Front 保留光朝外面 | **VK `ReceiverFacingCullMode=Back`**（几何上等价 GL Front） |
| **P4** | Shader slope bias 在 grazing 角不足 | 与 raster bias 独立的一层 | 临时加大 `CalcDirLight` 里 bias |
| **P5** | CSM ortho Z / ExpandCascadeZ 包住接收体 | 光空间体积过大 | cascade debug 1 |

**已降级：** `GetShadowMapSampleFlipY()` 未接 shader（TD-025 scheme A）；RDG/UBO（RND-F14 已修）。

### 4.3 推荐实验顺序

1. Debug **5** — 地面是否在 shadow map 里？  
2. 关接收体 Cast Shadow + 再看 Debug 5。  
3. **P3** — VK shadow `Back` cull（已试 2026-08-31；GL 仍 Front）  
4. **P3b** — 若仍不行：`VK_FRONT_FACE_CLOCKWISE` + Front  
5. RenderDoc 抽地面 shadow draw  

### 4.4 隔离实验记录（2026-08-31）

| # | 改动 | 结果 | 结论 |
|---|------|------|------|
| E0 | 基线：Front cull + bias 2/2 | 地面痤疮；cube/sphere 假影随相机 | 非「未开 cull」 |
| E1 | P1：Dir/Spot 不写 `gl_FragDepth` | 无改善 | P1 非主因（或不足够） |
| E2 | VK `Back` cull（GL Front） | 地面好转；竖直/曲面仍差；假影仍跟相机 | 朝向相关 workaround，非闭合修复 |
| E3 | 回 Front + **`MAX_CASCADES=1`** + **`DIR_SHADOW_FORCE_CASCADE=0`** | **与 E0 相同** | **排除**多 cascade 索引/边界混用（P5↓） |

**E3 后共识（用户验收 2026-08-31）：**

- 问题可认定在 **Dir + Spot** 写路径（Point 暂无明显问题；不同物体随光源变化）。
- **不是**「选了错误 cascade」——单级联 + 强制 layer 0 仍复现。
- 相机耦合 **不能** 用「关掉级联」解释掉：即使 `cascadeCount=1`，`BuildDirectionalShadowDrawCommands` 仍从 **相机视锥** 切 ortho 包围盒并做 texel snap → 矩阵每帧随相机变；若接收体仍被写入 map，采样结果在接收面上会跟相机走。
- 与历史一致：关接收体 **Cast Shadow** → 大块假影消失 → **写路径**（几何进 map）仍是第一嫌疑，读路径 PCF/bias 为第二层。

**降级假设：** P5（多 cascade 混用）、P1（`gl_FragDepth` 绕过 bias 为主因）。

**升序假设：** P3 / winding — Back cull 仅「修」水平面说明剔除的是 **与光向相关的错误三角面集合**；需 scheme B 或 `frontFace` A/B 做 GL/VK 闭合，而非永久 Back/Front 分裂。

### 4.5 症状细化（2026-08-31 用户反馈）

| 光源 | VK 自阴影 / 假影 | 备注 |
|------|------------------|------|
| **Directional** | 有（地面痤疮 + cube/sphere 相机耦合假影） | 矩阵仍从相机视锥切 ortho（即使单 cascade） |
| **Spot** | **也有** | 与 Dir 共享 2D 投影 Z + Front cull 写路径 |
| **Point** | 暂无明显问题 | 线性 `gl_FragDepth` 写 / 读自洽；**不用** raster polygon offset |

- **不同物体**在不同光源下表现不同 → 朝向 / winding，非「全局 VK depthBias 未开」。
- Dir-only 收窄已过时；Spot 需纳入同一写路径修复。

### 4.6 Shadow bias 两层架构（RHI 只读审计）

| 层 | 何时 | 配置来源 | OpenGL | Vulkan |
|----|------|----------|--------|--------|
| **A. Raster** | Shadow pass **写** depth map | `GetShadowPassCapabilities()` → `ShadowPass` PSO | `glPolygonOffset(slope, constant)` on bind PSO | PSO `depthBiasEnable` + `depthBiasSlopeFactor` / `depthBiasConstantFactor`（**静态**，无 `vkCmdSetDepthBias`） |
| **B. Shader** | Lit pass **读** map | `MaterialSceneShadows.glslinc` / `Phong.frag` | 同 shader（+ `MINENGINE_CLIP_DEPTH_ZERO_TO_ONE`） | 同左 |

**Caps 数值（shadow pass only）：**

| Backend | slope | constant |
|---------|-------|----------|
| GL | 2.0 | 4.0 |
| VK | 2.0 | 2.0 |

**按光源写深度（`ShadowPass.frag` + `ShadowPassParamsUBO.UseLinearDepth`）：**

| 光源 | `UseLinearDepth` | 深度来源 | Raster bias |
|------|------------------|----------|-------------|
| Dir / Spot | 0 | 固定管线 `gl_Position.z` | **应生效** |
| Point | 1 | `gl_FragDepth = distance / farPlane` | **绕过**（规范行为） |

**读侧 shader bias（`MaterialSceneShadows.glslinc`，与 RHI 无关）：**

- Dir：`ComputeDirectionalShadowFactor` — `(0.0015 + 0.02×(1-ndotl))×cascadeScale` + 几何 `samplePos` 偏移
- Spot：`max(0.0005, 0.005×(1-ndotl))`
- Point：`max(0.002, 0.01×(1-ndotl))`（最大）
- Legacy `Phong.frag` Dir：`max(0.0005, 0.005×(1-ndotl))`（弱于 graph 路径）

**审计结论：** VK raster bias **代码路径已接**（PSO + Dir/Spot 不写 `gl_FragDepth`）。试验提高 slope/constant **无改善** + Point 正常 → 当前症状更像 **错误面写入 map（cull/winding）**，而非 bias 开关失效。D32F 下 VK constant 语义与 GL `units` 不同（见 `RHIClipSpaceCapabilities` 注释）。

---

## 5) Useful commands

```powershell
# Diagnostic renderer (no RDG)
minEngine\bin\Editor.exe --renderer manual --rhi vulkan --project ..\MyMEProject\MyMEProject.meproject

# Default forward + RDG
minEngine\bin\Editor.exe --rhi vulkan --project ..\MyMEProject\MyMEProject.meproject

.\scripts\verify.ps1
```

---

## 6) Key code entry points

| Area | File |
|------|------|
| Shadow cull + bias template | `RenderPipeline/RenderPasses/ShadowPass.cpp` |
| Backend caps | `RHI/RHIClipSpaceCapabilities.cpp` |
| VK PSO rasterizer | `Vulkan/VulkanRHIResources.cpp` |
| Uniform slots | `RenderPipeline/Shadow/ShadowUniformBuffers.*` |
| Command build | `ForwardRenderer::BuildShadowDrawCommands` |
| Shader read bias | `Assets/EngineDefault/Shaders/Include/GLSL/MaterialSceneShadows.glslinc` |
| Shadow frag write | `Shaders/ShadowPass.frag` |
| GL polygon offset | `OpenGL/OpenGLRHI.cpp` `ApplyGraphicsPipelineState` |

---

## 7) Agent handoff（2026-08-31）

**Session note:** [`sessions/2026-08-31-vk-shadow-self-shadow-handoff.md`](../../sessions/2026-08-31-vk-shadow-self-shadow-handoff.md)

**状态：** RND-F14 Done；本轨为 **shadow 质量**（接收体假自阴影），非 UBO/RDG。

**工作区 TEMP（修前恢复）：** `MAX_CASCADES=1`，`DIR_SHADOW_FORCE_CASCADE=0`，E1 `ShadowPass.frag` 无 Dir/Spot `gl_FragDepth`。

**下一步：** Debug 5/6 → RenderDoc → scheme B 或 `VK_FRONT_FACE_CLOCKWISE` A/B。勿永久 E2 Back cull。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-31 | §4 GL/VK differential; bias trial inconclusive |
| 2026-08-31 | P1: omit gl_FragDepth (Dir/Spot); inconclusive |
| 2026-08-31 | §4.5–4.6 Dir+Spot、depth bias 两层；§7 handoff |
| 2026-08-31 | §4.4 E3: single cascade — CSM selection ruled out; dir-only write-path |
| 2026-08-31 | P3: VK shadow Back cull (GL Front equivalent for winding) |
