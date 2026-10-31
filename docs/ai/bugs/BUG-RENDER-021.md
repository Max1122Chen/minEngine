# BUG-RENDER-021 — 未带顶点布局的绘制被当成"布局不可用"而拒绝提交

- **Status:** Fixed
- **First seen:** 2026-10-31，真机日志（200 次以上）
- **Area:** Render / RHI 提交门禁

## 现象

真机日志持续出现同一条 Error，画面缺少对应内容：

```
[Render] [Error] Draw not submitted: the bound vertex input layout (0x0) has no native object
and none is coming, so the draw would name no data (6 requested). N draw(s) refused so far.
(RHICommandList.cpp:63)
```

- 目标地址是 `0x0`：不是"句柄未实体化"，而是根本没有布局；
- 每次 6 索引 = 一个 quad，指向全屏 quad 与 ScreenUI / 文字 / sprite 的 quad 路径；
- 反复出现说明这些 draw 从未被提交。

## 根因

`SubmitDrawBuffers` 只读 `MeshDrawPacket::VertexInputLayout`，为 `nullptr` 时
`GetDeferredUse(nullptr)` 返回 `Unusable`，于是按"布局不可用"拒绝。

而"包里没带布局"与"布局不可用"是两件事：`RHIGraphicsPSODesc::VertexInputLayout`
（`RHIGraphicsPipelineState.h:84`）可以让**管线状态**提供布局，OpenGL 后端在
`ApplyGraphicsPipelineState` 里确实用它绑定顶点数组（`OpenGLRHI.cpp:397-405`）。
后处理与 Present 的 packet 就属于这一类：`psoDesc.VertexInputLayout = m_ScreenQuadVertexLayout`，
packet 只填 `VertexBuffer/VertexCount = 6`（`PostProcessPass.cpp:122`、`202-204`）。

附带遗留：文字路径的 `WidgetDrawData`/`WidgetSceneProxy` 只带裸指针，
proxy 的 `m_VertexInputLayoutRef` 被显式 reset，draw 无法作为句柄持有字形布局。

## 修复

1. **门禁三段判断**（`RHICommandList.cpp`）：取包内布局（ref 或裸指针）→ 为空则取管线状态的
   布局；两者皆空才拒绝（新增独立计数与 Error，报告一条/每 100 条）；非空但未实体化或已失败
   仍按原样拒绝。布局来自管线状态时首次（每个 command list 一次）记一条 Info。
2. **源头补齐**：`WidgetDrawData` 增加 `RHIVertexInputLayoutRef`；文字 graphic 从
   `TextMeshBuilder` 的进程级句柄取 ref 一并传出；`WidgetSceneProxy` 只保留收句柄的重载，
   裸指针重载删除（不再有第二条绑定路径）。

## 回归检查

- `draw-packet-lifetime`：新增 3 例
  （无布局 packet + 带布局 PSO → 提交并出现 Info；PSO 布局未实体化 → 仍拒绝，实体化后提交；
  两处都无布局 → 拒绝），以及文字路径"proxy 绑定的布局就是 draw 命名的布局且可用"。
- `draw-packet-lifetime` 既有 "a draw whose vertex layout is not materialized is not submitted" 保持通过。
- `gpu-static-lifetime` 保持通过：共享字形布局仍是模块内单个 function-local static，
  释放入口仍是同一个访问器。

## 规则

"绘制没带某个状态"与"该状态不可用"必须分开判断：只有当**所有**提供者都不提供时才是错误。
提供状态的对象要能被提交前读到（这里是 pipeline state 的描述）。
