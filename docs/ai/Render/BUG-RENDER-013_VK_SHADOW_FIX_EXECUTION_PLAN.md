# BUG-RENDER-013 — VK Shadow Fix Execution Plan

## Meta

| 字段 | 值 |
|------|-----|
| **ID** | BUG-RENDER-013（修复轨）· 依托 RND-F13 ManualRenderer |
| **Type** | Implementation Plan |
| **Status** | ~~Draft → In Progress~~ → **Superseded** by [RND-F14](./RND-F14_SHADOW_PASS_UBO_LIFETIME_DESIGN.md) Phase A (2026-08-31) |
| **Owner** | project maintainer |
| **Last updated** | 2026-08-31 |
| **Related** | [BUG-RENDER-013](../bugs/BUG-RENDER-013.md) · [**RND-F14 Design（主方案，待审阅）**](./RND-F14_SHADOW_PASS_UBO_LIFETIME_DESIGN.md) · [VK_SHADOW_PICCOLO_MINENGINE_REFERENCE.md](./VK_SHADOW_PICCOLO_MINENGINE_REFERENCE.md) · [RND-F13](./RND-F13_MANUAL_RENDERER_IMPLEMENTATION.md) |
| **Supersession** | 切片顺序以 **RND-F14 Phase A** 为准；本文保留为历史对照 / S00–S08 细节清单 |

## TL;DR

**主因更新（2026-08-31）：** ShadowPass 对同一 host-visible `LightViewProj` / `ShadowParams` 做 `memcpy` 覆盖，与 VK deferred 录制冲突 → 见 [RND-F14](./RND-F14_SHADOW_PASS_UBO_LIFETIME_DESIGN.md)。  
旧顺序「S00→S01 矩阵单源」仍有用，但 S01 应实现为 **绑采样数组 offset / ring**，而非仅「两份数据抄一样」。  
**当前：** Design 审阅 → F14 Impl → Phase A1。

## Scope

### In

- `ManualRenderer` 诊断/fix 主战场；修通后回归 `ForwardRenderer`+RDG
- Dir-only 先收敛，再 spot/point/full-map
- VK RHI：depth layout、SRV `imageLayout`、barrier（必要时）
- `EngineSceneBindingSets`：invalidate、空槽、dirty 粒度
- ShadowPass / `BuildShadowDrawCommands`：矩阵与 UBO 一致性
- 文档与 `PROGRESS_LOG` 记录每 slice 结论

### Out（本计划内不做）

- CSM 分裂数学 / NDC-Z 大改（已排除为主因）
- RND-F12 RDG 语义大 refactor（降级为并行卫生）
- 全面迁移 Piccolo set0 布局或 MaterialCompiler 重绑
- 点光改 Piccolo 双抛物面 + GS（架构替换，另开 Feature）
- 生产默认 renderer 改为 Manual

---

## Reader quick start

1. [对照参考](./VK_SHADOW_PICCOLO_MINENGINE_REFERENCE.md) — 实验结论、BindingSet、Piccolo 差异  
2. 下表 **切片总览** — 顺序与 Gate  
3. **标准实验配置** — 所有 slice 默认从此起步，仅当 slice 说明允许才加变量  

### 标准实验配置（Baseline）

| 项 | Dir-only 阶段 | Full-map 阶段 |
|----|---------------|----------------|
| Renderer | `--renderer manual` | 同左 |
| RHI | `--rhi vulkan` | 同左 |
| Scene | `test` | 同左 |
| `MAX_SPOT_SHADOW_MAPS` / `MAX_POINT_SHADOW_MAPS` | `0` | `2` |
| `MAX_CASCADES` | `1`（S00–S03）；S04+ 可 `4` | `4` |
| `DIR_SHADOW_FORCE_CASCADE` | `0` | `-1` |
| 对比基线 | OpenGL 同场景；可选 Piccolo 同场景目视 | Manual ≈ Forward+RDG ≈ GL |

### 运行

```text
cd minEngine/bin
Editor.exe --renderer manual --rhi vulkan
```

### Shader 调试

`MaterialSceneShadows.glslinc` / inject：`DIR_SHADOW_DEBUG_MODE`  
- `5` — sampled depth  
- `6` — depth delta（current vs sampled）

