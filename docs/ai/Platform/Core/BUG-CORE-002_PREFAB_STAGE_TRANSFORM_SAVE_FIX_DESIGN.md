# BUG-CORE-002 — Prefab Stage Transform / Save Fix — Design Spec

## Meta
- **ID:** BUG-CORE-002（Fix Design）
- **Type:** Bug fix design
- **Status:** Done
- **Owner:** project maintainer
- **Last updated:** 2026-09-16
- **Branch:** `feat/prefab`
- **Related:**
  - [Bug Record](../../bugs/BUG-CORE-002.md)
  - [CORE-F23](./CORE-F23_PREFAB_ASSET_INSTANTIATE_DESIGN.md) · [CORE-F24](./CORE-F24_PREFAB_OVERRIDES_DESIGN.md) · [ED-F16](../../Editor/ED-F16_PREFAB_EDITOR_DESIGN.md)
  - [ASSET-F03](../../Asset/ASSET-F03_CREATE_ASSET_IDENTITY_DESIGN.md)（Guid 已 Done；本修不重复）
  - [CORE-F26](./CORE-F26_PREFAB_ASSET_DELETE_UNLINK_DESIGN.md)（删资产断链 — **Out**）
  - [BUG-ASSET-001](../../bugs/BUG-ASSET-001.md)（全盘 Scan — 正交，可加剧 Save 时序）

## TL;DR

**现状（修前）：** Stage `Instantiate` 用默认 identity 覆盖根 Transform；Create 世界 Scale 不稳；Stage 二次 Save 因 `EditCloneMap` 丢根映射失败。  
**已落地：** `bApplyWorldTransform`；Create 世界 bake + 外部 unresolved 清空；Clone/`RemapObjectGuid` 不再偷源 Guid 槽；WriteStageTree Scene 刷新 + 并行 rebuild + 回滚。  
**当前：** **Done** — `test prefab` / `test prefab-overrides` PASS。

## Scope

### In

- 修 `PrefabUtility::Instantiate` 对根 Transform 的覆盖策略（Stage / Level 语义分开）
- Create Prefab：模板根稳定反映源根 **世界** Transform（含 Scale）
- Stage Save：消灭 `EditCloneMap missing Prefab root mapping`；同会话可重复 Save
- 确认 / 加固根 Transform **不** Propagate（F24 O1）；非根 default 传播可手验
- 单测 / 手验清单

### Out

| 项 | 归属 |
|----|------|
| 删除 Prefab 资产时断开实例链接 | **CORE-F26** |
| Prefab Stage 独立相机 / 临时灯 | ED-F16 Amendment B |
| Watcher 全盘 ScanAssets | BUG-ASSET-001 |
| Apply instance → Prefab | 仍后置 |

## Reader quick start

1. Bug Record 症状 · 本文件 §2 现状 · §3 方案  
2. 代码：`PrefabUtility.cpp`（Instantiate / Create / WriteStageTreeToPrefab）· `PrefabStageController::BuildStage` / `SaveActive`

---

## 1) 背景与目标

用户手验 Prefab 创作闭环时同时撞上：Stage 根姿态错、Create Scale 疑似丢、Save 失败、传播验不了。根因集中在 **Instantiate 姿态策略** 与 **Stage Guid 映射生命周期**，合并为 BUG-CORE-002 一次修完垂直切片。

**成功标准：** Stage 可重复 Save；Stage 根姿态 = 模板根；Create 保留世界根 Scale；改非根 default 可传播；根摆放不被 Propagate。

---

## 2) 现状（代码已核对）

### 2.1 Instantiate 无条件覆盖根 Transform（症状 1 / 2 / Stage「初态」）

```text
PrefabUtility::Instantiate
  → clone templates（带模板局部 Transform）
  → FinalizeSceneObjects
  → instanceRoot->SetWorldTransform(params.WorldTransform)   // 默认 Transform{} = identity
```

