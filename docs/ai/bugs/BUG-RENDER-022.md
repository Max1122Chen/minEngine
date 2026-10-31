# BUG-RENDER-022 — 布局来自管线状态的 Info 在 Single 拓扑下每帧一条

- **Status:** Fixed
- **First seen:** 2026-10-31，真机一次会话 4123 条
- **Area:** Render / RHI 提交日志

## 现象

```
[Render] [Info] Draw submitted with the vertex input layout the graphics pipeline state binds
(6 requested): the packet carries none, ... Reported once per command list.
```

- 每次 6 顶点/索引 = 全屏 quad：后处理与 Present 的 packet 只填 `VertexBuffer/VertexCount`，
  布局在 PSO 上（`PostProcessPass.cpp`、`PresentPass.cpp`）；
- 一条会话数千条，淹没真机日志。

## 根因

限频状态是命令列表的成员（`RHICommandList::m_bReportedPipelineStateVertexLayout`）——
"每个 command list 一次"。而命令列表**不是稳定的工作单位**：

- Single 拓扑（immediate）：每帧在栈上新造一个列表（`RHICommandList`），于是每帧一条；
- Multi 拓扑：列表来自池，池会复用，但一帧仍可能换列表。

"每列表一次"因此等价于"每帧一次"，信息退化为噪声。

## 修复

限频改为**进程级**（`g_PipelineStateVertexLayoutDrawCount`，匿名命名空间内的 `std::atomic`）：
首次出现必报（带本次 draw 的请求量），之后每 4096 条一条，并带**累计计数**
（"... N draw(s) have read their vertices this way in this process; reported on the first one
and then every 4096."）。头文件里那个 per-list 成员删除，不再有第二条限频路径。

## 回归检查

- `draw-packet-lifetime` 新增两例（Single / Multi 各一）：12 帧、每帧一个新列表，
  断言该 Info 全进程 `>= 1`（首次可见）且 `<= 1`（不随帧增长），且文案带累计计数。
- 红灯（限频临时改回"每次绘制一条"）：`CHECK( notice.Count.load(...) <= 1 )` →
  `values: CHECK( 14 <= 1 )`。
- 真机对照（本机 `MaximumEditor.exe --project MyMEProject --frame-threads single`，
  同一镜头下列表每帧新建）：改前一次约 60s 会话 **31860 条**；改后 20s 会话 **2 条**
  （`1 draw(s)` 与 `4096 draw(s)` 各一条，Multi 与 Single 各 2 条）。

## 规则

日志限频不能挂在"可能是每帧新造"的对象上：限频状态的寿命必须长于它要抑制的重复周期，
否则"一次"会退化成"每帧一次"。
