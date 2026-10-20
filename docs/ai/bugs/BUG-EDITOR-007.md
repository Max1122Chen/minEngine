# BUG-EDITOR-007 — Prefab Stage 反复 save+close 后偶发访问违例

## Meta
- **ID:** BUG-EDITOR-007
- **Status:** Open
- **Severity:** S2（不阻日常使用；阻把 smoke 当 CI 门禁）
- **Owner:** 待分配（Runtime / Prefab 写回与场景销毁路径）
- **Found:** 2026-10-19
- **Affects:** `PrefabStageController` save/close 循环之后的编辑器主循环
- **Related Feature/Slice:** [MCP-F05](../Editor/MCP/MCP-F05_PREFAB_AUTHORING_AND_PROPERTY_REFS_DESIGN.md) S08 · `scripts/ui_prefab_smoke.ts`

## TL;DR

连续执行 `prefab.open → 改树 → prefab.save → prefab.close` 循环后，编辑器有约 **1/3** 概率在下一个自动化请求时以 `0xC0000005` 崩溃（栈在 Engine tick / libminEngine 内，尚未到达 automation surface）。

## 复现

```powershell
cd minEngineMcpServer
npx tsx scripts/ui_prefab_smoke.ts    # 约 3 次就能看到一次
```
或最小循环：open → 在 `rootGuid` 下建子节点 → save → close，随后再发一条 `scene.create_game_object`。

## 隔离结论（已做的实验）

| 场景 | 结果 |
|------|------|
| 只 open/close（不 save）× 4 | 稳定 |
| open → 加子节点 → close(discard) × 4 | 稳定 |
| open → 加子节点 → save（Stage 保持打开）× 4 | 稳定 |
| open → 加子节点 → save → close × 5 | 修复前 2/5 崩；修复后隔离探针 15/15 绿，但整轮 smoke 仍约 1/3 崩 |

## 已修复

- `prefab.close` 后未把 inspect 目标重置回文档场景，留下指向已销毁 Stage 场景的悬垂目标（一个真实窗口）。修后该探针 15/15 稳定。

## 残余嫌疑（均在 `minEngine/src/Runtime/**`，本次未动）

1. `PrefabUtility` Stage 回写时销毁/重建模板 GameObject；
2. `RefreshEditCloneMapAfterWrite` 的映射重建时序；
3. `ObjectManager` GUID 重映射；
4. 场景/渲染销毁路径在 Stage 关闭后的回调。

## 绕路（已采用，不阻消费方）

消费方作者化主路径只用 `prefab.create_from_game_object(overwrite=true)`（重建即落盘，不进入 Stage 循环）；`open/save/close` 在修复前属实验能力。

## 退出条件

`ui_prefab_smoke.ts` 连续 20 次全绿（不崩），且 Stage 循环不再需要「不要用」的告诫；随后把 smoke 接入 CI。