`PrefabInstantiateParams::WorldTransform` 默认构造为原点 + 单位 Scale。  
`PrefabStageController::BuildStage` 使用默认 `params` → **一进 Stage 根就被写成 identity**，与模板无关。

Level Instantiate（Hierarchy）同样默认刷到原点——可接受为「生成在原点」，但 **Stage 必须保留模板根姿态**。

### 2.2 Create 与世界 Scale（症状 2）

`CreatePrefabFromGameObject`：子树克隆写入模板（序列化的是 **局部** Transform）；对模板根 `DetachFromParent(KeepWorldTransform)` 意图 bake 世界矩阵。  
若源根有父级 Scale，依赖 Detach 分解是否正确；再叠加 Stage Instantiate 的 identity 覆盖，手验极易得出「Create 没尊重世界 Scale」。

Create **不应**改写 Level 源实例 Transform；若源实例被改，属额外 bug（修时断言）。

### 2.3 Stage Save：EditCloneMap 丢根映射（症状 3）

`WriteStageTreeToPrefab` 入口：

```text
EditCloneMap.SourceToClonedGuid.find(prefab.GetRootGuid())
  → 缺失则失败："EditCloneMap missing Prefab root mapping…"
```

日志：同会话 **先 Save 成功**，watcher 全扫后 **再 Save 失败**。说明：

- 首次 BuildStage 的 map 曾有效；
- 首次 `WriteStageTreeToPrefab` 末尾会 `Clear` + 按 stage↔template 重建 map；
- 之后 `prefab.GetRootGuid()` 与 map 键失配，或 map 被清空/Stage 持有的 Prefab 与 map 不同步。

嫌疑（修时钉死其一）：

| 嫌疑 | 说明 |
|------|------|
| C1 | 回写后 `RecordClone` 未覆盖新 `RootGuid`，或只记录了部分对象 |
| C2 | `ClearTemplateObjects` 后失败路径未恢复 map，留下坏 Prefab + 旧 map |
| C3 | Save 触发 Scan/Reload：`stage->Asset` 与 map 仍指旧对象，但 `GetRootGuid()` 被某路径改写；或反之 Asset 被换而 map 未 rebuild |
| C4 | BUG-ASSET-001 全扫放大时序窗（本修仍以 map 契约为准，不替代 001） |

### 2.4 Propagate 与手验（症状 1 / 4）

F24：根 Transform 应跳过（`IsRootTransformPath` + 根组件分支）。  
若 Level 实例根姿态仍随 Stage Save 变，可能是：(a) 实际被 Stage identity **写进模板** 后经 **非 Propagate** 路径看到；(b) skip 失效。本修先消 A/C，再加断言/单测锁 O1。

---

## 3) 方案

### 3.1 Instantiate：显式「是否应用 WorldTransform」

**默认拍板：**

```text
PrefabInstantiateParams:
  bApplyWorldTransform = false;   // 新增；默认 false = 保留克隆根姿态
  WorldTransform{};               // 仅当 bApplyWorldTransform 时使用
```

| 调用方 | 行为 |
|--------|------|
| **BuildStage** | `bApplyWorldTransform = false` → Stage 根 = 模板根局部（无父即世界） |
| **Hierarchy Instantiate** | `bApplyWorldTransform = true`，`WorldTransform = identity`（或日后光标/相机前）→ 生成在原点 |
| **带父 Instantiate** | true + Attach KeepWorld（现有） |

**拒绝：** 继续默认 `SetWorldTransform(identity)` 且无开关（Stage 永久错）。

### 3.2 Create：模板根世界姿态

- Create 完成后断言：模板根 `GetParent()==null`，其局部 Transform 等于源根 **Detach 前世界** Transform（容差）。
- 若 Detach(KeepWorld) 对 Scale 分解有缺陷 → 修 `SceneComponent` 分解或 Create 显式 `SetWorldTransform(sourceWorld)` 再清父。
- Level 源实例 Transform 在 Create 前后不变（单测/手验）。

### 3.3 Stage Save：EditCloneMap 契约