---

## 1) 切片总览

| Slice | 内容 | 优先级 | Gate（进入条件） | 状态 |
|-------|------|--------|------------------|------|
| **S00** | 定界：RenderDoc + DEBUG_MODE | P0 | — | Planned |
| **S01** | 矩阵单源（shadow 画 = lit 采） | P1.1 | S00 有 map/采样结论 | Planned |
| **S02** | VK depth layout + SRV `imageLayout` | P1.2 | S01 后 dir-only 仍明显不对，或 S00 指向 layout | Planned |
| **S03** | Set1 invalidate + dummy 空槽 | P1.3–4 | S01 完成；full-map 前必做 | Planned |
| **S04** | Piccolo 式 R32 dir A/B（可选分支） | P2 | S01–S02 后 dir-only 仍不对 | Deferred |
| **S05** | Set1 dirty 拆分；BuildSceneSet1 时机 | P3 | Dir-only 通过后再开 spot/point | Deferred |
| **S06** | Full-map（spot/point/4 cascade） | — | S01–S03 Done | Planned |
| **S07** | Forward+RDG 回归 | — | S06 Manual 通过 | Planned |
| **S08** | 质量轨：bias / BUG-010 多影 | P4 | 与 S06 并行或之后 | Deferred |

状态：`Planned | In Progress | Done | Blocked | Deferred | Cancelled`

---

## 2) 切片详情

### S00 — 定界证据（P0，可零代码或仅 debug 常量）

**Goal：** 判定错误在 **shadow map 写入** 还是 **采样/descriptor/矩阵**。

**Steps**

1. Baseline 配置下抓 1 帧 RenderDoc（Manual + VK）。
2. 对 dir cascade 0（或唯一 cascade）：
   - Shadow pass 的 depth attachment / layer 是否有几何 depth？
   - Base pass 采样前 atlas layout 是否为 `DEPTH_STENCIL_READ_ONLY`？
3. 启用 `DIR_SHADOW_DEBUG_MODE=5/6`，目视 map 与 delta。
4. （可选）同场景 Piccolo 目视 dir shadow color。

**Touch：** 无代码或仅临时 `#define` / inject；记录截图路径于 slice note。

**DoD**

- [ ] 文档记录：**map 本体** 对/错（一句话 + RenderDoc 观察）
- [ ] 文档记录：采样前 **layout** 是否符合预期
- [ ] 分支决策写入下表：

| map 本体 | 画面/shader 采样 | 下一 slice |
|----------|------------------|------------|
| 错 | — | S02（写入/RHI）优先，S01 并行查矩阵 |
| 对 | 错 | **S01** 优先，其次 S03 |
| 对 | 对但浅/偏 | S08 + 微调 bias |

**Verify：** 上述 DoD 勾选 + 结论写入 [BUG-RENDER-013](../bugs/BUG-RENDER-013.md) 或 `PROGRESS_LOG.md`。

---

### S01 — 矩阵单源（P1.1，首选代码刀）

**Goal：** ShadowPass 绘制与 Base pass `DirLightViewProj[cascade]` **同一数据源、同一 cascade 槽**，消除 Piccolo 对比中最显眼的「双轨 UBO」差异。

**现状**

- Shadow 画：`m_LightViewProjUniformBuffer`，每 cascade `UpdateSubresource(offset=0)`
- Base 采：`m_DirLightViewProjUniformBuffer`，`BuildShadowDrawCommands` 写 array

**Target（择一，实现时选最小 diff）**

- **A（推荐）：** ShadowPass 绑定 `m_DirLightViewProjUniformBuffer`，每 cascade 更新 **对应 offset**（`sizeof(Matrix4) * cascadeIndex`），shader 仍单 mat4 或改为 dynamic offset / 单 cascade 指针。
- **B：** 保留单 buffer，但 `BuildShadowDrawCommands` 与 ShadowPass **只写一处**，Base 只读不写。

**Touch（预期）**

- `ShadowPass.cpp` / `.h`
- `ForwardRenderer.cpp`（`m_LightViewProjUniformBuffer` 与 shadow pass 接线）
- `EnginePipelineLayouts` / shadow shader binding（若改 buffer 或 offset）
- `ShadowPass.vert`（若 binding 名不变可不动）

