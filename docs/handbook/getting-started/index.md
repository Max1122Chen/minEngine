# 快速开始

## 运行 Editor（常用）

1. 构建 `Editor` 目标（CMake 生成 `minEngine/build` 等，以你本地配置为准）。
2. 准备工程描述文件 `*.meproject`。
3. 从 `minEngine/bin`（或构建输出目录）启动：

```text
Editor.exe --project <path-to-project.meproject>
```

可选：`--engine-config` 指定 `EngineConfig.meconfig`；详见 [CLI](../runtime/core/cli.md)。

## 文档

- [入口、启动与主循环](startup.md) — `main`、`Engine` 初始化顺序、Editor 与默认循环对比
- [Core 层](../runtime/core/overview.md) — 路径、日志、对象与反射

## 验证

仓库根目录：

```powershell
.\scripts\verify.ps1
```

用于本地构建与 smoke 测试（不替代完整 Editor 手工冒烟）。

## 代码布局

| 目录 | 说明 |
|------|------|
| `minEngine/minEngine/` | Runtime 与 `main.h` |
| `minEngine/Editor/` | 编辑器 Application（`CreateApplication`） |
| `minEngine/Playground/` | 遗留示例，**非**日常入口 |
