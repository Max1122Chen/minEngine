# ASSET-F03 — Create Asset 身份契约（单对象 · 同步 meta · 禁 Load 换身）— Design Spec

## Meta
- **ID:** `ASSET-F03`
- **Type:** Refactor（资产创建路径契约；含 Feature 级 API 收敛）
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-15
- **Branch:** 建议 `feat/asset-create`（或先在 `feat/prefab` 落地 S01 再抽分支）
- **Related:**
  - [FEATURE_REGISTRY](../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../ACTIVE_WORK.md)
  - [ASSET-F01](./ASSET-F01_IMPORT_PIPELINE_DESIGN.md) / [ASSET-F02](./ASSET-F02_IMPORT_SERVICE_DESIGN.md)（Import/Load 注册；**本 Feature 管 Create/Register 身份**）
  - [CORE-F23 Amendment B](../Platform/Core/CORE-F23_PREFAB_ASSET_INSTANTIATE_DESIGN.md)（Prefab Create→实例 Guid；**Guid 根因并入本 Feature**）
  - [BUG-ASSET-001](../bugs/BUG-ASSET-001.md)（全盘 ScanAssets；**Out**，勿绑）
- **Depends on:** 现有 `AssetManager` / `AssetMeta` / Loaders；`ObjectManager::RemapObjectGuid`（仅兜底）
- **Blocks:** 干净的 Hierarchy Create Prefab → Propagate；任何「Create 后立刻把资产 Guid 写入外部表」的调用方

## TL;DR

**问题：** 当前 `CreateAsset<T>` 是反模式：`NewObject(临时 Guid A)` → 写盘 → `RegisterAsset` 另造 meta Guid **B** → `LoadAsset` **再 New 一份**带 B 的对象。调用方若在 Load 前把 A 写进别的系统（Prefab 实例表），身份永久失配。  
**方案：** Create 只保留 **一个** 内存对象；**Guid 只生成一次**；写盘前就有权威 meta（或与对象同 Guid 的 pending meta）；`RegisterAsset` **不得**在「首次登记」时无故换 Guid；**禁止**用「再反序列化换对象」来完成身份对齐。  
**当前：** **Done**（S00–S02：Register preferred Guid、Create 单对象、Prefab Save 对齐）。

## Scope

### In

- 定义 **Create 身份契约**（§3.2）并落到 `AssetManager`
- 统一 / 收敛 `CreateAsset<Scene|Material|Prefab|AnimationGraph|…>`：单对象返回，无 Create 后 Load 换身
- `RegisterAsset`（及等价首次登记）：支持 **preferred Guid** = 内存对象已有 Guid；已有 meta 则对象对齐 meta（Load 路径）
- `PrefabUtility::SavePrefabAsset` / Hierarchy Create Prefab **消费**该契约（消掉 CORE-F23 Amendment B 的 Guid 根因）
- 单测：Create 后 `object.GetGuid() == meta.Guid`；磁盘根 `m_Guid` 一致；Prefab Create→Propagate 命中

### Out

| 项 | 说明 |
|----|------|
| Import 管线大改 | 仍属 F01/F02；Import 已有「先定 Guid 再写」先例，仅对齐契约叙述 |
| Watcher / 全盘 ScanAssets | **BUG-ASSET-001** |
| Async load / 资产流式 | Capability Roadmap 另项 |
| 改 Load 反序列化算法本身 | Load 仍可 New+FromFile；只禁 **Create 路径**用 Load 修身份 |
| Prefab Stage 相机 / 灯 | **ED-F16 Amendment B** |
| 全盘迁移历史坏 Guid 的未打开 Scene | Out（可后置工具） |

## Reader quick start

1. §0 Pre-flight · §3 目标契约与数据流  
2. §4 与 F23-B / Import 关系 · §7 切片  
3. 代码：`AssetManager::CreateAsset*`、`RegisterAsset`、`PrefabUtility::SavePrefabAsset`、各 `*Loader`

---

## 0) Pre-flight

| 项 | 判断 |
|----|------|
| **Prerequisites** | AssetMeta / RegisterAsset / CreateAsset 特化 **sound**；Prefab 实例表依赖资产 Guid **sound**（暴露了缺口） |
| **Tech-debt if now** | **Medium** — 动 Create/Register 影响多资产；应用切片 + 单测锁契约 |
| **WIP** | `feat/prefab` Amendment B Review；本 Feature **吸收 F23-B 的 Guid 部分**，避免两套修法 |
| **Philosophy** | Mechanism：资产身份是平台机制；一次 Guid、一对象 — Prefer Simplicity；禁「Load 换身」旁路 |
| **True refactor?** | **是** — 改契约并删除 Create→Load 换身路径，不是再加一层 Guid remap 胶水长期并存 |
| **Recommendation** | **Go with scope cut**：只做 Create/Register 身份；Import/Scan 不动。Prefab Guid 以本 Feature S01/S02 修复，F23-B 改为「依赖 ASSET-F03」 |

---

## 1) 背景与目标

