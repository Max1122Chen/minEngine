# BUG-RENDER-020 — 进程级静态持有 GPU 句柄，设备销毁后释放导致泄漏

- **Status:** Fixed
- **First seen:** 2026-10-30, Single 拓扑，关闭编辑器时
- **Area:** Render / Teardown

## 现象

关闭编辑器时日志末尾出现一条：

```
[RHI] [Error] openGL vertex input layout: last reference dropped while no RHI was alive to destroy it;
              the native object leaks (it is not destroyed on this thread).
```

进程不崩，但 native 对象泄漏，且这条消息出现在引擎关停之后约 180 ms。

## 根因

关停顺序（日志与源码对位）：`Engine::Shutdown` → SceneManager → UISystem → ObjectManager →
`RenderSystem::Shutdown`（销毁句柄、随后 `m_RHI->Shutdown(); m_RHI.reset()`）→ 窗口终止 → 管道关停
→ **183 ms 后** 报错。

设备的延迟销毁队列挂在设备本身，设备 reset 之后队列为空，此时任何最后引用的释放只能报错放弃。
而报错对象的所有者是**进程级静态**：`TextMeshBuilder` 里的 `static RHIVertexInputLayoutRef` 只创建、
从不释放，唯一调用方是文字渲染路径（任何渲染过文字的会话都会实例化它），于是它活到进程退出。

## 修复

- 工厂与释放入口共享同一个访问器，静态不再是藏在 `Get` 函数体内的局部副本；
- 新增 `ReleaseSharedLayout()`/`HasSharedLayout()`，释放后再次调用仍能重建；
- 在 `RenderSystem::Shutdown` 里、`RHIDrainAllDeferredDestroys` 与 `m_RHI->Shutdown()` **之间**调用，
  与既有的单例网格释放同款，句柄走普通延迟销毁路径；
- 同类审计另发现 `FontGlyphAtlasCache` 的进程静态持有 `Texture2D` 且从不释放，一并在此处清空。

## 回归检查

- `gpu-static-lifetime`：断言"关停流程结束时没有进程级静态仍持有 GPU 句柄"，
  修复前以探针计数失败（`HandlesAlive == 1`），修复后通过。
- 真机：关闭编辑器后日志不应再出现该条 Error。

## 规则

"进程级静态持有 GPU 对象"必须在 `RenderSystem::Shutdown` 登记释放。新增静态持有者时，
同一处必须出现对应的释放调用。
