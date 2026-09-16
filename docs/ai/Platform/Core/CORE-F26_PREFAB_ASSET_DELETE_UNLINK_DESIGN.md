# CORE-F26 — Prefab 资产删除与实例断链 — Design Spec

## Meta
- **ID:** `CORE-F26`
- **Type:** Feature
- **Status:** Review
- **Owner:** project maintainer
- **Last updated:** 2026-09-16
- **Branch:** `feat/prefab`（或后续 `feat/prefab-unlink`）
- **Related:**
  - [FEATURE_REGISTRY](../../FEATURE_REGISTRY.md) · [ACTIVE_WORK](../../ACTIVE_WORK.md)
  - [CORE-F23](./CORE-F23_PREFAB_ASSET_INSTANTIATE_DESIGN.md)（`PrefabInstanceRecord`）
  - [CORE-F24](./CORE-F24_PREFAB_OVERRIDES_DESIGN.md)
  - [ED-F16](../../Editor/ED-F16_PREFAB_EDITOR_DESIGN.md)
  - [BUG-CORE-002](../../bugs/BUG-CORE-002.md)（Stage Transform/Save — **正交**）
  - [BUG-ASSET-001](../../bugs/BUG-ASSET-001.md)
- **Depends on:** CORE-F23 Done（实例表）；AssetManager `DeleteAsset`
- **Blocks:** 可安全删 Prefab 而不留下假实例

## TL;DR

**问题：** `DeleteAsset` 明确 **reference scan not implemented**；删除 `.meprefab` 后 Scene 仍保留 `PrefabInstanceRecord` → Hierarchy 假实例、Resolve 失败/幽灵 Guid。  
**方案（默认）：** 删除前扫描 **已打开** Editor Scene；有引用则 **阻断** 或提供 **Unpack 后删除**；删除成功则清除/烘焙相关 Record。未打开 Scene 的磁盘引用本期仅文档警告。  
**当前：** **Review（待审批）** — 未实现。

## Scope

### In

- Prefab（及可扩展：任意带 Scene 侧 Guid 引用的资产）删除时的 **引用策略**
- 打开 Scene：检测 `PrefabInstanceRecord.PrefabAssetGuid == deleted Guid`
- 策略落地：阻断删除 **或** Unpack（去 Record，保留 GO 数据）后删除
- Editor：删除 Prefab 时的错误/确认 UX（Content Browser / DeleteSelectedAsset）
- 单测：有实例时删失败；Unpack 后 Record 空且 GO 仍在

### Out

| 项 | 说明 |
|----|------|
| 扫全盘未打开 `.mescene` 批量改写 | Out（可后置工具） |
| Missing Prefab 占位 Actor / 红字持久化语义 | 后置（可选 Phase 2） |
| Stage Transform / Save / Propagate | **BUG-CORE-002** |
| 通用任意资产引用图（Mesh 被删等） | 可复用扫描骨架，但本期只保证 Prefab 实例表 |

## Reader quick start

1. §2 现状 · §3 策略拍板 · §4 备选  
2. 代码：`AssetManager::DeleteAsset` · `Scene::m_PrefabInstances` · CB Delete

---

## 0) Pre-flight（摘要）

| 项 | 判断 |
|----|------|
| Prerequisites | PrefabInstanceRecord **sound**；DeleteAsset 无引用扫描 **known gap** |
| Tech-debt | **Medium** — 只做开 Scene + Prefab 表，避免假「全图引用」 |
| WIP | BUG-CORE-002 Review；本 Feature **并行可设计，实现建议在 002 后或并行不碰 Instantiate** |
| Philosophy | Mechanism：断链/阻断是引擎机制；Unpack 是可选政策，默认提供明确选择而非静默丢数据 |
| Recommendation | **Go with scope cut**：开 Scene 阻断 + Unpack 删除；全盘迁移 Out |

---

## 1) 背景与目标

日志：

```text
DeleteAsset: reference scan is not implemented (v0); proceeding with 'Prefabs/Cube.meprefab'.
```

删除后 Level 仍显示 Prefab 实例外观，但资产已不存在 → 假链接。目标：删除与实例身份 **一致**——要么删不掉，要么删前变成普通 GO（或明确 Missing，本期不做）。

---

## 2) 现状

- `AssetManager::DeleteAsset`：删文件 + meta + 注册表；**不**读 Scene。
- Editor 实例身份：`Scene::m_PrefabInstances` + Hierarchy 着色。
- Resolve Prefab：按 `PrefabAssetGuid` Load；资产没了则失败，Record 仍在。

---

## 3) 方案（默认拍板）