**DoD**

- [ ] Dir-only、单 cascade：RenderDoc 或 DEBUG 6 显示 **projCoords.z vs sampled 系统性偏移减小**
- [ ] 同配置 VK 阴影 **轮廓/位置** 向 GL 靠拢（user 目视）
- [ ] OpenGL 回归：无退化
- [ ] 无新增 validation layer error

**Verify**

```text
Editor.exe --renderer manual --rhi vulkan   # MAX_*=0, MAX_CASCADES=1, FORCE_CASCADE=0
Editor.exe --renderer forward --rhi opengl  # 同场景对比
```

**Rollback：** 保留旧 buffer 路径直至 S01 验收通过。

---

### S02 — VK depth layout + SRV（P1.2）

**Goal：** 确保 shadow atlas 在 Base pass 采样前处于 **`DEPTH_STENCIL_READ_ONLY_OPTIMAL`**，且 `VkDescriptorImageInfo::imageLayout` 与 tracked layout 一致。

**检查/修改点**

- `VulkanRHI::RHICmdEndRenderPass` / `RHICmdTransition`
- `VulkanRHIShaderResourceView` / descriptor write path
- ManualRenderer：`ExecuteManualShadowPasses` 末尾 transition 是否多余/不足
- 多 cascade 往返：layer 0 only 测试是否仍错

**DoD**

- [ ] RenderDoc：最后一档 shadow pass 结束 → 第一个 base draw 之间，barrier 正确
- [ ] Dir-only 目视改善或 S00「map 对、采样错」路径关闭
- [ ] 不改变 OpenGL 行为（或 GL 路径无 diff）

**Verify：** 同 S01 + RenderDoc layout 截图。

**Gate：** 若 S01 已解决 dir-only，S02 可降为「只读验证 + 小修」。

---

### S03 — Set1 卫生：invalidate + dummy 槽（P1.3–4）

**Goal：** 消除 **latched SRV**、spot/point 禁用时的 **null descriptor** 风险；为 full-map 做准备。

**Tasks**

1. 审计 `InvalidateShadowTextureBindings` 调用点（Manual 纹理 recreate、RDG bind、toggle Cast Shadow）。
2. Spot/point 未启用时：set1 绑定 **稳定 dummy depth/cube**（尺寸 1×1 或引擎已有 dummy），禁止 nullptr SRV 进 descriptor。
3. 确认 `m_ShadowBindingGeneration` 在 handle/physicalIndex 变化时必 bump。

**Touch（预期）**

- `EngineSceneBindingSets.cpp`
- `ManualRenderer.cpp` / `ForwardRenderer.cpp`（invalidate 时机）
- 可选：`VulkanRHI` dummy texture 复用

**DoD**

- [ ] Toggle Point Cast Shadow：Dir 影 **不 latched**；关 point 后 moving point **不再** 影响 stale dir 影
- [ ] Validation：无 descriptor 警告
- [ ] Dir-only 无回归

**Verify：** full-map 配置下重复 BUG-013 复现步骤。

**Gate：** S06 前 **必须 Done**。

---

### S04 — Piccolo 式 R32 dir A/B（P2，条件执行）

**Goal：** **隔离**「depth texture 采样链」是否为 VK dir-only 剩余问题的主因。

**Scope（最小）**

- 仅 ManualRenderer + dir-only + 单 cascade
- Shadow pass：R32 color STORE + transient depth（Piccolo 同构）
- Base pass 临时 `sampler2D` 读 color（可 fork debug shader 或 `#ifdef`）
- **不**替换生产 spot/point/cube 路径

**Gate：** 仅当 S01–S02 后 dir-only **仍明显不对** 时启动；否则 **Cancelled**。

**DoD**

- [ ] A/B 结论写入 BUG-013：color 路径是否显著更接近 GL/Piccolo
- [ ] 若 A/B 证明 depth 链是主因：S02 加深或立项「生产 depth 链 hardening」；**不**默认全面改 color

**Verify：** 同 baseline + 与 S01 同场景对比。

---

### S05 — Set1 dirty 拆分（P3，条件执行）

**Goal：** 降低 Set1 大杂烩的 **失效 blast radius**；不强制物理拆 set。

**Tasks**

