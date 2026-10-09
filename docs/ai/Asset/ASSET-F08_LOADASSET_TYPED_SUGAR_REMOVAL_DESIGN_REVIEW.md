# ASSET-F08 — 移除 LoadAsset<T> — 设计评审

## Meta
- **Type:** Design Review
- **Status:** Done
- **Mode:** A（开工前）
- **Owner:** agent（自审）+ 待维护者审批
- **Last updated:** 2026-10-08
- **Design:** [ASSET-F08_LOADASSET_TYPED_SUGAR_REMOVAL_DESIGN.md](./ASSET-F08_LOADASSET_TYPED_SUGAR_REMOVAL_DESIGN.md)
- **Feature ID:** `ASSET-F08`
- **Skill:** `design-review`

---

## 1) 评审上下文

| 项 | 内容 |
|----|------|
| **被评审设计** | 删除 AM 上 `LoadAsset<T>`（默认含 `CreateAsset<T>`）糖衣并迁移调用点 |
| **模式** | A 开工前 |
| **焦点** | 合同清晰、为 F07 铺路、避免假分派 |
| **局限** | 自审；外仓调用面待实现期 grep |

## 2) 设计理解

- 真源：`ByPath` / `ByGUID` / `ByMeta` → Handler。
- 糖衣不参与分派，只 cast；应删除而非强化。
- F07 不得再以 `LoadAsset<DataAsset&gt;` 为推荐用法。

## 3) 发现项

### D01 — 与 F06 一致，属收尾而非反转

| 项 | 内容 |
|----|------|
| **种类** | 设计优点 |
| **严重度** | Informational |
| **说明** | F06 已把糖衣标成可选；F08 删除是正向收债 |

### D02 — CreateAsset<T> 默认 In 合理

| 项 | 内容 |
|----|------|
| **种类** | 待确认默认 |
| **严重度** | Low |
| **建议** | 一并删除，避免「Load 删了 Create 还留糖衣」 |
| **阻塞？** | 否（若改 Out 也可开工） |

### D03 — 外仓联动

| 项 | 内容 |
|----|------|
| **种类** | 风险 |
| **严重度** | Medium |
| **说明** | minCraft 等若调用模板需同步；S04 强制 grep |
| **阻塞设计？** | 否 |

### D04 — 不要引入新的 AM typed helper

| 项 | 内容 |
|----|------|
| **种类** | 约束重申 |
| **说明** | 禁止 `LoadAs&lt;T&gt;` 换皮重回 AM |

## 4) 待决

- Create 糖衣是否 In（默认是）。
- 其余可后置。

## 5) 就绪评估

**标签：** Ready after targeted clarifications  

**裁决：** **Ready with deferred items** — 确认 Create 默认后可开工；Deferred：反射 IsA 替换 cast、handbook 长文。

## 6) 建议下一步

1. 审批 F08（及 Create 默认）。
2. 开 `feat/asset-load-api`，S01–S04。
3. 合入后再开 F07 / `feat/asset-dataasset`。
