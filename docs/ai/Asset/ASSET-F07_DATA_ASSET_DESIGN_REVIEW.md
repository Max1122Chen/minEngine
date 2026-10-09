# ASSET-F07 DataAsset — 设计评审

## Meta
- **Type:** Design Review
- **Status:** Done
- **Mode:** A（开工前）
- **Owner:** agent（自审）+ 待维护者审批
- **Last updated:** 2026-10-08
- **Design:** [ASSET-F07_DATA_ASSET_DESIGN.md](./ASSET-F07_DATA_ASSET_DESIGN.md)
- **Feature ID:** `ASSET-F07`
- **Related:** ASSET-F06 Done；ED-F24 AES；外部 prompt（非权威）
- **Skill:** `design-review`

---

## 1) 评审上下文

| 项 | 内容 |
|----|------|
| **被评审设计** | 薄 DataAsset：在现有 AM 管线上可编辑的定义资产 |
| **评审模式** | A 开工前 |
| **Design Meta Status** | Done |
| **相关文档 / 代码** | Design Spec；ASSET-F06；ED-F24；AssetWorkflow Inspector（今日仅 meta + MIC 特例）；SpriteLoader Create 模式 |
| **已核对系统** | AssetManager handlers；AES 白名单；CB Inspector `DrawInspector`；`AssetSerializeHelper`；`AssetTypeRegistry` 扩展名映射 |
| **评审焦点** | 先抬引擎能力；薄 MVP；Import/Cook 后置 |
| **不在范围** | 实现代码审计；minCraft 迁移方案 |
| **局限** | 起草方自审；Open Questions 须维护者确认 |

---

## 2) 设计理解

### 目标
- 引擎 `DataAsset : Asset`，扩展名 `.medata`，经 F06 做 Create/Load/Save。
- 编辑器 Create（AES/CB/MCP）+ 反射编辑 + Save。
- 运行时 ByPath/ByMeta + `Cast<T>`（F08；勿恢复 `LoadAsset<T>`），生命周期与其它 Asset 同质。
- 文档 + 测试证明：游戏模块如何注册派生类型。

### 非目标
- 外部 Authoring Import/Cook、PrimaryDataAsset、独立 Schema DSL、pak、Async 大改、Core 内玩法类型。

### 主概念
- DataAsset 是 Asset 族成员，不是平行管线。
- Definition ≠ Instance。
- 本期 Schema = C++ Reflection。

### 职责
- Loader 管 IO；AES 管 Create 门禁；Inspector 管编辑 + Save UX；游戏管字段与 Registry bake。

### 依赖
- F06 / F03 / AES / Property 控件 — **就绪**。

### 关键工作流
- Create → 编辑 → Save → Load 往返；Tests 内 `SampleDataAsset` 派生样例。

### 关键契约
- 一个扩展名 → 一个 AssetTypeId（不做多态共用 `.medata`）。
- S03 Inspector 编辑为本期 **In**（不假设已有）。

### 关键不变量
- F03 Guid；Inspector 选中隔离；Save 失败须有明确错误提示。

---

## 3) 发现项

### D01 — CB Inspector 缺口正确升为 In

| 项 | 内容 |
|----|------|
| **种类** | 设计优点（已确认） |
| **严重度** | Informational |
| **置信度** | Confirmed |
| **证据** | 今日 AssetWorkflow Inspector 仅 meta 文本 + MaterialInstance 特例 |
| **问题** | 无 — 设计未假装「编辑已可用」 |
| **为何重要** | 砍掉 S03 则无法满足「编辑器可编辑」 |
| **阻塞实现？** | 否 |
| **开工前必须改？** | 否 — S03 保持不可砍 |

### D02 — Inspector 匹配策略仍待拍板

| 项 | 内容 |
|----|------|
| **种类** | 缺失决策 |
| **严重度** | Medium |
| **置信度** | Confirmed |
| **证据** | Design Open Question 1 |
| **问题** | 仅看 typeId vs `dynamic_pointer_cast<DataAsset>`，影响 S04 派生类型编辑 |
| **建议** | 采纳默认：基类看 typeId；派生以 cast 成功为准（更省事） |
| **阻塞实现？** | 若采纳默认则否 |
| **开工前必须改？** | 是 — 需维护者确认 |

### D03 — 游戏派生类型 vs AES 硬编码白名单

