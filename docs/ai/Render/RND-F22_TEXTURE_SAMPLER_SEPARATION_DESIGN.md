# RND-F22 — Texture / Sampler 分离升级（RHI → Material）

## Meta
- **ID:** RND-F22
- **Type:** Feature / Refactor
- **Complexity:** L3
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-22
- **Related:**
  - [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
  - 哲学：[ENGINE_DESIGN_PHILOSOPHY](../ENGINE_DESIGN_PHILOSOPHY.md)
  - 正交：[SPR-F07](../Sprite/SPR-F07_ATLAS_DILATE_BORDER_DESIGN.md)
  - 触媒：minCraft 反哺；消费面 = Material 图可编 Sampler
  - 对标（职责）：UE FRHISamplerState；Godot RD::SamplerState + sampler_create
  - 代码锚点：`RHIShaderBinding` · `Material::CommitCompileResult` / `MaterialTextureParameter` · `MaterialRenderProxy::RebuildShaderBindingSet` · `Configure2DTextureSampling` · `VulkanRHI::m_DefaultSampler` · `MaterialGraphNodeDef_TextureSample`

## TL;DR

**问题：** Texture/Sampler 纠缠；材质图无法选 Nearest；游戏仓正式路径消费不了。

**方案：** `RHISamplerDesc` + **`GetOrCreateSampler` 缓存**；Binding `(SRV, Sampler)`；GL Sampler Object；`Texture2D.Preferred`；**TextureSample 可编 + 显式 Inherit/Override**；编译期把每 slot 的 SamplerDesc 写入 **`MaterialTextureParameter`**，Proxy 只读表填 Binding（不扫图）。

**验收：** Editor 将 TextureSample 设为 Override+Nearest 后 GL/VK Nearest；同 Desc 指针相同；旧材质 Inherit → 行为同今日 Linear。

**状态：** Done — RHI Sampler cache + Binding + TextureSample Override + Proxy Resolve；smoke PASS（bin cwd）。

---

## Problem

### 产品
- 像素风需要 Nearest；今日恒 Linear。
- 仅 C++ Preferred 不够：minCraft 走 Material 图，TextureSample 必须可配、可存盘、可在 Inspector 编辑。

### 架构
| 层 | 病状 |
|----|------|
| OpenGL | 采样态烤进 texture object |
| Vulkan | 全局 m_DefaultSampler |
| Binding | 无 Sampler 字段 |
| Material | TextureSample 无属性；Proxy 只绑 SRV；`MaterialTextureParameter` 无采样意图 |
| 资产 | Texture2D Filtering 死字段 |

---

## Requirements

### RHI / 绑定
- R1：`RHISampler` + `RHISamplerDesc`，与 `RHITexture` 独立。
- R2：`GetOrCreateSampler(desc)` **缓存复用**；同 Desc 同实例；随设备销毁。
- R3：`RHIShaderBinding` 携带 `Sampler*`（null = DefaultLinearRepeat）。
- R4：OpenGL 用 `glBindSampler` 为采样真源；作者意图不烤进 tex；apply 时若 Sampler null 必须 `glBindSampler(unit, 0)` 清残留。
- R5：凡 `CreateShaderBindingSet` 的纹理槽（Material set2、Scene set1 阴影/IBL、其它）都必须填 Sampler（默认或强制），禁止「VK 偷偷 Default、GL 靠烤态」。

### 资产
- R6：`Texture2D` 持有 PreferredSamplerDesc（API/创建可设）；不是「字段==GL 状态」。

### Material（消费面硬需求）
- R7：`TextureSample` 增加可序列化属性：
  - `bOverrideSampler`（bool，默认 **false** = Inherit）
  - 当 Override：`MinFilter` / `MagFilter` / `AddressU` / `AddressV`（MVP：Nearest|Linear；Wrap 现有枚举）
- R8：Inspector 可编辑；改后脏编译 / 重建 Proxy（对齐 ScalarParameter）。
- R9：**编译期**收集 slot→SamplerDesc，写入 `MaterialTextureParameter`（与今日从 TextureObject 填纹理默认值同阶段：`CommitCompileResult`）。
- R10：Resolve（Proxy 绑定时）：
  1. 若引擎强制（depth/shadow compare）→ 强制 Sampler
  2. 否则若该 slot 编译结果为 Override → `GetOrCreateSampler(compiledDesc)`
  3. 否则若绑定 Texture 有 Preferred → Preferred
  4. 否则 DefaultLinearRepeat
- R11：同 slot 多个 TextureSample：
  - 若 ≥1 个 Override 且 Desc 不一致 → **Warn + first Override wins**（图遍历顺序与 Compile 收集 TextureObject 一致）
  - 全 Inherit → 走 Preferred/Default
- R12：旧图无新字段 → 反序列化默认 Inherit → 零回归。

### 文档
- R13：像素风：TextureSample 勾选 Override + Nearest；与 Preferred / SPR-F07 Dilate 一句互链。

---

## Constraints
- C1：双后端同期。
- C2：默认零回归。
- C3：无 `.mesampler` 资产。
- C4：不拆 GLSL texture2D + sampler。
- C5：无 minCraft 特例路径。
- C6：禁止 GL 烤态与 Sampler Object 双真源收尾后残留。
- C7：MVP 不要求 MaterialInstance 运行时改 Sampler（改纹理仍用编译期 slot Sampler）。

---

## Preferences
- P1：Desc cache（确认）。
- P2：作者面在 TextureSample + **显式 Inherit/Override**（不用「全默认=Inherit」隐式规则）。
- P3：Sampler 元数据进 `MaterialTextureParameter`，Proxy 不扫 Graph。
- P4：Shader 仍 sampler2D。
- P5：TextureObject 本期不挂 Sampler 属性。

---

## Assumptions
- A1：GL Sampler Object 可用。
- A2：一 TextureSlot ↔ 一 combined binding ↔ 一 Sampler。
- A3：节点 Inspector 能显示新 ME_PROPERTY；否则本 Feature 补最小编辑。
- A4：`MaterialTextureParameter` / `CommitCompileResult` 是正确挂载点（已有 TextureObject→参数表路径）。

---

## Non-Goals
- `.mesampler` 资产 / Sampler 连线节点
- GLSL 拆 texture + sampler
- 各向异性 / LOD bias 全集 / 作者面 compare-sampler
- Instance 动态 Sampler 覆盖 API
- 修改 minCraft 仓

---

## Current Architecture

```
CreateFromPixels -> RHITexture (no sampler)
  GL: Configure2DTextureSampling bakes Linear
  VK: image only; bind uses m_DefaultSampler
Material::CommitCompileResult
  -> MaterialTextureParameter { slot, name, Texture2D Value }
MaterialRenderProxy::RebuildShaderBindingSet
  -> Binding { TextureSRV } only
```

---

## Proposed Architecture

### 核心命题
> Texture = 图像；Sampler = Desc 可缓存对象；在 Binding 组合。  
> 材质作者政策在 TextureSample；**生效副本**在编译后的 `MaterialTextureParameter`。

### Desc cache
- `RHISamplerRef RHI::GetOrCreateSampler(const RHISamplerDesc&)`
- 状态空间小；多节点共享同一 NearestClamp 实例。
- 内置：LinearRepeat（Default）、NearestRepeat、NearestClamp、LinearClamp。

### RHISamplerDesc（MVP）
```
MinFilter, MagFilter  in { Nearest, Linear }  // 见 Mip 映射
AddressU, AddressV, AddressW
```
- TextureSample 只编 U/V；**AddressW = AddressV**（Cube/Array 绑定时）。
- **Mip 映射（锁定）：** 若底层纹理 `NumMips > 1`（或创建时 GenerateMips），则：
  - Mag 仍用 MagFilter
  - Min：Linear → 后端 Min = LinearMipmapLinear；Nearest → NearestMipmapNearest
  - 若 `NumMips == 1`：Min/Mag 按字面 Nearest/Linear
- `RHICmdGenerateMips` **禁止**再改 GL texture min filter 作为真源；只生成 mip 链。

### Binding
```
RHIShaderBinding:
  Type, Buffer..., TextureSRV*, Sampler*  // null => DefaultLinearRepeat
```
OpenGL apply：`glBindTexture` + `glBindSampler(unit, samplerId or 0)`。  
Vulkan write：`imageInfo.sampler = resolved VkSampler`。

### Material 编译挂载（锁定，对齐全仓）

在 `Material::CommitCompileResult` 填充 `MaterialTextureParameter` 时：

1. 照旧：从 `TextureObject` 取 ParameterName / DefaultTexture。
2. **新增：** 扫描连接该 slot 的 `TextureSample` 节点：
   - 解析 input0 → TextureObject → TextureSlotIndex
   - 应用 R11 冲突规则，得到 `bOverrideSampler` + Desc
3. 写入例如：
```
struct MaterialTextureParameter {
  int SlotIndex;
  string ParameterName, ShaderSymbolName;
  shared_ptr<Texture2D> Value;
  bool bOverrideSampler = false;
  RHISamplerDesc SamplerDesc{}; // meaningful iff bOverrideSampler
};
```

### Proxy
`RebuildShaderBindingSet`：对每个纹理 entry  
`sampler = Resolve(forced?, param.bOverrideSampler, param.SamplerDesc, texture->Preferred, Default)`  
`resources[i].Sampler = sampler.get()`（保持 cache 存活：Proxy 可持有 `vector<RHISamplerRef>` 与 SRV 并列）。

### TextureSample 节点属性（锁定）
```
ME_PROPERTY() bool bOverrideSampler = false;
ME_PROPERTY() TextureFiltering MinFilter = Linear;
ME_PROPERTY() TextureFiltering MagFilter = Linear;
ME_PROPERTY() TextureWrapping AddressU = Repeat;
ME_PROPERTY() TextureWrapping AddressV = Repeat;
```
- **禁止**用「四字段全默认」暗示 Inherit（会与「显式要 Linear 盖掉 Preferred=Nearest」冲突）。
- Inherit：`bOverrideSampler == false`，忽略 Filter/Wrap 字段值（UI 可灰显）。

### Texture2D Preferred
- Sprite / 无图路径 / Inherit 材质槽使用。
- 磁盘 meta 序列化 Preferred：**本期可选**；节点 Override 已够 minCraft。

### 非 Material 绑定（R5）
| 路径 | Sampler |
|------|---------|
| Material set2 | Resolve 如上 |
| Scene set1 阴影 depth | 强制 Nearest + ClampToBorder（或今日等价） |
| IBL / 环境 | Default LinearClamp（或保持今日视觉的 Desc） |
| Editor ImGui | DefaultLinearRepeat 可接受 |

### Shadow / depth
强制 Sampler；不读节点。

---

## Responsibilities

| Unit | Responsible for | Not for |
|------|-----------------|--------|
| RHISamplerCache | Desc 复用 | 图像 |
| RHITexture | 图像 / mip 链 | 过滤真源 |
| TextureSample | 作者 Override/Inherit + Desc | 持有 RHI 句柄 |
| Material::CommitCompileResult | slot→MaterialTextureParameter（含 Sampler 意图） | 每帧扫图 |
| MaterialRenderProxy | Resolve + 填 Binding.Sampler | Pack / 图编辑 |
| Texture2D | Preferred | 盖 Override |

---

## Boundaries
- Allowed：RHI Sampler+cache；Binding；TextureSample；Compile 参数表；Proxy；全纹理 Binding 填 Sampler；GL Sampler Object。
- Forbidden：仅 C++、无图编辑；Proxy 为唯一收集点且与 Compile 双轨；只修单后端。

---

## Dependencies
- Soft：SPR-F07、RND-F23。
- 硬：现有 Material 编译 / Proxy / Modern RHI。

---

## Ownership & Lifetime
- Sampler：RHI cache；Proxy 可额外 shared_ptr 钉住当前 Binding 用到的实例。
- 节点：POD 属性；编译结果在 Material 参数表。

---

## Data Flow

```
TextureSample (Override?) -> CommitCompileResult -> MaterialTextureParameter
Texture2D.Preferred ---------------------> Resolve (if Inherit)
Engine forced ---------------------------> Resolve
Resolve -> GetOrCreateSampler -> Binding.Sampler -> GL/VK
```

## State & Invariants
- I1：采样真源在 Sampler/绑定。
- I2：同 Desc → 同 cache 实例。
- I3：Inherit + 无 Preferred → LinearRepeat。
- I4：Override 冲突 → Warn + first。
- I5：Apply 路径不得留下「上一 draw 的 sampler 绑在 unit 上」的脏状态（null → bind 0）。

---

## API / Interface Semantics

### GetOrCreateSampler
- 查找或创建；同线程约定同其它 RHI create。

### TextureSample
- 序列化四 Filter/Wrap + bOverrideSampler；脏 → CompileGeneration++ / InvalidateParameterBindings。

### MaterialTextureParameter
- 运行时真源（除强制与 Preferred 回退外）。

### 不做（MVP）
- Instance::SetSamplerOverride。

---

## Failure Model
- 非法 Desc：映射 + Warn。
- Override 冲突：Warn + first。
- 无 Sampler Object：硬失败。
- TextureSample 未连 TextureObject：该 Sample 不参与 slot 收集（与今日采样失败/未用 slot 行为一致）。

---

## Alternatives & Trade-offs

### Option A（recommended）— cache + Sample Override + Compile 写入 MaterialTextureParameter
- Pros：对齐现有 Compile 管道；Proxy 简单；minCraft 可消费；Inherit 明确。
- Cons：Compile 需扫 Sample 边。

### Option B — Proxy 每帧/每次重建扫 Graph
- Why not：与 Scalar/Texture 参数表模式不一致；易漏脏。

### Option C — 隐式「全默认=Inherit」
- Why not：无法显式 Linear 覆盖 Preferred=Nearest（review D02）。

### Option D — 仅 C++ Preferred
- 否决：无消费面。

---

## Integration Impact

| 模块 | 影响 |
|------|------|
| RHISampler + GL/VK | 新建/改 apply 与 descriptor write |
| RHIShaderBinding | +Sampler |
| Material.h/.cpp CommitCompileResult | +Sampler 字段与收集 |
| MaterialGraphNodeDef_TextureSample + gen | 新属性 |
| MaterialRenderProxy | Resolve + 钉住 SamplerRef |
| EngineSceneBindingSets 等 | 每纹理槽填 Sampler |
| Texture2DLoader | Preferred |
| OpenGL GenerateMips | 去掉改 tex filter 真源 |
| Tests | cache；Override Nearest；Inherit；冲突 Warn |

---

## Migration / Compatibility
- 旧材质：缺字段 → Inherit → Linear（+ Preferred 若有）。
- 行为变更仅当作者 Override。

---

## Verification Strategy
- Cache：同 Desc 指针相等。
- Override+Nearest：Binding.Sampler 为 Nearest（单测读 Desc 或 debug）。
- Inherit + Preferred=Nearest：Nearest；Inherit + 无 Preferred：Linear。
- 显式 Override Linear + Preferred Nearest：结果 Linear（回归隐式规则不会误伤）。
- 冲突：Warn + first。
- GL：连续两 draw 不同 sampler，第二 draw null/Default 时 unit 上无残留 Nearest。
- 回归：旧材质、sprite-atlas；GL+VK。
- 手验：Material Editor 改 TextureSample → 存盘 → 重开 → 像素风贴图。

---

## Open Questions（非阻塞）
1. Preferred 是否写入 Texture 资产 meta — Defer（节点 Override 已够）。
2. AddressW 是否单独暴露 — Defer（= AddressV）。
3. 冲突改为 compile error — Defer（保持 Warn+first）。

---

## Design Review

- **Mode:** A+B（多轮修订后 Pre-Implementation）
- **Verdict:** Ready with deferred items
- **Date:** 2026-09-22
- **摘要：** 吸收 D01 收集落点、D02 Inherit 显式化、D03 全 Binding 填 Sampler、D05 Mip 映射、D10 glBindSampler(0)；其余 Open 非阻塞。
- **Ready for implementation?** yes — 建议再写短 Implementation Plan（切片 S02–S06）后开工；大代码前 mentor Pre-flight。

---

## Suggested Slices

| Slice | 内容 |
|-------|------|
| S01 | 本 Design Review 收口（本文） |
| S02 | RHISamplerDesc + GetOrCreateSampler + GL/VK 对象 |
| S03 | Binding.Sampler；GL bindSampler(+0)；VK write；清烤态；GenerateMips 消毒；SceneSet1 填 Sampler |
| S04 | Texture2D Preferred + Loader |
| S05 | TextureSample 属性 + CommitCompileResult 收集 + Proxy Resolve + Inspector |
| S06 | 单测 + 双后端回归 + 短文档 |

---

## Philosophy checklist
1. 机制：可配 Sampler。
2. Prefer Simplicity：Desc cache，无 Sampler 资产。
3. Agent-Friendly：图属性可序列化。
4. 消费面闭环：Material 图可编。

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-22 | 初稿 Filtering（废弃） |
| 2026-09-22 | 升格分离；纳入 TextureSample；Desc cache |
| 2026-09-22 | **design-review 修订：** Compile→MaterialTextureParameter；bOverrideSampler；全 Binding 填 Sampler；Mip 映射；glBindSampler(0)；Status→Review |