**不变量（Save 前必真）：**

```text
EditCloneMap.SourceToClonedGuid.contains(prefab.GetRootGuid())
  && FindObject(mappedStageGuid) 是 Stage 唯一顶层根
```

**修法（按钉死根因选一，可组合）：**

1. `WriteStageTreeToPrefab` 成功末尾：用 **当前** `prefab.GetRootGuid()` 再校验 map；失败则 **重建** map（扫 Stage 树 vs 模板 Guid），禁止留下半成功状态。  
2. 失败路径：若已 `ClearTemplateObjects`，必须回滚或禁止在清库后失败返回（fail closed + 不清库，或事务式替换模板列表）。  
3. Save 后若 Asset 可能 Reload：Stage 要么固定持有同一 `shared_ptr` 且禁止静默换根 Guid，要么 Reload 时 **Rebuild EditCloneMap**（OpenFromAsset 同逻辑）。  
4. 日志：Save 失败时打印 `RootGuid`、map size、是否含键（便于回归）。

### 3.4 Propagate 加固

- 保持「根 Transform 不传播」。  
- 单测：Stage/模板改根 Position/Scale → Propagate → 实例根不变；改 `CastShadows` 等 → 实例变。

### 3.5 切片建议

| Slice | 内容 | 验证 |
|-------|------|------|
| **S01** | Instantiate `bApplyWorldTransform`；BuildStage 关覆盖 | Stage 打开姿态；单测 |
| **S02** | Create 世界根 Transform / Scale bake + 源实例不变 | 单测 + 手工 |
| **S03** | WriteStageTree map 刷新/回滚/校验；重复 Save | 手工 + 单测 |
| **S04** | Propagate 根跳过 + 非根传播手验；DoD | `test prefab*` |

---

## 4) 备选方案

| 选项 | 结论 |
|------|------|
| Stage 专用 Instantiate 重载（不碰 Level） | 可作实现细节；契约仍要显式开关 |
| Stage 打开后再 Copy 模板 Transform | 可行但易漏；**不如**不覆盖 |
| Save 失败时静默 Rebuild Stage | 丢未保存编辑；**拒绝**作唯一手段 |

---

## 5) 风险与缓解

| 风险 | 缓解 |
|------|------|
| Level Instantiate 默认改行为（不再强制 identity） | 默认 `bApplyWorldTransform=false` 时 Level 需 **显式 true**（Editor 调用处改一处） |
| map 重建扫错树 | 单根 Stage 不变量 + 校验顶层数量 |
| 与 BUG-ASSET-001 交织 | 本修不依赖 001 Done；注明全扫仍可能干扰手感 |

---

## 6) 验收标准

- [x] Stage 打开根 Transform = 模板根（含非单位 Scale）
- [x] Create：模板根世界 Scale 正确；源 Level 实例 Transform 不变
- [x] 同 Stage 连续 Save ≥2 无 EditCloneMap 警告（单测覆盖二次 Write + 模拟 Unregister）
- [x] 非根 default 传播可手验；根 Transform 不 Propagate（既有 overrides 单测）
- [x] `test prefab` / `test prefab-overrides` 绿；Bug Record → Fixed

---

## 7) 开放点（设计默认）

| # | 问题 | 默认 |
|---|------|------|
| O1 | Level Instantiate 是否仍默认刷原点 | **是**（`bApplyWorldTransform=true` + identity） |
| O2 | Stage 是否允许用户改根 Transform 并写入 Prefab default | **是**（改的是模板 default；Propagate 仍跳过实例根摆放） |
| O3 | Save map 失配是否自动 Rebuild Stage | **否**；优先修契约；仅诊断日志 + 并行树 rebuild map |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-16 | **Done：** S01–S04 实现；钉死 Clone Remap 偷源 Guid 槽；Create 外部 unresolved 可继续 |
| 2026-09-16 | **Review：** 合并 Transform / Scale / Save map；显式 Instantiate 开关；map 契约；CORE-F26 Out |
