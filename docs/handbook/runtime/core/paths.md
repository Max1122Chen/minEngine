# Paths

`PathRegistry` 在启动后持有**已解析的路径**，避免各子系统重复拼接引擎根、默认资产目录与工程 Content。

## 职责

| 路径 | 含义 |
|------|------|
| `EngineRoot` | 引擎安装/检出根目录 |
| `EngineConfigFilePath` | `EngineConfig.meconfig` 位置 |
| `EngineDefaultAssetsRoot` | 引擎自带默认资产（可 CLI 覆盖） |
| `ProjectRoot` / `ProjectContentRoot` | 打开的工程及其 Content |

提供 `ResolveEngineRelative`、`ResolveProjectRelative`，以及 `ResolvePathAgainstRoot`（配置里相对/绝对路径规则）。

## 启动协作

1. `ApplicationCommandLine::TryParse` 得到 `CommandLineResult`。
2. `PathRegistry::LoadEngineConfiguration` 发现配置、加载 JSON、填充 `EngineConfig` 与上述路径。
3. 上层 `Engine` / 资产管线再使用统一路径访问文件。

**入口：** `Runtime/Core/Paths/PathRegistry.h`  
（配置结构体定义在引擎配置相关头文件中，由加载流程一并填充。）
