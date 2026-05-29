# Unified Command-Line Interface — Design Spec

## Meta
- **ID:** `CLI-F01`
- **Type:** Feature
- **Status:** Done (S01–S03; full suite migration → TEST-F01)
- **Owner:** project maintainer
- **Last updated:** 2026-05-28
- **Related:** [CLI_F01_IMPLEMENTATION.md](./CLI_F01_IMPLEMENTATION.md), [INFRASTRUCTURE_ROADMAP.md](../INFRASTRUCTURE_ROADMAP.md), [TECH_DEBT.md](../../TECH_DEBT.md) TD-001

## TL;DR

Introduce a **single CLI parse + dispatch** layer (CLI11 + thin Runtime wrapper): **subcommands switch mode**, **`--` options configure the current mode**. Replace scattered `ShouldRun*TestsOnly` argv scans and duplicate engine/project parsing. First milestone: `test` subcommand + global engine options; default mode remains **Editor**.

## Scope
- **In:** CLI11 vendoring; `ApplicationCommandLine`; global `--engine-config` / `--engine-root`; subcommand `test`; default Editor mode with `--project` / `-p`; unified `main.h` dispatch; migrate legacy `--*-test` flags (alias then remove per slice).
- **Out:** `TestRunner` suite registry internals (`TEST-F01`); `scripts/verify.ps1` (TEST-F01-S05); Playground entry; remote CI; changing test assertions.

## Reader quick start
1. This file — UX rules, modules, migration.
2. [CLI_F01_IMPLEMENTATION.md](./CLI_F01_IMPLEMENTATION.md) — slices S01–S03.
3. Code (planned): `Runtime/Core/CLI/`, `minEngine/src/main.h`, `Editor/src/Editor.cpp`.

---

## 1) 背景与目标

### Pain
- Five independent `--*-test` parsers in `*Test.cpp` + chain in `main.h`.
- `PathRegistry` and `Editor.cpp` each scan `argv` with different rules.
- No unified `--help`; agents and CI cannot discover commands from one place.

### Goals
- **One parse** per process start; **one help tree**.
- **Convention:** subcommand = **mode**; `--option` = **parameter for current mode**.
- **Professional, learnable:** align with Git/Cargo/dotnet style; keep engine-specific flags only where needed.
- **True refactor:** delete per-module argv loops as suites migrate (`TEST-F01` registers runners; CLI only dispatches).

### Success
```text
Editor.exe --help
Editor.exe --engine-config=path/to/EngineConfig.meconfig test smoke
Editor.exe --project path/to/My.meproject
Editor.exe test material-ir
```
Exit codes: `0` success, `1` test/runtime failure, `2` usage/parse error (documented in help).

---

## 2) 现状

| Location | Behavior |
|----------|----------|
| `minEngine/src/main.h` | Sequential `ShouldRun*TestsOnly` → early exit before `CreateApplication()` |
| `*Test.cpp` | Each defines its own flag string |
| `PathRegistry.cpp` | `ParsePrefixedArg` for `--engine-config=`, `--engine-root=` |
| `Editor.cpp` | `ParseProjectDescriptorPathFromArgs` for `--project` / `-p` / positional `.meproject` |
| Third-party | **No** C++ CLI library in engine targets today |

---

## 3) 方案

### 3.1 UX rules (binding)

| Layer | Rule | Examples |
|-------|------|----------|
| **Mode** | **Subcommand** (short verb/noun) | `test` |
| **Mode parameters** | **Long/short options** under that mode | `test --filter=…` (future) |
| **Global** | Options on **root** app (valid in all modes) | `--engine-config=`, `--engine-root=`, `--help`, `--version` |
| **Default mode** | No subcommand ⇒ **Editor** | `Editor.exe --project foo.meproject` |
| **Positionals** | Mode-specific | `test smoke`, `test full`, `test material-ir` |

**Not used as primary UX:** `--run-tests` as a long option for mode switch (may exist as **deprecated alias** one release only).

### 3.2 Command tree (v1)

```text
Editor.exe [global options] [command]

Global options:
  --engine-config=<path>     EngineConfig.meconfig (same semantics as today)
  --engine-root=<path>       Override engine root after config load
  -h, --help
  --version

Commands:
  (default) editor           GUI Editor — requires project descriptor
    --project <path>, -p <path>
    <positional .meproject>  if single non-option arg

  test                       Headless test mode — no Application window
    smoke                    Run smoke suite (fast)
    full                     Run all registered suites
    <suite-id>               Run one suite, e.g. material-ir, asset-manager, …
```

**Suite IDs (v1, stable strings):**

| Suite ID | Replaces legacy flag |
|----------|----------------------|
| `material-ir` | `--material-ir-test` |
| `asset-manager` | `--asset-manager-test` |
| `object-manager` | `--object-manager-test` |
| `serialization-archive` | `--serialization-archive-test` |
| `reflection-function` | `--reflection-function-test` / `=suite` (see below) |

**Reflection-function:**  
`test reflection-function` runs default suite set; optional **`--suite=<name>`** on `test` subcommand (or `test reflection-function --suite=name`) — exact spelling in Implementation; must support at least today’s `=suite` behavior.

### 3.3 Architecture

```text
┌─────────────────────────────────────────────────────────┐
│ ApplicationCommandLine::Parse(argc, argv)               │
│   CLI11::App rootApp                                    │
│   ├─ global: engine-config, engine-root, help, version  │
│   ├─ subcommand: test → TestDispatchConfig              │
│   └─ default: editor → EditorLaunchConfig               │
└───────────────────────────┬─────────────────────────────┘
                            │ CommandLineResult
            ┌───────────────┼───────────────┐
            ▼               ▼               ▼
     main.h dispatch   PathRegistry    Editor::Initialize
     (early test exit)  (from result,   (project path from
                        not re-scan)     result)
```

