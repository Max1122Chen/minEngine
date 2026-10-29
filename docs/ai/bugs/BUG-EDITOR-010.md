# BUG-EDITOR-010 — ImGui 纹理归位引入的两处回归：多线程拓扑永不实体化 / 释放走错队列

## Meta
- **ID:** BUG-EDITOR-010
- **Status:** Fixed
- **Severity:** S1（视口在 Multi 拓扑永久不可用；Vulkan 下每个标识符都真的泄漏）
- **Owner:** Editor Platform / RHI 延迟创建
- **Found:** 2026-10-29（用户真机反馈）
- **Last updated:** 2026-10-29
- **Affects:** `EditorRHIImGuiTexture::Request/``RequestRelease`、`EditorRHIImGuiTexturePin::Pin/Reset`、`Editor::Shutdown`
- **Related:** commit `34b482c6 fix(editor): create ImGui texture bindings on the graphics role` · [TD-051](../TECH_DEBT.md)

## TL;DR

上一片把"注册 ImGui 纹理"改成走 RHI 的延迟创建队列，同时把 pin 注册表换成 weak 列表，引入了两处独立回归：

1. **Multi 拓扑视口永久 "Scene color texture is not ready."**：视口拿到 `ImTextureID_Invalid` 后当帧就把 pin 交还（`Reset`），而 `Reset` 会释放绑定 → 队列里那条创建请求变成"只剩队列自己持有" → 队列的 NotNeeded 优化判定"没人要了"，**永远不创建**。标识符因此恒为 0，视口每帧都落到兜底文案，且全程 0 条错误日志。
2. **每条 ImGui 纹理标识符的释放都丢弃并报错**：释放走的是 `PostNativeDestroy` → `RHIDeferredDestroyQueue::s_ActiveQueue`。该注册表是 `RHIDeferredDestroy.h` 里的类内 `inline static`，而该类未导出——**每个模块各有一份**。引擎 DLL 里 RHI 注册的那份，编辑器 DLL 那份永远是空的，于是编辑器里每一次释放都报 "no RHI was alive to destroy it" 并把标识符直接丢掉。这不是关停顺序问题：泄漏行从第一个编辑器帧开始就以约 150 行/秒稳定出现（本次实测单次会话 2286 行）。

## 症状

- **A（Multi）：** 视口永远显示 `Scene color texture is not ready.`（`EditorViewportWindow::DrawSceneColorImage` 的两处兜底文案），日志 0 条错误。Single 拓扑正常。
- **B（Single/Multi 皆有）：** 日志被 `[RHI] [Error] editor ImGui texture identifier: last reference dropped while no RHI was alive to destroy it` 刷屏；用户会话 533 行，本次复现会话 2286 行。OpenGL 后端下"泄漏"只是记账（release 是空操作）；Vulkan 后端下每次都是真实的 `VkDescriptorSet` 泄漏。

## 期望

- 一个"还没实体化"的请求，在调用方当帧用不上时也必须活到图形角色跑过它为止；等待方（pin）持强引用。
- 释放必须投递给"标识符所属设备"的销毁队列；设备真的没了时静默并计数（池随设备对象一起没了，不是泄漏）。
- 两条都要有红灯：A 用"多帧后仍 Invalid"，B 用"泄漏计数 > 0"。

## 复现

- A：`minEngineTests.exe test editor-imgui-texture` 的 "a request a frame cannot use yet is still built by the graphics role"（修复前红）。
- B：同套件的 "a release reaches the queue of the device that owns the identifier"（修复前红：`CHECK( GetPendingCount() == 1 )` → `0 == 1`，队列同时打出 "no RHI was alive"）。
- 真机：`MaximumEditor.exe --project <proj> --frame-threads single|multi`，看图口文案 / 数日志行。

## 环境

- Windows，契约构建 `minEngine/build`（Debug），产物 `minEngine/bin`。
- 单次真机复现：`--frame-threads single`，OpenGL 后端，约 20 秒运行。

## 根因

### A · 请求在实体化前被等待方丢掉

`EditorRHIImGuiTexture::Request` 建好绑定后交给 `RHI::ScheduleNativeCreate`，队列的 lambda 持有该 `shared_ptr`，并在 drain 时用 `target.use_count() == 1` 判断"没人要了 → `NotNeeded`、不创建"。

Multi 拓扑下 `RHIGraphicsRole::IsCurrent()` 在 UI 线程为假，所以请求留在队列里；同帧视口因为拿到 Invalid 而调用 `m_PinnedImGuiTexture.Reset(rhi)`，旧 `Reset` 立即 `RequestRelease` 并丢掉绑定。此时强引用只剩队列那个 lambda → `use_count() == 1` → 下一帧 drain 命中 `NotNeeded` → 下一帧又建新请求又丢弃，循环到天荒地老。

