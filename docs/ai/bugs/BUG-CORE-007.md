# BUG-CORE-007 — Canvas 组件失活不使后代 Widget 失效（旧帧残留）

## Meta
- **ID:** BUG-CORE-007
- **Status:** Open
- **Severity:** S2（用户可见：关掉 Canvas 后其下 UI 继续显示最后一帧，直到别的原因触发代理更新）
- **Owner:** 待分配（UI 代理失效传播）
- **Found:** 2026-10-21（渲染数据分域修复期间发现）
- **Affects:** `CanvasComponent` 组件级 active 变更 → 其后代 GameObject 上的 `WidgetComponent` 代理
- **Related:** CORE-F43（活动/可见级联）、UI-F10（Widget 内容源接口化）

## TL;DR

Widget 的绘制位由游戏线程在代理更新时计算，其中包含「所属 Canvas 是否 active」。但 `CanvasComponent` 的 active 变更**不会**让后代 GameObject 上的 Widget 变脏：现有的失效传播只覆盖同 GameObject 的兄弟组件（`Component::SetActive` 里把同 GO 的 Widget 标脏）。

因此组件级关掉 Canvas 后，其下 UI 会**保留最后一帧画面**，直到某个无关事件碰巧触发代理重建。这是「组件 active 不生效」这一类缺陷的最后一个残留面。

## 症状

- 在 Inspector 里关掉某个 `CanvasComponent` 的 active；
- 其下所有 Widget 仍然显示上一帧内容（不是消失）；
- 任何引起代理重建的操作（改布局、改材质、切场景）之后才消失。

`SkyBoxComponent` 目前是同样形状（代理位只在更新时发布），属同一类边界。

## 根因（file:line）

| # | 事实 | 位置 |
|---|------|------|
| 1 | Widget 的 `m_bVisible` 在代理更新时计算，含「装配可绘制 && 自身 active&&visible」 | `WidgetComponent::FillSceneProxy` |
| 2 | canvas 项由游戏线程在 `RenderScene::UpdateWidget` 并入 `m_bVisible` | `RenderScene.cpp` 的 `UpdateWidget` |
| 3 | 组件级 `SetActive` 只把**同 GameObject** 的 Widget 标脏 | `Component::SetActive` → `NotifyOwnerWidgetContentChanged` |
| 4 | 后代 GameObject 上的 Widget 不在该通知范围内 | 由 3 推导 |

## 建议修复方向（未实施）

1. **最小**：`CanvasComponent` 的 active 变更遍历其所属 Canvas 子树，把后代 Widget 全部标脏（只做失效，不改任何作者化字段）。
2. **更普遍**：把「谁是代理内容的驱动者」做成显式登记（谁发布代理位、谁在自身变更时弄脏订阅者），`SkyBox` 与 `Canvas` 都走同一条路径。这条与 UI-F10 的接口化重叠，可合并。

## 验证方向

- 关掉 Canvas 的 active → **同一帧**后 Widget 不再绘制、也不再参与命中。
- 恢复 active → 恢复绘制，且 Widget 与 Canvas 的**作者化 flag 值未变**。
- 反向验证：移除失效传播后该用例必须变红。

## 兼容影响

只改失效传播，不改序列化格式、不改作者化字段语义。

## 变更记录

| 版本 | 日期 | 说明 |
|------|------|------|
| v1 | 2026-10-21 | 建档：渲染分域修复期间发现；含根因与两个修复方向；未修。 |