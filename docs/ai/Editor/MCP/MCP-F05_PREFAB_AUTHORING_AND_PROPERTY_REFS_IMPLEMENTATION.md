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
| S07 | smoke `ui_prefab_smoke.ts`（55 检查） | Done |
| S08 | Stage save+close 循环稳定性 | **Blocked** → BUG-EDITOR-007 |
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
?? minEngineMcpServer/scripts/ui_prefab_smoke.ts
```
加上本 Feature 文档与 Registry/日志行。**零写入 `minEngine/src/Runtime/**`**。

## 验证记录

| 项 | 命令 | 结果 |
|----|------|------|
| 构建 | `cmake --build minEngine/build --target Editor --parallel` | exit 0，`[100%] Built target Editor` |
| 全量验证 | `powershell -File scripts/verify.ps1` | `VERIFY_EXIT=0`，13 suites PASSED / 0 FAILED |
| 端到端 smoke | `cd minEngineMcpServer && npx tsx scripts/ui_prefab_smoke.ts` | `55/55 checks passed`（两次独立跑通） |

## 已知问题

- Stage save+close 循环偶发 AV（约 1/3）→ [BUG-EDITOR-007](../../bugs/BUG-EDITOR-007.md)；已修一处悬垂 inspect 目标，残余嫌疑在 Runtime。主路径用 `create_from_game_object` 规避。

## 下一步

1. 修 BUG-EDITOR-007（Runtime 侧）后再把 smoke 接入 CI。
2. 补 Editor automation surface 的单元测试目标（S09）。
3. 嵌套 Prefab 引用重写（Deferred）。