| 项 | 内容 |
|----|------|
| **种类** | 架构风险 |
| **严重度** | Low |
| **置信度** | Likely |
| **证据** | `IsCreatableAssetTypeId` 今日为静态数组 |
| **问题** | 游戏模块若要在 CB Create，须改 Editor 白名单 |
| **为何重要** | 长期扩展故事不完整 |
| **建议** | MVP：AES 仅白名单 `DataAsset`；派生 Create 走测试 / AM API；**Deferred**：creatable 动态注册 |
| **阻塞实现？** | 否 |
| **开工前必须改？** | Defer OK — Design 已记 Non-Goals |

### D04 — 属性编辑 Undo 深度未定

| 项 | 内容 |
|----|------|
| **种类** | 歧义 |
| **严重度** | Low |
| **置信度** | Confirmed |
| **证据** | Undo Deferred；假定复用 `PropertyValueWidget` |
| **问题** | Scene 属性命令可能需要 Asset 目标适配 |
| **建议** | S03 可用直接 setter + Save，不做 Undo |
| **阻塞实现？** | 否 |
| **开工前必须改？** | Defer OK |

### D05 — 相对外部 prompt 的范围纪律

| 项 | 内容 |
|----|------|
| **种类** | 可选改进 |
| **严重度** | Informational |
| **置信度** | Confirmed |
| **证据** | Design Non-Goals |
| **问题** | 无 — Out 清单清晰 |
| **建议** | 外部 prompt 仅作愿景；勿并入 F07 实现 |

---

## 4) 歧义与待决

### 阻塞（只需确认，不必重设计）
- 采纳 Open Questions 默认：Inspector cast 策略；Save 按钮；Undo Deferred；保留 `m_Notes`。

### 重要可后置
- 游戏类型 AES creatable 动态注册（D03）。
- Save 是否原子写（若 helper 非原子，仅文档记账）。

### 可选
- 另开 IMPLEMENTATION.md vs 设计内 Slices（L2 用设计内切片即可）。

---

## 5) 缺失工作流 / 边角

- Inspector 持有 `shared_ptr` 期间磁盘被改：接受过期，直到重选。
- `ProjectContentRoot` 为空时 Create：与其它 Create 失败行为一致。
- 序列化格式：继承 `AssetSerializeHelper`，无需新决策。

---

## 6) 验收与测试缺口

| 需求 / 行为 | 当前验证 | 缺口 |
|-------------|----------|------|
| Create/Load/Save + Guid | 计划 suite `data-asset` | 必须实现 |
| Inspector 编辑 + Save | 手验 + 单测 Reload 字段 | 必须实现 S03 |
| 派生 SampleDataAsset | S04 测试 | 必须实现 |
| AES Create | Automation / 手验 | S02 |

---

## 7) 值得保留的设计选择

- 扩展 Asset/AM，而非平行 DataPipeline。
- 正确把「真缺口」定在 Inspector，而非再造资产系统。
- Non-Goals 挡住外部 prompt 的 L3 膨胀。
- 从当前 `master` 开分支，避开过时 worktree tip。
- 哲学：Core 不进玩法类型。

---

## 8) 实现就绪评估

**标签：** 澄清后可开工（Ready after targeted clarifications）

**理由：** 架构与切片可执行。剩余主要是维护者对 Open Questions 的确认，以及一条 Deferred（游戏 creatable 动态注册）。无矛盾需求。仅当你否决 S03、或要求本期做 Import/Cook 时，才应标 Not ready。

---

## 9) 建议下一步

1. 维护者批准 Design + Open Questions 默认（或逐条修改）。
2. Design 已标 Planned / Ready with deferred；批准后从当前 `master` 开 `feat/asset-dataasset`。
3. 实现 S01 → S05；未批准前不大面积写代码。

---

## 10) 建议验收清单

- [ ] 经 AES / CB / `asset.create` 创建 `.medata`
- [ ] Inspector 能改 `m_Notes`（或样例字段），Save 后 Reload 仍在
- [ ] `LoadAsset<DataAsset>` 与 Sample 派生 roundtrip 测试通过
- [ ] Guid 稳定（F03）
- [ ] Core 无 Block/Item 等玩法类型
- [ ] 本 Feature 未启动 Import/Cook/Primary

---

## 裁决（自审）

**Ready with deferred items** — 维护者确认默认倾向后可开工。

**后置：** 游戏模块 AES creatable 动态注册；Inspector Undo；外部 Import 管线。
---

## 11) 修订附记（2026-10-08）

- 维护者决定：**先做 ASSET-F08**（删除 `LoadAsset<T>` 糖衣，默认含 `CreateAsset<T>`），再做本 Feature。
- Design 已改为推荐 `LoadAssetByPath` / `ByMeta`；F08 **Done**；实现已完成。
- 原裁决「Ready with deferred」对 **F07 架构**仍成立；**开工顺序**以 F08 合入为闸门。