（Single 拓扑下 `IsCurrent()` 为真，创建在 `Request` 里同步完成，所以这条缺陷只在 Multi 显形——正是"日志 0 条错误"的原因：`NotNeeded` 按设计静默。）

### B · 释放走的是"每模块一份"的 header 静态

`RequestRelease` → `PostNativeDestroy` → `RHIDeferredDestroyQueue::PostToActiveQueue` → `s_ActiveQueue`（`inline static`，类未导出）。`libMaximumEditord.dll` 与 `libminEngined.dll` 各有一份：`RHI::RHI()` 在引擎模块里注册，编辑器模块那份恒空 → `PostToActiveQueue` 每次都走"没有 RHI 存活"分支：打 Error、计数、**回调丢弃**。

**证据链（证伪"关停顺序"假设）：**
1. `OpenGLRHI Initialized`（日志第 11 行，22:00:38）与 `OpenGLRHI Shutdown`（第 2445 行，22:00:58）之间，泄漏行从 22:00:42 起每秒稳定 130–166 行。
2. 该消息只在 `GetActiveQueue()` 为空时出现（`PostToActiveQueue` 唯一分支），期间 RHI 显然存活。
3. `RHIDeferredDestroyQueue` 无 `MINENGINE_API`，`s_ActiveQueue` 也不在 `minEngine_export_snapshot.txt` 中 → 跨模块不可能共享。
4. 关停顺序反而是安全的：`Editor::Shutdown` 先 `m_ImGuiBackend.Shutdown()`（`UninstallBackendOps` 把所有标识符随设备对象一起丢弃、归零），再 `m_Engine->Shutdown()`（RHI 才死）。pins 确实在 RHI 之后才析构，但那时它们手里的标识符已经是 0，构造不出那条消息。用户"关停时刷屏"的印象来自"日志尾部才看"。

## 修复

- **A（谁持、持到什么时候）：** 由 **pin** 持强引用，从 `Request` 起，到"标识符已存在（`IsReady`）/创建已失败/被另一个纹理的请求顶替/pin 析构"为止。`Pin` 改用绑定自带的 `Matches` 判身份（顺带删掉 `m_BoundRhi/m_BoundTexture` 这两个只写不读的字段），`Reset` 只对"已经有标识符或已失败"的绑定走释放，**未实体化的请求保留**。于是队列里那条请求在 drain 时 `use_count() >= 2`，`NotNeeded` 不再命中。
  - 为什么不改 NotNeeded 判定：那条优化的语义是对的（"没人再要的请求不必创建"），错的是等待方把请求丢了。改队列只能靠猜（队列看不到调用方是否会再问），而让等待方持住请求是能写清所有权的那一半。
- **B：** `RequestRelease` 直接 `m_Rhi->GetDeferredDestroyQueue().Post(...)`——和延迟创建走的是同一条"绑在自己持有的 RHI 上"的路（`ScheduleNativeCreate` 一直是这样，所以创建从来没坏过）。设备真的没了时（`GetReleaseIdentifierOp()` 为空，即后端已 `UninstallBackendOps`）静默计数 `ReleasesDropped`，因为标识符随设备对象一起消亡，不是泄漏。设备代次（`DeviceObjectsGeneration`）检查保留。
- **关停顺序钉住：** `Editor::Shutdown` 在 `m_ImGuiBackend.Shutdown()` 之后、`m_Engine->Shutdown()` 之前打印一行注册表/创建队列计数，把"退役了多少、丢了几个、队列建了几个/丢了几个人要的"变成日志的一部分。

## 验证

- `editor-imgui-texture`：9 例 / 72 断言 PASS（原 7 例 / 61 断言；新增 2 例，且原 7 例不改一行仍绿）。
- `rhi-deferred-create`：18 例 / 154 断言 PASS。`test smoke`：exit 0。
- 真机（同一份构建）：
  - `--frame-threads single`：ImGui 纹理泄漏行 **2286 → 0**；报告行 `2860 registration(s), 2860 retired at a batch tail, 0 dropped, 0 discarded`。
  - `--frame-threads multi`：报告行 `3286 registration(s), 3286 retired, 0 dropped`，创建队列 `3342 posted, 3342 created, **0 not needed**, 0 failed`——即 Multi 下每条请求都实体化了（修复前视口永远拿不到标识符）。
- 导出门禁：3014（+0），无符号消失，快照未变。

## 残留

- `RHIDeferredDestroyQueue::s_ActiveQueue` 仍是每模块一份的 header 静态 → **任何其它模块**在模块内析构引擎侧 GPU 资源会踩同一坑，见 [TD-051](../TECH_DEBT.md)。
- 编辑器里"每帧注册一次纹理"的既有抖动未查（本次会话 2860 次注册 / 20 秒）：修复后每次都正确退役、0 丢弃，但抖动本身不是本记录的范围。
- Vulkan 后端未在本次真机复现中跑（`--rhi vulkan` 未验证）；OpenGL 下的证据足以定位两条根因，因为两条都与后端无关。
