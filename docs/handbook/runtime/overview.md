# Runtime

> 文档建设中。

**代码目录：** `minEngine/minEngine/src/Runtime/`

引擎运行时根目录，与源码一致分为四层：

| 层 | 说明 | 文档入口 |
|----|------|----------|
| **Core** | 对象、反射、序列化、数学、日志等 | [核心总览](core/overview.md) |
| **Function** | 场景/项目、输入、渲染等功能子系统 | 见左侧「功能层」 |
| **Platform** | 操作系统相关抽象 | [平台层](platform/file-dialog/overview.md) |
| **Resource** | 资产与加载 | [资产管理](resource/asset-manager/overview.md) |

进程入口、`Engine` 初始化与主循环见 [快速开始 → 启动与主循环](../getting-started/startup.md)（`Engine.h` 位于 `Runtime/` 根，非上表子目录）。
