# 手动调试 Editor 启动崩溃（BUG-EDITOR-002）

## 前提
- 已 **Debug 构建**：`cmake --build minEngine/build --target Editor`（`Editor.exe` 含 DWARF，约 400MB+）
- MinGW GDB：`D:\Dev\mingw64\bin\gdborig.exe`
- **关键：** GDB 需要 `D:\Dev\mingw64\opt\bin` 在 `PATH`（`libpython3.12.dll`），并建议 `PYTHONHOME=D:\Dev\mingw64\opt`
- **构建前：** 若 `libminEngined.dll: Permission denied`，先结束所有 `Editor.exe`：
  ```powershell
  .\scripts\debug\kill_editor_processes.ps1   # 必要时「以管理员身份运行」PowerShell
  ```

## 方式 D — 内置崩溃日志（无需 GDB）

Editor 启动时会安装 `SetUnhandledExceptionFilter`，崩溃写入 **`minEngine/bin/ed_crash.log`**（与 `Editor.exe` 同目录）：

```
ExceptionCode=0xC0000005
ExceptionAddress=0x...
Stack (N frames):
  #0 0x...
Context RIP=0x... RSP=0x... RBP=0x...
```

符号化（Debug 构建，在 `minEngine/bin`）：

```powershell
$env:PATH = "D:\Dev\mingw64\bin;$env:PATH"
addr2line -f -C -e Editor.exe 0xADDRESS_FROM_LOG
```

将 `ed_crash.log` 片段贴到 BUG-EDITOR-002 调查记录。

## 方式 A — 推荐脚本（PowerShell）

```powershell
cd D:\Dev\GitRepo\minEngine

# ★ 交互（推荐）：崩溃后 gdb 停住，输入 bt full
.\scripts\debug\debug_editor.ps1

# Vulkan
.\scripts\debug\debug_editor.ps1 -Rhi vulkan

# 批量：最多 5 次，每次最多跑 20 秒（避免 gdb 一直挂着）
.\scripts\debug\debug_editor.ps1 -Batch -Loop 5 -RunSeconds 20
```

日志：`minEngine/bin/ed_gdb_bt.log`（UTF-8）。崩溃后请复制 **`=== backtrace ===` 以下** 到 BUG-EDITOR-002。

## 方式 B — 纯 GDB 命令行

```powershell
$env:PATH = "D:\Dev\mingw64\bin;D:\Dev\mingw64\opt\bin;D:\Dev\GitRepo\minEngine\minEngine\bin;$env:PATH"
$env:PYTHONHOME = "D:\Dev\mingw64\opt"
cd D:\Dev\GitRepo\minEngine\minEngine\bin

D:\Dev\mingw64\bin\gdborig.exe -q -x gdb_editor_crash.gdb
```

在 `(gdb)` 提示符下：
- `run` — 再跑一轮（改参数先 `set args ...`）
- `bt full` — 完整栈
- `thread apply all bt full` — 多线程
- `info registers` — 寄存器
- `quit`

## 方式 C — Cursor / VS Code（cppdbg）

### 前置（必做）

1. 在 Cursor 扩展市场安装 **C/C++**（Microsoft，`ms-vscode.cpptools`）。  
   `launch.json` 的 `"type": "cppdbg"` **依赖此扩展**；未安装会报 *Unable to establish a connection to GDB*。
2. 已 **Debug 构建** `Editor.exe`（见前提）。
3. 使用仓库根目录 `.vscode/launch.json` 中的 **Editor (GDB / MinGW)**。

### 启动

1. 运行与调试 → 选 **Editor (GDB / MinGW)** → F5  
2. 程序在 **集成终端** 启动（已关闭 `externalConsole`，避免 Windows 下 GDB 握手失败）。
3. 崩溃后调试控制台输入 `bt full` / `thread apply all bt full`。

### 若仍报 *Unable to establish a connection to GDB*

| 检查项 | 处理 |
|--------|------|
| 未装 C/C++ 扩展 | 安装 `ms-vscode.cpptools`，重载窗口 |
| `gdborig.exe` | 已改为 **`gdb.exe`**（`miDebuggerPath`） |
| `externalConsole` | 必须为 **false**（已配置） |
| GDB 缺 Python DLL | `PATH` 含 `mingw64/opt/bin`，`PYTHONHOME=D:/Dev/mingw64/opt`（已在 `environment`） |
| 仍失败 | 看 **调试控制台** 的 `engineLogging` 输出；或改用方式 A（脚本，最稳） |

配置见 `.vscode/launch.json`。

## 常见问题

| 现象 | 处理 |
|------|------|
| `gdborig` 一闪退出 / `0xC0000135` | `PATH` 加 `mingw64\bin` + `mingw64\opt\bin`；`PYTHONHOME=D:\Dev\mingw64\opt` |
| Batch 一直不结束 | 加 `-RunSeconds 20`；或改用**交互模式** |
| 栈里只有 `??` | 确认 Debug 构建、在 `minEngine\bin` 下启动 |
| 无符号 `libminEngined.dll` | 正常；看 **Editor.exe** 与带路径的帧即可 |
| 崩溃在 GPU 驱动 | 栈底可能是 `nvoglv64.dll` / `amdvlk`；往上找第一帧项目代码 |
| `Permission denied` 链接 DLL | 僵尸 `Editor.exe` 占用；运行 `kill_editor_processes.ps1` 或任务管理器结束 |
| GDB 无 backtrace | 优先看 `ed_crash.log`（方式 D） |
| Cursor *Unable to establish connection to GDB* | 安装 `ms-vscode.cpptools`；用 `gdb.exe` + `externalConsole: false`（见方式 C） |

## 退出码对照（无 GDB 时）

| 码 | 含义 |
|----|------|
| `-1073741819` (`0xC0000005`) | 访问违例 |
| `-1073741757` (`0xC000041D`) | 回调/异常链损坏（常为二次崩溃） |
