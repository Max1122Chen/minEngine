# 入口、启动与主循环

本文说明进程如何进入 minEngine、运行时如何初始化，以及**编辑器**与**引擎默认循环**的差异。日常开发以 **Editor** 为准；`minEngine/Playground/` 源码中已标明废弃。

## 进程入口

可执行文件（如 `Editor.exe`）链接引擎提供的 `main`，定义在 `minEngine/minEngine/src/main.h`：

```text
main
  → ApplicationCommandLine::TryParse
  →（Test 模式）ForwardToMinEngineTestsExecutable
  → CreateApplication()          // 由目标程序实现：Editor / Playground
  → Application::Initialize
  → Application::Run
  → Application::Shutdown
```

`CreateApplication()` **不在** Runtime 内实现，而在链接的目标中（例如 `Editor.cpp` 返回 `new Editor()`）。这是常见的「引擎库 + 客户端 Application」拆分，概念上类似 UE 里**可执行目标挂接模块**，但实现是 minEngine 自有代码。

## 命令行

`ApplicationCommandLine::TryParse` 产出 [CommandLineResult](../runtime/core/cli.md)：

| 模式 | 行为 |
|------|------|
| `Editor`（默认） | 继续启动 Application；Editor 要求 `--project <*.meproject>` |
| `Test` | 转交 `minEngineTests`，不创建 Editor |
| `--help` / `--version` | 打印后退出 |

路径解析见 [Paths](../runtime/core/paths.md)（`EngineConfig.meconfig`、工程 Content 等）。

## Engine 初始化

`Engine::Initialize`（`Runtime/Engine.cpp`）在设置单例 `Engine::Get()` 后大致顺序为：

1. **LogSystem::Initialize**
2. **ReflectionSystem::FinalizeReflection** — 注册期结束，失败则断言
3. **PathRegistry::LoadEngineConfiguration** — 填充 `EngineConfig`，标记 `m_EnginePathConfigLoaded`
4. **StartSystems** — 按固定顺序创建并 `Initialize` 各子系统单例：

```text
ObjectManager → ProjectManager → AssetManager
→ GLFWWindowSystem → FileDialogService → InputSystem
→ RenderSystem → SceneManager
```

5. 若路径配置成功，**RenderSystem::LoadEngineRenderingAssets**

窗口由 **GLFW** 实现（`GLFWWindowSystem`）；属于第三方依赖，见仓库 Third-Party。

## 引擎默认主循环

`Engine::Run()` 在 `WindowSystem::ShouldClose()` 为假时循环：

```text
每帧:
  CalculateDeltaTime()
  TickOneFrame(delta)
    → PollEvents()
    → TickLogicalFrame()   // InputSystem::Tick, SceneManager::Tick, SendAllEndOfFrameUpdates
    → TickRendererFrame()  // RenderSystem::Tick
  SwapBuffers()
```

`TickOneFrame` / `PollEvents` / `TickLogicalFrame` / `TickRendererFrame` 也可被外部（Editor）拆开调用，用于在逻辑与渲染之间插入 UI。

## Editor 主循环（当前产品路径）

`Editor` **不**调用 `Engine::Run()`，自行驱动循环（`Editor.cpp`），以便插入 ImGui 与编辑器模块：

```text
每帧:
  CalculateDeltaTime()
  PollEvents()
  TickLogicalFrame()              // 引擎逻辑
  SceneEditor::SyncSelectionWithScene()
  ActiveSubModule::Tick()         // 场景 / 材质等子编辑器
  UpdateWindowTitle()
  ImGui NewFrame + EditorGUIManager.Tick + …
  SceneManager::SendAllEndOfFrameUpdates()  // UI 可能再次标脏，补刷代理
  TickRendererFrame()
  ImGui::Render + 绘制 UI
  SwapBuffers()
```

要点：

- **Present Pass** 在 Editor 启动时被关闭（`SetPresentPassEnabled(false)`），最终呈现由 Editor 视口与 ImGui 合成负责。
- UI 栈为 **[Dear ImGui](https://github.com/ocornut/imgui)** + GLFW/OpenGL3 后端（第三方），与引擎 Runtime 解耦在 Editor 目标内。

启动时 Editor 在 `Engine::Initialize` 之后创建 ImGui 上下文、注册模块，并通过 `--project` 打开 `.meproject` 工程。

## 关闭顺序

- **Editor::Shutdown**：Editor 模块与 ImGui → `Engine::Shutdown`
- **Engine::Shutdown**：`ShutdownSystems()` 逆序释放（FileDialog → Project → Scene → Input → Render → Window → Asset → Object），并清空 `Engine` 单例

## 其它可执行目标

| 目标 | 说明 |
|------|------|
| **minEngineTests** | CLI `Test` 模式；不经过 `Engine::Run` 产品循环 |
| **Playground** | 旧测试 harness，文件头注明 **DEPRECATED**；仍调用 `Engine::Run()`，非维护路径 |

## 代码入口速查

| 主题 | 路径 |
|------|------|
| `main` | `minEngine/minEngine/src/main.h` |
| `Application` 接口 | `minEngine/minEngine/src/Application.h` |
| `Engine` | `minEngine/minEngine/src/Runtime/Engine.h`, `Engine.cpp` |
| `Editor` 启动/循环 | `minEngine/Editor/src/Editor.cpp` |