**Modules (Runtime):**

| File | Responsibility |
|------|----------------|
| `Runtime/Core/CLI/CommandLineResult.h` | Parsed mode + structs (`EditorLaunchConfig`, `TestDispatchConfig`) |
| `Runtime/Core/CLI/ApplicationCommandLine.h/.cpp` | Build CLI11 apps, parse, fill result |
| `Runtime/Core/CLI/CommandLineExitCode.h` | `Success`, `Failure`, `UsageError` |

**Third-party:** [CLI11](https://github.com/CLIUtils/CLI11) — **header-only**, vendored under `minEngine/Third-Party/CLI11/` (pin tag e.g. `v2.4.2` in Implementation commit). Wrapped so game code does not include CLI11 outside `ApplicationCommandLine.cpp`.

**Dispatch API:**

```cpp
// Pseudocode — names may adjust in implementation
enum class ApplicationMode { Editor, Test };

struct CommandLineResult {
    ApplicationMode mode;
    std::optional<std::filesystem::path> engineConfigPath;
    std::optional<std::filesystem::path> engineRootOverride;
    // Editor
    std::optional<std::filesystem::path> projectDescriptorPath;
    // Test
    enum class TestRunKind { Smoke, Full, SingleSuite };
    TestRunKind testRunKind;
    std::string suiteId;  // when SingleSuite
    std::optional<std::string> reflectionSuiteFilter;
};

class ApplicationCommandLine {
public:
    static std::optional<CommandLineResult> TryParse(int argc, char** argv);
    static int PrintHelpAndReturnUsageError(); // exit code 2
};
```

`main.h` flow:

1. `TryParse` → on failure print CLI11 message + return 2.
2. If `mode == Test` → `TestRunner::Run(result)` (stub calls existing `Run*Tests` until TEST-F01); return exit code.
3. Else → `CreateApplication()` → `Initialize` with `CommandLineResult` (Editor paths).

### 3.4 PathRegistry integration (refactor, not band-aid)

- **Remove** duplicate `ParsePrefixedArg` scans from call sites once CLI owns globals.
- `PathRegistry::LoadEngineConfiguration` gains overload or internal path: **`LoadEngineConfiguration(const CommandLineResult&)`** (or explicit paths from result).
- **ENV** `MINENGINE_ENGINE_CONFIG` / `MINENGINE_ENGINE_ROOT` behavior **unchanged** when CLI omits overrides (existing discovery order preserved inside PathRegistry).

### 3.5 Editor integration

- **Delete** `ParseProjectDescriptorPathFromArgs` loop; read `projectDescriptorPath` from `CommandLineResult`.
- Editor still **requires** a `.meproject` in default mode; error message points to `Editor.exe --help`.

### 3.6 Legacy flag migration

| Phase | Behavior |
|-------|----------|
| **S03** | If argv contains legacy `--material-ir-test`, print **stderr warning** and treat as `test material-ir` |
| **TEST-F01-S03** | Remove legacy parsers from each `*Test.cpp`; remove aliases |

No permanent dual API.

### 3.7 CMake / linkage

- `minEngine` static/shared lib: add `Runtime/Core/CLI/*.cpp`, include `Third-Party/CLI11/include`.
- `Editor` links `minEngine` — no separate CLI11 in Editor target.

---

## 4) 备选方案

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| **A. CLI11 + wrapper** | Subcommands, help, maintained | One more vendor dep | **选用** |
| B. Hand-rolled parser | No dep | Repeats today’s fragmentation | 拒绝 |
| C. cxxopts | Light | Weak subcommands | 拒绝 |
| D. `--run-tests` as mode flag | Minimal change | Violates mode/option convention | 仅作临时 alias |

---

## 5) 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| CLI11 include bleed | Long compile in Editor | Single .cpp owns CLI11 |
| Editor vs headless entry | Divergent parse | Only `ApplicationCommandLine` |
| Order of global vs subcommand flags | User confusion | Document; CLI11 allows globals before subcommand |
| Reflection suite `=` syntax | Break scripts | Alias period + doc table |
| Parse twice regression | Subtle bugs | PathRegistry must not scan argv after migration |

---

## 6) 验收标准

- [x] `Editor.exe --help` lists global options + `test` + default editor usage.
- [x] `Editor.exe test --help` lists smoke / full / suite IDs.
- [ ] `Editor.exe test smoke` runs existing smoke path (≥ material-ir) exit 0 on clean tree. *(TEST-F01)*
- [x] `Editor.exe --engine-config=<valid>` still resolves paths (log unchanged).
- [ ] `Editor.exe --project <path.meproject>` opens Editor (manual or existing workflow). *(manual — not re-run this slice)*
- [x] No `ShouldRunMaterialIRTestsOnly` in `main.h` after S03 (replaced by dispatch).
- [x] `PathRegistry` editor path uses `LoadEngineConfiguration(CommandLineResult)`; legacy test paths still delegate via thin argc wrapper.
- [x] Documented exit codes in this spec match implementation.

---

## 7) Status note

N/A (Draft).

---

## 变更记录

| 日期 | 说明 |
|------|------|
| 2026-05-28 | Initial draft; subcommand=mode, `--`=params; CLI11 |
| 2026-05-28 | Status → Planned (user approved) |
