# MCP-F05 — Prefab 作者化自动化 + 属性引用解析 — Implementation

## Meta
- **ID:** `MCP-F05`
- **Status:** In Progress
- **Design:** [DESIGN](./MCP-F05_PREFAB_AUTHORING_AND_PROPERTY_REFS_DESIGN.md)
- **Branch:** `feat/automation-prefab-ops`
- **开工日期:** 2026-10-19

## 切片

| Slice | 内容 | 状态 |
|-------|------|------|
| S01 | `prefab.create_from_game_object`（不改场景；overwrite 保 GUID + 备份还原） | Done |
| S02 | `prefab.open` / `prefab.close`（走文档打开/标签页关闭路径；幂等、脏 Stage 保留） | Done |
| S03 | `prefab.save`（`SaveStage(key)`：WriteStageTreeToPrefab → SavePrefabAsset → RestoreTemplateObjects） | Done |
| S04 | `set_property` 对象引用（`$assetRef`/`$guid`/`$objectRef`；phase A 全解析才写） | Done |
| S05 | `properties[]` 批量 + `PartialWrite` + `applied[]` | Done |
| S06 | 会话接线（Scene/Prefab 会话 → Stage 场景） | Done |
| S07 | smoke `ui_prefab_smoke.ts`（79 检查，含 [7b] save+close 循环） | Done |
| S08 | Stage save+close 循环稳定性 | Done（[BUG-EDITOR-007](../../bugs/BUG-EDITOR-007.md) 已修） |
| S09 | Editor automation surface 单元测试 | Deferred（无测试目标） |

## 变更面

```
 M Editor/src/Automation/CapabilityRegistry.cpp
 M Editor/src/Automation/EditorAutomationSurface.cpp
 M Editor/src/Automation/EditorAutomationSurface.h
 M Editor/src/Services/Scene/SceneEditorServices.cpp
 M Editor/src/Services/Scene/ScenePropertyValueCodec.cpp
 M Editor/src/Services/Scene/ScenePropertyValueCodec.h
 M Editor/src/SubEditor/Prefab/PrefabStageController.cpp
 M Editor/src/SubEditor/Prefab/PrefabStageController.h
 M minEngine/src/Runtime/Engine.cpp
 M minEngine/src/Runtime/Function/Framework/Scene/SceneManager.cpp
 M minEngine/src/Runtime/Function/Framework/Scene/SceneManager.h
 M minEngine/Tests/Suites/DeferredStructuralEditTest.cpp
 M minEngine/Tests/Suites/DeferredStructuralEditTest.h
?? minEngineMcpServer/scripts/ui_prefab_smoke.ts
```
加上本 Feature 文档与 Registry/日志行。`minEngine/src/Runtime/**` 的改动只来自 S08 的
[BUG-EDITOR-007](../../bugs/BUG-EDITOR-007.md) 场景退休修复（`SceneManager::RetireScene` /
`FlushRetiredScenes`），不改 Prefab 写回语义。

## 验证记录

| 项 | 命令 | 结果 |
|----|------|------|
| 构建 | `cmake --build minEngine/build --target Editor --parallel` | exit 0，`[100%] Built target Editor` |
| 全量验证 | `powershell -File scripts/verify.ps1` | `VERIFY_EXIT=0`，13 suites PASSED / 0 FAILED |
| 端到端 smoke | `cd minEngineMcpServer && npx tsx scripts/ui_prefab_smoke.ts` | `79/79 checks passed`（连续 20 次全绿，见 BUG-EDITOR-007） |

## 已知问题

- 无阻塞项。Stage save+close 循环的偶发 AV 已在 [BUG-EDITOR-007](../../bugs/BUG-EDITOR-007.md) 修复
  （待绘制队列持有已销毁 Stage 场景裸指针 → 场景退休机制）。
- smoke 已含固定 save+close 循环步骤 [7b]，可直接作为 CI 门禁。

## 下一步

1. 把 smoke 接入 CI（`79/79 checks passed` 已连续 20 次稳定）。
2. 补 Editor automation surface 的单元测试目标（S09）。
3. 嵌套 Prefab 引用重写（Deferred）。