1. 分离 generation：`shadowSrvGeneration` / `iblGeneration` / `shadowUboGeneration`
2. 评估 `BuildSceneSet1` 移到 **shadow pass 之后**（仅当 SRV 不依赖 post-write 内容）
3. （可选）Set1a dir / Set1b spot·point — 仅当 S03 后 full-map 仍串槽

**Gate：** Dir-only 已通过 S01–S03；**S06 前至少完成 5.1**。

**DoD**

- [ ] IBL 变更不强制重建 shadow SRV
- [ ] Full-map toggle 行为符合 BUG-013 期望

---

### S06 — Full-map Manual 验收

**Goal：** Dir + Spot + Point + 4 cascade，Manual VK **功能正确**（允许质量轨后续优化）。

**配置：** 生产常量 `MAX_*=2`，`MAX_CASCADES=4`，`FORCE_CASCADE=-1`。

**DoD**

- [ ] Dir 影不随 point 开关 **异常耦合**
- [ ] Spot/point 影 **基本合理**（相对 GL）
- [ ] 无 crash / validation error
- [ ] 与 Forward+RDG **同错同对**（应一起变好）

**Verify：** BUG-RENDER-013 复现步骤反向验证 + GL 对比。

---

### S07 — Forward + RDG 回归

**Goal：** Manual 上 fix **继承**到默认 Forward 路径。

**DoD**

- [ ] `--renderer forward --rhi vulkan` full-map 与 Manual 一致
- [ ] Editor 默认路径目视通过
- [ ] `minEngineTests` / 相关 render tests 通过（若有）

---

### S08 — 质量轨（P4，并行或收尾）

**Goal：** BUG-010 浅影、多 cascade 重影、VK bias 与 Piccolo 差异。

**Tasks（非阻塞 013 关账）**

- VK `DepthBiasConstant` vs GL 对齐实验
- CSM 选择与 split 重叠（BUG-010）
- PCF 参数

**Gate：** S06 功能正确后再优先打磨。

---

## 3) 依赖顺序

```text
S00 定界
  ↓
S01 矩阵单源 ──────────────────────────┐
  ↓                                    │
S02 depth layout（可与 S01 验证并行）   │
  ↓                                    │
S03 set1 卫生                          │
  ↓                                    │
S06 full-map ←─────────────────────────┘
  ↓
S07 Forward+RDG 回归

S04 R32 A/B ──(仅 S01–S02 不足时)──→ 指导 S02 或 Cancel
S05 set1 dirty ──(S03 后、S06 前)──→ 支持 full-map
S08 质量轨 ──(与 S06 并行或之后)
```

---

## 4) 风险与决策点

| 风险 | 缓解 |
|------|------|
| 一次改太多无法归因 | 每 slice 独立 commit 或 PROGRESS 条目；严格 baseline 配置 |
| S01 改 shader binding 波及 GL | 双后端同路径；GL 必跑 |
| S04 color A/B 与生产分叉 | 特性开关/`#ifdef`；不合并到 main 直至决策 |
| full-map 仍炸 | 先完成 S03，再 S05，最后才考虑 spot/point 语义（cube vs Piccolo array） |

| 决策点 | 选项 | 建议 |
|--------|------|------|
| S04 是否做 | 做 R32 A/B / 跳过 | S01–S02 后 dir-only 仍错 → 做 |
| 生产是否改 color shadow | 是 / 否 | 默认 **否**；仅当 S04 强证明 depth 链不可信 |
| Set1 物理拆分 | 是 / 否 | S05 细粒度 dirty 不够再拆 |

---

## 5) 文档与 DoD（计划级）

本计划 **Done** 当：

- [ ] S00–S03、S06、S07 全部 Done
- [ ] BUG-RENDER-013 回归项勾选
- [ ] [VK_SHADOW_PICCOLO_MINENGINE_REFERENCE.md](./VK_SHADOW_PICCOLO_MINENGINE_REFERENCE.md) 变更记录更新
- [ ] RND-F13 S03 标记 Done 或移交维护

---

## 6) 变更记录

| 日期 | 说明 |
|------|------|
| 2026-08-31 | 初稿：P0–P4 优先级落为 S00–S08；Piccolo 对照 + Manual 主战场 |