### 3.1 删除前门禁（开 Scene）

```text
DeleteAsset(path) where type==Prefab:
  guid = meta.Guid
  refs = FindPrefabInstanceRefsInOpenEditorScenes(guid)
  if refs 非空:
    → 默认：失败，返回错误（列出 Scene 名 + 实例根名/数量）
    → Editor 可选：「Unpack 引用并删除」→ Unpack 全部 refs → 再 DeleteAsset
  else:
    → 正常删除
```

### 3.2 Unpack 语义（删除路径用）

对每个命中的 `PrefabInstanceRecord`：

1. 从 `m_PrefabInstances` **移除**该 Record（含 Overrides）。
2. **保留**场景中 GO/Component 数据与 Guid（已是世界对象）。
3. Hierarchy 立即按「非实例」绘制（无 Prefab 色/图标）。

不做：删 GO；不改 Transform；不写回 Prefab。

### 3.3 未打开 Scene

- 本期：**不**扫磁盘 `.mescene`。
- 文档/对话框注明：其它 Scene 文件可能仍含旧 Record；打开时可选校验（后置：加载时若 Guid 无资产则自动 Unpack 或标 Missing）。

### 3.4 API 草图

```text
struct PrefabInstanceRef {
  Scene* Scene;
  GUID RootInstanceGuid;
  std::string RootName; // 诊断
};

std::vector<PrefabInstanceRef>
  PrefabUtility::FindInstanceRefsInOpenEditorScenes(const GUID& prefabAssetGuid);

bool PrefabUtility::UnpackInstance(Scene& scene, const GUID& rootInstanceGuid, std::string* outError);
bool PrefabUtility::UnpackAllInstancesOfPrefab(Scene& scene, const GUID& prefabAssetGuid, ...);

// AssetManager::DeleteAsset — Prefab 分支调用门禁；或 Editor 包一层 DeletePrefabAsset
```

### 3.5 Editor UX

- 普通删除：有引用 → Toast/对话框说明数量，**不删**。
- 「Unpack and Delete」：确认后 Unpack 所有开 Scene 引用再删。
- 无引用：直接删（现行为）。

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| **A. 阻断 + 可选 Unpack 删除** | 不丢摆放；语义清 | 多一步确认 | **选用（MVP）** |
| B. 静默 Unpack 再删 | 少点一下 | 用户未察觉断链 | 可作高级选项，不作默认 |
| C. 仅清 Record 留「裸」链接字段 | — | 易残留半状态 | **拒绝** |
| D. Missing Prefab 占位 | 长期正确 | 本期过重 | **Phase 2** |
| E. 阻断且无 Unpack | 实现最简 | 用户无法删仍被引用的 Prefab | 不单独采用 |

---

## 5) 风险与缓解

| 风险 | 缓解 |
|------|------|
| 未打开 Scene 仍脏 | 文档警告；后置打开时校验 |
| Unpack 与 Undo | MVP：删除操作为一包（Unpack+Delete）或标不可 Undo；可后置 |
| 扩到 Mesh 等引用 | 接口按 Guid 扫描预留；本期只接 PrefabInstance 表 |

---

## 6) 验收标准

- [ ] 开 Scene 有该 Prefab 实例时，普通 Delete **失败**且说明引用
- [ ] Unpack and Delete：Record 清除，GO 仍在，资产文件/meta 删除
- [ ] 无引用时 Delete 成功
- [ ] 单测覆盖阻断与 Unpack
- [ ] 文档：Registry Done 勾选；与 BUG-CORE-002 边界清晰

---

## 7) 切片建议

| Slice | 内容 | 验证 |
|-------|------|------|
| **S01** | FindInstanceRefs + Delete 门禁（阻断） | 单测 |
| **S02** | UnpackInstance / UnpackAll | 单测 |
| **S03** | Editor「Unpack and Delete」 | 手工 |
| **S04** | DoD / 加载期脏 Record 警告（可选） | — |

---

## 8) 开放点（设计默认）

| # | 问题 | 默认 |
|---|------|------|
| O1 | 默认删除策略 | **阻断**；Unpack 需显式确认 |
| O2 | 未打开 Scene | **不扫盘**；打开后再议自动 Unpack vs Missing |
| O3 | 是否进通用 Asset 引用框架 | **本期仅 PrefabInstance 表** |
| O4 | Stage 正打开该 Prefab 时删除 | **先关/Discard Stage** 再删（实现时硬拦） |

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-16 | **Review：** 删 Prefab 断链；阻断 + Unpack 删除；全盘 Out；与 BUG-CORE-002 拆分 |
