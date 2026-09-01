# minEngine Launcher

Rust **CLI** + **Tauri 2 GUI** for opening and managing minEngine projects.

## Prerequisites

- [rustup](https://rustup.rs/) (stable toolchain)
- Built `Editor` executable under `minEngine/bin/`
- **GUI only:** [Node.js](https://nodejs.org/) 18+ (for `ui/` frontend build / `tauri dev`)

If `minEngine/bin/` is empty in this worktree, copy from the main tree:

```powershell
$src = "D:\Dev\GitRepo\minEngine\minEngine\bin"
$dst = "D:\Dev\GitRepo\minEngine-launcher\minEngine\bin"
New-Item -ItemType Directory -Force -Path $dst | Out-Null
Copy-Item "$src\Editor.exe","$src\libminEngine.dll","$src\libminEngined.dll","$src\libassimp-6.dll" -Destination $dst
```

(`minEngine/bin/` is gitignored.)

### Rust mirror (optional, China network)

```powershell
$env:RUSTUP_DIST_SERVER = "https://rsproxy.cn"
$env:RUSTUP_UPDATE_ROOT = "https://rsproxy.cn/rustup"
rustup toolchain install stable --profile minimal
```

## Build

### CLI

```powershell
cd Launcher
cargo build -p minlauncher
```

### GUI (Tauri)

```powershell
cd Launcher/crates/minlauncher-app/ui
npm install
npm run build

cd ../../..
cargo build -p minlauncher-app
```

Binary: `Launcher/target/debug/minlauncher-gui.exe` (or `release/` after `--release`).

## Quick start (CLI)

```powershell
# From repo root (after building Editor)
cd Launcher
cargo run -p minlauncher -- config set editor ..\minEngine\bin\Editor.exe
cargo run -p minlauncher -- open ..\minEngine\MyMEProject\MyMEProject.meproject

# Create a project
cargo run -p minlauncher -- create DemoGame --parent ..\Projects
cargo run -p minlauncher -- recent list
```

## GUI dev

Install Tauri CLI once:

```powershell
cargo install tauri-cli --version "^2" --locked
```

Run with hot reload (starts Vite + WebView):

```powershell
cd Launcher/crates/minlauncher-app
cargo tauri dev
```

Or run the built app (uses pre-built `ui/dist`):

```powershell
cd Launcher
cargo run -p minlauncher-app --bin minlauncher-gui
```

## Environment variables

| Variable | Purpose |
|----------|---------|
| `MINENGINE_EDITOR` | Override Editor executable path |
| `MINENGINE_LAUNCHER_CONFIG` | Override settings directory |
| `MINENGINE_LAUNCHER_TEMPLATES` | Override `Templates/` root |

Settings file default: `%APPDATA%/minEngine/Launcher/settings.json` (Windows). CLI and GUI share the same file.

## Workspace layout

```text
Launcher/
  crates/minlauncher-core/   # shared library (CLI + Tauri commands)
  crates/minlauncher/        # CLI binary
  crates/minlauncher-app/    # Tauri 2 + React UI
    ui/                      # Vite + React + TypeScript
  Templates/Empty/           # default project template
```

## Tests

```powershell
cd Launcher
cargo test -p minlauncher-core
```
