# Log

日志系统对 **[spdlog](https://github.com/gabime/spdlog)** 做薄封装，提供引擎与应用程序两条 logger 通道。

## LogSystem

- `LogSystem::Initialize()` / `Shutdown()` 管理生命周期。
- `GetCoreLogger()` — 通道名 `MINENGINE`（`LogChannelNames::Core`）。
- `GetClientLogger()` — 通道名 `APP`（`LogChannelNames::Client`）。

## 宏

| 宏前缀 | 用途 |
|--------|------|
| `ME_CORE_TRACE` … `ME_CORE_CRITICAL` | 引擎内部 |
| `ME_TRACE` … `ME_CRITICAL` | 应用 / Playground |

实现上转发到对应 `spdlog::logger`。控制台输出等行为由 `LogSystem.cpp` / `LogConsole` 配置（若启用）。

**说明：** 格式、异步 sink、文件轮转等能力以 spdlog 为准；minEngine 未再实现一套日志后端。

**入口：** `Runtime/Core/Log/LogSystem.h`
