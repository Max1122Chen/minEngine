# Assert

调试断言由引擎自有宏 **`ME_ASSERT(expr, message)`** 提供（`Assert.h`），**非**第三方 assert 库。

- `ME_ASSERT_ENABLED` 为 `1` 时：条件为假则向 `stderr` 打印文件/行号与消息，并调用 `__debugbreak()`（便于挂调试器）。
- 关闭后宏为空操作，便于发布构建裁剪。

用于不变量检查；错误处理与日志请使用 [Log](log.md) 与正常返回路径。

**入口：** `Runtime/Core/Assert/Assert.h`