### 1.1 Pain

1. **双 Guid：** 内存 A vs meta B，磁盘可能仍写着 A。  
2. **双对象：** Create 末尾 `LoadAsset` 再造一份，原对象丢弃 —— 身份靠「碰巧没人引用 A」成立。  
3. **Prefab 爆雷：** Create Prefab **必须**在 Save 前把 Guid 写入 `PrefabInstanceRecord`，正好踩中「引用了临时 A」。  
4. **心智负担：** 调用方不知道该信 `object.GetGuid()` 还是 `meta.Guid`。

### 1.2 成功标准

- Create 成功后：**同一** `shared_ptr` 对象的 Guid == `.meta` Guid == 磁盘资产根 `m_Guid`。  
- 无 Create 路径上的「为对齐身份而 Load」。  
- Prefab：Create+Save 后实例表 Guid 与资产 Guid 一致，Propagate 命中。

---

## 2) 现状

### 2.1 `CreateAsset<T>`（Scene / Material / Prefab / AnimationGraph）

```text
NewObject(GenerateGUID() → A)
  → Write*AssetFile(object)     // 盘上常为 A
  → RegisterAsset(path)         // 无 meta → meta.Guid = GenerateGUID() → B
  → return LoadAsset(path)      // NewObject(..., B) + Deserialize
```

调用方拿到 B 的对象；A 对象析构。对「无外部表」资产表面正常。

### 2.2 Hierarchy Create Prefab

```text
CreatePrefabFromGameObject → Prefab(A) + PrefabInstanceRecord(A)
  → SavePrefabAsset → 写盘(A) + RegisterAsset → meta(B)
  → 仍持有 Prefab(A)          // 无 Load 换身
```

→ Propagate 用 Stage 加载的 Prefab(B) 对记录(A) → 失配。

### 2.3 较好先例（应对齐）

AnimationClip Import：`pendingMeta.Guid` 先定 → `importedClip.SetGuid` → 写盘 → Register（读已有 meta）。**一 Guid、一对象**。

---

## 3) 方案

### 3.1 目标数据流（Create）

```text
GenerateGUID() → G（只一次）
  → NewObject(..., G)  或  NewObject 后立刻 SetGuid(G)
  → Build AssetMeta { Guid=G, Path, Type, Name }
  → Write asset file（根 m_Guid = G）
  → Write/Update .meta（Guid = G）
  → Register into registry（采用 G；禁止再 Generate）
  → 可选：放入 LoadedAssetCache
  → return 同一 shared_ptr
```

**不出现：** Create 末尾为身份而 `LoadAsset`。

### 3.2 身份契约（硬不变量）

| ID | 不变量 |
|----|--------|
| C1 | 任意成功 Create 返回的对象：`object.GetGuid() == FindAssetMetaByPath(path)->Guid` |
| C2 | 同路径磁盘资产根 `m_Guid` 与 meta Guid 一致（Create/Save 成功后） |
| C3 | 首次登记不得静默更换调用方已持有对象的 Guid，除非显式 `Remap` API 并更新所有已知引用表 |
| C4 | Load 路径：以 **meta Guid** 为准构造/对齐对象（可 `NewObject(..., meta.Guid)` 或 `ApplyMetaIdentity`） |
| C5 | Create 路径 **禁止** 依赖「再反序列化」完成 C1/C2 |

### 3.3 `RegisterAsset` 行为调整

| 情况 | 行为 |
|------|------|
| 路径尚无 meta，调用方提供 **preferredGuid**（非零） | meta.Guid = preferredGuid；写 `.meta`；登记 |
| 路径尚无 meta，未提供 preferred | 允许 `GenerateGUID()`（仅「无对象的登记」遗留路径）；**Create 不得走此分支** |
| 路径已有 meta | 采用已有 Guid；**不**覆盖为新 Guid |
| Create 已持有对象且 Guid ≠ meta.Guid | **失败**或显式 Remap 辅助（Create 主路径应避免进入） |

建议 API 形态（示意，实现可微调）：

```cpp
AssetMeta RegisterAsset(const std::string& path,
                        const std::string& assetTypeId,
                        const GUID* preferredGuid = nullptr);

// Create 内部：
AssetMeta meta = RegisterAsset(relPath, typeId, &object->GetGuid());
```

或拆分：`RegisterNewAssetWithGuid(path, typeId, guid)` / `EnsureRegistered(path, typeId)`。

### 3.4 `CreateAsset<T>` 目标实现骨架

```text
allocate unique path
NewObject(name, nullptr, G)   // G = GenerateGUID() once
fill default content
WriteAssetFile(path, *object) // must serialize m_Guid = G
meta = RegisterAsset(path, typeId, &G)
cache object if applicable
return object                 // SAME pointer
```

删除：Create 末尾的 `LoadAsset<T>(meta.AssetPath)`。

### 3.5 Prefab / Save 路径

