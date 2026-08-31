# Agent 转交 — VK 接收体自阴影（RND-F14 后质量轨）

Last updated: 2026-08-31  
Status: **活跃 — 供干净上下文 Agent 首条消息引用**  
分支：`feat/render`  
工作区：`d:\Dev\GitRepo\minEngine`

---

## 0) 一句话任务

RND-F14 已修复 VK 多光源 shadow UBO 寿命；三种光源 shadow **可用**。剩余问题：**VK 接收体假自阴影**（Dir + Spot 明显，Point 暂无明显问题），GL 正常。主因倾向 **shadow map 写入路径 cull/winding（GL≠VK）**，非 CSM 级联选择、非全局 depth bias 未开。

---

## 1) 必读（按顺序）

1. `docs/ai/ACTIVE_WORK.md` — 当前 backlog 一行摘要
2. `docs/ai/playbooks/Render/VK_SHADOW_DEBUGGING.md` — **主 playbook**（§4.4–§4.6、§7）
3. `docs/ai/bugs/BUG-RENDER-010.md` — §已知后续（质量轨，非阻塞关账）
4. `docs/ai/Render/RND-F14_SHADOW_PASS_UBO_LIFETIME_DESIGN.md` — Done；勿再当主因
5. 本文件 §3–§6

**Tier C：** 以代码 + 用户目视为准。

---

## 2) 已完成（勿重复做）

| 项 | 状态 |
|----|------|
| RND-F14 ShadowPass UBO 寿命 | **Done / Verified**（commit `f07c803` 等） |
| BUG-RENDER-013 / 010 / 011 | **Closed**（功能可用；010 有质量后续） |
| RND-F13 ManualRenderer | **Done**（`--renderer manual`） |
| RDG 主因假设 | **降级**（Manual == Forward 错影 → 非 RDG 独有） |

---

## 3) 用户目视共识（2026-08-31）

| 观察 | 含义 |
|------|------|
| **Dir + Spot** 均有接收体自阴影 / 假影 | 非仅方向光；共享 **2D 投影 Z** shadow 写路径 |
| **Point** 暂无明显同类问题 | Point 写 `gl_FragDepth`（线性距离），读侧同语义；不走 raster polygon offset |
| **不同物体**在不同光源下中招不同 | 朝向 / winding 相关，非全局 bias 开关失效 |
| 关接收体 **Cast Shadow** → 假影消失 | 几何仍 **写入** shadow map（写路径第一嫌疑） |
| cube/sphere 假影 **随相机**（Dir） | 单级联仍复现 → 非 cascade **索引**；矩阵仍绑相机视锥 + texel snap |
| PCF 软边可见 | 真 shadow map 采样，非纯光照 bug |
| **GL 同场景正常** | VK 栅格 / winding 不等价于 GL |

---

## 4) 隔离实验摘要（工作区可能仍含 TEMP）

| # | 改动 | 结果 |
|---|------|------|
| E0 | Front cull + VK bias 2/2 | 基线症状 |
| E1 | Dir/Spot 不写 `gl_FragDepth`（P1） | 无改善 |
| E2 | VK Back cull | 仅水平地面好转；竖直/曲面仍差 |
| E3 | `MAX_CASCADES=1` + `DIR_SHADOW_FORCE_CASCADE=0` | **与 E0 相同** → **排除**多 cascade 选择 |

### 工作区 TEMP（正式修前须恢复）

| 文件 | TEMP 值 | 恢复为 |
|------|---------|--------|
| `EngineRenderLimits.h` | `MAX_CASCADES = 1` | `4` |
| `MaterialSceneShadows.glslinc` | `DIR_SHADOW_FORCE_CASCADE 0` | `-1` |
| `Phong.frag` | 同上 | `-1` |
| `ShadowPass.frag` | Dir/Spot 不写 `gl_FragDepth` | 视 P1 结论保留或回退 |
| `RHIClipSpaceCapabilities.cpp` | Front cull, bias 2/2 | 修 winding 后再定 |

---

## 5) Depth bias 架构（只读审计结论）

**两层独立：**

| 层 | 阶段 | 配置 | Dir/Spot | Point |
|----|------|------|----------|-------|
| **A. Raster** | Shadow pass 写 map | `kOpenGLShadowPass` / `kVulkanShadowPass` → ShadowPass PSO | `glPolygonOffset` / PSO `depthBiasEnable` | **绕过**（`gl_FragDepth`） |
| **B. Shader** | Lit pass 读 map | `MaterialSceneShadows.glslinc` / `Phong.frag` | 各光源不同 `bias` 公式 | 读侧 bias 最大 |

**VK raster bias 应已生效：** PSO 静态 `depthBiasEnable` + slope/constant（无 `vkCmdSetDepthBias`）；Dir/Spot 用固定管线深度。提高 VK constant/slope **无目视改善** → 更像 **错误三角面写入**，而非 bias 未接。

详见 playbook §4.6。

---

## 6) 主因假设（当前排序）

1. **P3 / winding** — scheme A（shadow 不 flip + Front + CCW）在 VK 上与 GL 剔不同面；Back cull 只修水平面支持此论。
2. **写路径** — 接收体深度进 map；Debug 5/6 二分。
3. **读路径 / shader bias** — 第二层；Point 正常不能证明 raster bias 坏了。

**已降级：** P5（多 cascade）、P1（`gl_FragDepth` 为主因）、全局「VK depthBias 未开」。

---

## 7) 建议下一步（接手的 Agent）

1. **恢复 TEMP**（若不再做 E3 对照）：`MAX_CASCADES=4`，`DIR_SHADOW_FORCE_CASCADE=-1`。
2. **Debug 5/6**（`DIR_SHADOW_DEBUG_MODE`）— 接收体是否在 map；depth delta ≈ 0？
3. **RenderDoc** — shadow draw 的 cull / frontFace / depthBias PSO 字段。
4. **修复 A/B（需用户批准代码）：**
   - **Scheme B：** `kVulkanShadowPass.ViewportFlipY=true` + `GetEffectiveCullMode()` 翻 Back
   - **P3b：** shadow PSO `VK_FRONT_FACE_CLOCKWISE` + Front
5. 勿永久保留 VK Back / GL Front 分裂（E2 已证非闭合）。

---

## 8) 关键代码入口

| 区域 | 路径 |
|------|------|
| Shadow cull + raster bias caps | `RHI/RHIClipSpaceCapabilities.cpp` |
| ShadowPass PSO | `RenderPasses/ShadowPass.cpp` |
| VK PSO rasterizer | `Vulkan/VulkanRHIResources.cpp` (~1941) |
| GL polygon offset | `OpenGL/OpenGLRHI.cpp` (~258) |
| Shadow 写深度 | `Shaders/ShadowPass.frag` |
| Shadow 读 + bias | `Assets/.../MaterialSceneShadows.glslinc` |
| Dir CSM 矩阵（相机耦合） | `ForwardRenderer::BuildDirectionalShadowDrawCommands` |

---

## 9) 验证命令

```powershell
minEngine\bin\Editor.exe --rhi vulkan --project ..\MyMEProject\MyMEProject.meproject
minEngine\bin\Editor.exe --renderer manual --rhi vulkan --project ..\MyMEProject\MyMEProject.meproject
minEngine\bin\Editor.exe --rhi opengl --project ..\MyMEProject\MyMEProject.meproject
.\scripts\verify.ps1
```

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-31 | 初版：E0–E3、Dir+Spot、depth bias 审计、handoff |
