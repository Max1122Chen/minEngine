# BUG-RENDER-016 — PIE / BasePass SubmitDrawBuffers GetDesc AV (dangling IndexBuffer)

## Meta
- **ID:** BUG-RENDER-016
- **Status:** Fixed (pending PIE verification)
- **Severity:** S1
- **Owner:** project maintainer
- **Found:** 2026-10-25
- **Last updated:** 2026-10-25
- **Affects:** OpenGL Editor PIE · `RHICommandList::SubmitDrawBuffers` · BasePass scene meshes
- **Related Feature/Slice:** Consumer feedback minCraft `ENGINE_FEEDBACK_PROMPT_PIE_SHIFT_F1_RECAPTURE_NULL` (misattributed to PointerPolicy / CORE-F42)

## TL;DR
Entering Play (F5) on minCraft `default` scene crashes in `SubmitDrawBuffers` while calling `IndexBuffer->GetDesc()` — the buffer pointer is non-null but its vtable word is `0x1` (dangling/corrupted). Not a PointerPolicy / cursor-callback bug.

---

## 症状
- Access violation during scene mesh submit (BasePass).
- `ed_crash.log` often shows `FaultRIP=0` or a non-module address when the virtual call jumps through a destroyed vtable; symbolized / gdb stacks land in `SubmitDrawBuffers`.
- minCraft feedback correlated the crash with Shift+F1 → click-to-recapture; engine-side live repro crashes on **F5 / early PIE frames** before release/recapture.

## 期望
- Play remains stable; draw list never holds freed `RHIBuffer*`.
- Invalid packets are skipped (or IndexCount is authored so GetDesc is not required on a raw pointer).

## 复现
1. `MinCraftEditor.exe --project minCraft/minCraft.meproject --open-scene Scenes/default.mescene`
2. Press **F5** (EnterPlay).
3. Crash within the first PIE render frames (Shift+F1 not required).

Automation / evidence paths used:
- `minCraft-render/bin_diag/` + `gdb` (`scripts`-style SendInput F5)
- First crash session: `bin_diag/gdb_repro.log`
- Packet dump: `bin_diag/gdb_packet.log`

## 环境
- OS: Windows 10/11
- Host: minCraft-render `MinCraftEditor` + `libminEngine.dll` (2026-10-25)
- RHI: OpenGL
- Scene: `Scenes/default.mescene`
- Note: `Textures/Ui/white.metex` present in later repros — crash still reproduces (UI missing-texture storm is not required).

## 根因
### Confirmed fault site (gdb)
```
#0 RHICommandList::SubmitDrawBuffers  RHICommandList.cpp:225
     packet.IndexBuffer->GetDesc().ElementCount
#1 SubmitMeshDrawPacket
#2 SubmitSceneMeshDrawPackets         SceneMeshDrawUtils.cpp:102
#3 BasePass::BuildRenderPass          BasePass.cpp:99
…
```

### Packet at fault (gdb_packet.log)
| Field | Value |
|-------|-------|
| `drawIndex` | **1** (of 39 cmds/packets) |
| `IndexBuffer` | `0x202f9e80` (non-null) |
| `VertexBuffer` | `0x20883f50` |
| `IndexCount` / `VertexCount` | **0 / 0** |
| `*IndexBuffer` first qword (vtable) | **`0x1`** (invalid) |
| `MaterialRenderProxy*` | `0xf2b0c80` (looks live) |

`IndexCount == 0` forces the `GetDesc()` fallback. `PrepareSceneMeshDrawPackets` copies raw `m_IndexBuffer` / `m_VertexBuffer` but never fills `packet.IndexCount` (`MeshDrawCommand` has no index-count field). That is OK for live buffers; here the object behind `IndexBuffer` is already destroyed/corrupted, so the virtual call SEGV.

### Why FaultRIP=0 / wild RIP in ed_crash.log
Calling a virtual method through vtable=`0x1` (or freed memory) matches the earlier `ExceptionAddress=0` / unknown RIP records. `CaptureStackBackTrace` from the unhandled filter then shows only the filter frames — which led the consumer prompt to suspect a null OS/ImGui cursor callback.

### Ruled out (for this crash)
- PointerPolicy / `ApplyPointerPolicy` / GLFW cursor callback null call as the crashing instruction.
- Recapture-only trigger: live gdb dies on F5 before Shift+F1.