- `PrefabUtility::SavePrefabAsset`：首次登记时 `preferredGuid = prefab.GetGuid()`。  
- `CreatePrefabFromGameObject` 仍可先写实例表（Guid=A），Save 后 A 即权威资产 Guid —— **不再变成 B**。  
- 若历史项目已 A/B 分裂：提供一次 Save 时的修复（Remap 对象 + 打开 Scene 的 `PrefabAssetGuid` + 重写文件）；不强制扫全盘。

### 3.6 删除列表（true refactor）

| 删除 / 禁止 | 说明 |
|-------------|------|
| Create 成功路径上的 `return LoadAsset<…>` | 各 `CreateAsset` 特化 |
| 首次 Register 对 Create 对象「无视已有 Guid 再 Generate」 | `RegisterAsset` 无 preferred 时 Create 不得调用 |
| 长期「Propagate 用路径模糊匹配」兜底 | **拒绝**（掩盖契约破坏） |
| F23-B 仅作「Save 后 Remap B→回写」的永久方案 | 可作迁移切片，最终以 C1–C5 为准 |

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| **A. Guid 一次生成；meta 与对象同步；Create 不 Load** | 契约清晰；Prefab 自然正确 | 要改 Register/Create | **选用** |
| B. 保持 Register 造 B，Create 后 Remap 对象+所有引用表 | 少动 Register | 引用表易漏；仍双 Guid 窗口 | 仅作迁移兜底 |
| C. Create 后 Load 换身，并规定「禁止在 Load 前引用 Guid」 | 少改 AM | Prefab 做不到；反模式固化 | **拒绝** |
| D. 资产 Guid 与 Object Guid 分离（第二套 id） | — | 双轨身份 | **拒绝** |

---

## 5) 与周边 Feature 关系

| Feature | 关系 |
|---------|------|
| **CORE-F23 Amendment B** | Guid 根因 **升级并入本 Feature**；F23-B 文档改为「依赖 ASSET-F03；Prefab 侧只保留实例表/Propagate 验收」 |
| **ED-F16 Amendment B** | 无关（相机/灯）；可并行 |
| **ASSET-F02** | Load/Import 注册表保留；Create 身份是正交缺口 |
| **BUG-ASSET-001** | 仍独立；Create 写盘仍可能触发 watcher，不在本期修 |

---

## 6) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| 改 Register 影响 Import/Scan | 回归 | preferred 仅可选参数；已有 meta 行为不变；单测 + 手验 Import |
| 磁盘旧文件 m_Guid ≠ meta | 混淆 | Load 以 meta 为准（C4）；下次 Save 写齐 |
| Prefab 已坏的打开 Scene | Propagate 仍失败 | S02 打开 Save 修复；文档说明 |
| 范围膨胀到 Async/Watcher | 拖死 | Out 表硬切 |

---

## 7) 验收标准

- [ ] `CreateAsset<Scene/Material/Prefab/AnimationGraph>`：返回对象 Guid == meta Guid；**无** Create 末尾 Load 换身
- [ ] Hierarchy Create Prefab：`PrefabInstanceRecord.PrefabAssetGuid == AssetMeta.Guid`；Stage 改 default → Save → 源树更新
- [ ] `test` 相关资产 / prefab / prefab-overrides 绿；新增 Create 身份单测
- [ ] 文档：本 Design Done；F23-B 标注由 ASSET-F03 消化；FEATURE_REGISTRY / ACTIVE_WORK 更新

---

## 8) 建议切片

| Slice | 内容 | 验证 |
|-------|------|------|
| **S00** | 契约落地：`RegisterAsset` preferred Guid + 单测 | 单测 |
| **S01** | 改 `CreateAsset<*>`：单对象、同步 meta、去 Load 换身 | 单测 + 手验 CB Create |
| **S02** | `SavePrefabAsset` / Create Prefab 消费契约；Propagate 手验 | `test prefab*` + 手工 CastShadows |
| **S03** | 历史 A/B 轻量修复（打开资产 Save 时） | 手工坏项目 |
| **S04** | DoD / 文档 / 删旧注释路径 | — |

**实现顺序建议：** S00→S01→S02（先解锁 Prefab）→S03→S04。

---

## 9) 开放点（设计默认）

| # | 问题 | 默认 |
|---|------|------|
| O1 | 权威 Guid 谁先生成 | **对象与 meta 同一时刻使用同一 G**（Create 内一次 `GenerateGUID`） |
| O2 | Create 是否写 LoadedAssetCache | **是**（返回即缓存，避免紧接着 Load 两份） |
| O3 | F23-B 是否保留独立实现 | **否** — Guid 并入 ASSET-F03；F23 只验收 Prefab 行为 |
| O4 | Import 是否强制改同一 API | **本期叙述对齐**；代码已合规的 Import 可不改 |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-15 | **Review：** 登记 ASSET-F03；Create 单对象 + 同步 meta；禁 Load 换身；吸收 F23-B Guid 根因 |
| 2026-09-15 | **Done：** RegisterAsset preferred Guid；CreateAsset 单对象+Cache；SavePrefabAsset 先写 meta；	est asset-manager/prefab/prefab-overrides PASS |
