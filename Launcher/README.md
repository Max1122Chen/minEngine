# minEngine Launcher

Rust CLI for opening and managing minEngine projects. GUI (Tauri 2) is planned for a later slice.

## Prerequisites

- [rustup](https://rustup.rs/) (stable toolchain)
- Built `Editor` executable under `minEngine/bin/`

If `minEngine/bin/` is empty in this worktree, copy from the main tree:

```powershell
$src = "D:\Dev\GitRepo\minEngine\minEngine\bin"
$dst = "D:\Dev\GitRepo\minEngine-launcher\minEngine\bin"
New-Item -ItemType Directory -Force -Path $dst | Out-Null
Copy-Item "$src\Editor.exe","$src\libminEngine.dll","$src\libassimp-6.dll" -Destination $dst
```

(`minEngine/bin/` is gitignored.)

### Rust mirror (optional, China network)

```powershell
$env:RUSTUP_DIST_SERVER = "https://rsproxy.cn"
$env:RUSTUP_UPDATE_ROOT = "https://rsproxy.cn/rustup"
rustup toolchain install stable --profile minimal
```

## Build

```powershell
cd Launcher
cargo build -p minlauncher
```

Release binary:

```powershell
cargo build -p minlauncher --release
```

## Quick start

```powershell
# From repo root (after building Editor)
cd Launcher
cargo run -p minlauncher -- config set editor ..\minEngine\bin\Editor.exe
cargo run -p minlauncher -- open ..\minEngine\MyMEProject\MyMEProject.meproject

# Create a project
cargo run -p minlauncher -- create DemoGame --parent ..\Projects
cargo run -p minlauncher -- recent list
```

## Environment variables

| Variable | Purpose |
|----------|---------|
| `MINENGINE_EDITOR` | Override Editor executable path |
| `MINENGINE_LAUNCHER_CONFIG` | Override settings directory |
| `MINENGINE_LAUNCHER_TEMPLATES` | Override `Templates/` root |

Settings file default: `%APPDATA%/minEngine/Launcher/settings.json` (Windows).

## Workspace layout

```text
Launcher/
  crates/minlauncher-core/   # shared library (future Tauri commands)
  crates/minlauncher/        # CLI binary
  Templates/Empty/           # default project template
```