### Still open (why the buffer is already dead)
Likely proxy/mesh GPU buffer lifetime vs draw-list retention across PIE duplicate / BeginPlay / voxel or mesh rebuild. Same family as primitive recreate races (`BUG-PHYS-005` flavor), but the proven crash is the submit path above — track UAF source as the fix investigation.

## 修复

### 根因（已确认）

帧线程分离后 Game 与 Render 分属两个线程，而 `PrimitiveSceneProxy` 保存的是**裸指针**（顶点/索引缓冲、顶点布局、材质渲染代理），这些资源的真正所有者是 Game 线程上的 `StaticMesh` 与 `Material`。Game 线程销毁组件或换网格（体素块重建即此路径）时释放最后一个资产引用，`RHIBuffer` 对象随即被 free；而代理的移除命令要经命令管道排到 Render 线程的 `FlushApplyForFrame` 才生效，Render 线程在此期间仍在用该代理构造 draw packet。

`SubmitDrawBuffers` 只是受害者：`IndexCount == 0` 让它多走了一次 `GetDesc()` 虚调用，把必然失效的指针变成了必然崩溃。包内 `IndexBuffer` 非空而首字为 `0x1`，说明对象已释放且内存被复用。

### 修复（已落地）

- 代理持有其绘制所需资源的强引用：`RHIBufferRef`（顶点/索引）、`RHIVertexInputLayoutRef`、`std::shared_ptr<MaterialInterface>`。旧裸指针成员保留为**帧视图**，只在一帧内有效。
- 统一绑定入口 `SetMeshResources` / `SetMaterialSource` / `ClearMeshResources`，Static / Skeletal / Sprite / Widget 四个同步点都在 Game 线程把资产句柄交出去。
- 绘制范围贯通：索引数从网格 section 元数据在同步时算出，经 batch → command → packet 传递；`MeshDrawPacket::BindDrawCommand` 是唯一转换点，避免某个产出方忘记区间。
- `SubmitDrawBuffers` 删除两条 `GetDesc()` 兜底：范围为零即不绘制。提交路径不再询问借来的 GPU 对象任何信息。
- 补齐其余 packet 产出方（阴影、后期/呈现的全屏四边形、天空盒、屏幕 UI）的范围，因此兜底是**被删除**而不是被绕过。

### 已知副作用与后续

- 代理持有材质强引用，材质可能在 Render 线程析构，而 `MEObject` 析构会触达 `ObjectManager`（游戏线程状态）。需要延迟释放队列，已登记 TECH_DEBT。
- 同类隐患（组件反向指针、屏幕 UI 的控件回指、骨骼调色板、控件贴图借用）本次不修，逐条登记 TECH_DEBT。
- 崩溃诊断增强（故障 CONTEXT + RSP dump）仍是独立事项。

## 回归验证

自动化（headless）：

- [x] 新套件 `scene-proxy-lifetime`：资产释放后代理仍持有缓冲与布局、删除代理后释放；材质所有者随代理存活；索引数来自元数据且提交过程不查询缓冲（`DrawIndexed(36)` 且 `GetDesc` 调用计数不变）。
- [x] 相邻套件回归：`frustum`、`mesh-draw-sort`、`screen-ui-text`、`sprite-translucency` 全绿。
- [x] `minEngineTests.exe test smoke` 全绿。

手工（待执行）：

- [ ] Editor：Play `default` 场景存活 ≥30s。
- [ ] Play → Shift+F1 → 点击视口图像 → 重新捕获，无访问违例。

> 手工项需要重建 `D:/Dev/GitRepo/minCraft*` 的编辑器宿主：它们当前导入的符号早于引擎删除，未重建的二进制无法加载当前引擎库。

## 关联
- minCraft: `docs/ai/INFRA/ENGINE_FEEDBACK_PROMPT_PIE_SHIFT_F1_RECAPTURE_NULL.md`
- Related lifetime: `BUG-PHYS-005` (proxy null during recreate)
- Evidence: `minCraft-render/bin_diag/gdb_repro.log`, `gdb_packet.log`

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-10-25 | Opened from live gdb repro; PointerPolicy attribution rejected |
| 2026-10-25 | 根因定为跨线程资源所有权缺失（非 PointerPolicy、非 submit 路径）；代理强引用 + 绘制范围贯通已落地，headless 回归绿；手工 PIE 验证待外部宿主重建 |
