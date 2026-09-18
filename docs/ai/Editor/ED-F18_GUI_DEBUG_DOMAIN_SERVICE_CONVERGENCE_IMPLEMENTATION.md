# ED-F18 — Implementation Plan

## Meta
- **ID:** `ED-F18`
- **Status:** Done
- **Last updated:** 2026-09-18
- **Design:** [ED-F18_GUI_DEBUG_DOMAIN_SERVICE_CONVERGENCE_DESIGN.md](./ED-F18_GUI_DEBUG_DOMAIN_SERVICE_CONVERGENCE_DESIGN.md)

## Slices

| Slice | Status | Notes |
|-------|--------|-------|
| S01 | Done | `SceneEditorServices` 原语；Editor 持有；Host 引用；Surface 适配（Hierarchy/Json 仅 Surface） |
| S02 | Done | 删除 `SceneAutomationServices` / `ISceneAutomationServices` |
| S03 | Done | Debug `add_go`/`delete_go`/`add_comp`/`remove_comp`/`reparent` → Services |
| S04 | Done | Scene GUI 发 cmd 薄壳 + Debug rename/move/set blob 同栈（见下） |

### S04 明细

| 项 | Status | 说明 |
|----|--------|------|
| Create/Delete/Add/Remove/Reparent/`CommitObjectPropertyBlobs` | Done | `SceneEditor::Submit*` → Services |
| Rename（GO/Component） | Done | `SetProperty(..., "m_Name", ...)` |
| `MoveComponent` | Done | 新原语；GUI + Debug |
| `InstantiatePrefab` | Done | 仅发 cmd；对话框/Load 留 Submit 控制流 |
| Console/Builtin `set` | Done | → `CommitObjectPropertyBlobs`（字面量→void* 仍可后置） |
| B1 Gizmo Transform | Done | `SetTransform(before, after)` → 既有 `EditorSetGameObjectTransformCommand`；不走 SetProperty |
| Create Prefab | Deferred | 资产 Create 族，另议 |
| Material / Anim Submit | Out | 后置 |

## Key paths

- `Editor/src/Services/Scene/SceneEditorServices.*` · `SceneEditorResult.h`
- `Editor/src/SubEditor/Scene/SceneEditor.cpp`（Submit 薄壳）
- `Editor/src/Shell/EditorContextHelpers.h`（`GetSceneDocumentSession`）
- `Editor/src/Automation/EditorAutomationSurface.*`（Client）
- `Editor/src/Automation/AutomationHost.*`（引用 Services）
- `Editor` / `IEditorContext::GetSceneEditorServices`
- `Editor/src/Services/EditorSceneDebugCommands.cpp`
- `Editor/src/Services/EditorConsoleCommands.cpp`

## Verify

```text
cmake --build <build> --target Editor
cd minEngineMcpServer && npm run accept:mcp -- --hold 20
```

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-09-18 | S01–S03 落地；S04 / Debug `set` void* 收敛后置 |
| 2026-09-18 | S04：Move/Instantiate/CommitBlobs；Rename→SetProperty；GUI Submit 发 cmd 薄壳；B1 `SetTransform` 专用 command；CreatePrefab 后置 |
