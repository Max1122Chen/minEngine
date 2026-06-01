# CLI

命令行模块在进程入口**统一解析**参数，供 Editor、测试 runner、Playground 等模式共用。

## ApplicationCommandLine

- `TryParse(argc, argv)` → `std::optional<CommandLineResult>`。
- 解析失败或 `--help` 等场景通过 `CommandLineExitCode` / `GetLastExitCode()` 表达退出语义。

`CommandLineResult` 携带应用模式（普通运行 / 测试等）、引擎配置文件路径、工程路径等字段（详见 `CommandLineResult.h`）。

## 与其它 Core 模块

解析结果传入 `PathRegistry::LoadEngineConfiguration(const CommandLineResult&, …)`，再驱动资源与场景加载。

**入口：** `Runtime/Core/CLI/ApplicationCommandLine.h`, `CommandLineResult.h`
