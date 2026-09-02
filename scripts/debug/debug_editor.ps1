# Run Editor under GDB for BUG-EDITOR-002 manual crash investigation.
# Usage:
#   .\scripts\debug\debug_editor.ps1                    # interactive (recommended)
#   .\scripts\debug\debug_editor.ps1 -Rhi vulkan
#   .\scripts\debug\debug_editor.ps1 -Batch -Loop 5     # auto-retry, log to bin/ed_gdb_bt.log
#   .\scripts\debug\debug_editor.ps1 -Batch -RunSeconds 20  # stop after 20s even if alive

param(
    [ValidateSet("opengl", "vulkan")]
    [string] $Rhi = "opengl",
    [int] $Loop = 1,
    [int] $RunSeconds = 0,
    [switch] $Batch
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$BinDir = Join-Path $RepoRoot "minEngine\bin"
$GdbDir = "D:\Dev\mingw64\bin"
$GdbOptDir = "D:\Dev\mingw64\opt\bin"
$GdbExe = Join-Path $GdbDir "gdborig.exe"
$Project = Join-Path $RepoRoot "minEngine\MyMEProject\MyMEProject.meproject"
$LogFile = Join-Path $BinDir "ed_gdb_bt.log"

if (-not (Test-Path $GdbExe)) {
    Write-Error "gdborig not found at $GdbExe — edit GdbDir in this script."
}
if (-not (Test-Path (Join-Path $GdbOptDir "libpython3.12.dll"))) {
    Write-Error "libpython3.12.dll missing under $GdbOptDir — add mingw64/opt/bin to PATH."
}
if (-not (Test-Path (Join-Path $BinDir "Editor.exe"))) {
    Write-Error "Editor.exe not found. Build: cmake --build minEngine/build --target Editor"
}
if (-not (Test-Path $Project)) {
    Write-Error "Project not found: $Project"
}

$env:PATH = "$GdbDir;$GdbOptDir;$BinDir;$env:PATH"
$env:PYTHONHOME = Join-Path (Split-Path $GdbDir -Parent) "opt"

function Invoke-Gdb {
    param([string[]] $GdbArgs)
    $prevEap = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        & $GdbExe @GdbArgs 2>&1 | ForEach-Object { "$_" }
    }
    finally {
        $ErrorActionPreference = $prevEap
    }
}

function Write-LogLine {
    param([string] $Line)
    Add-Content -Path $LogFile -Value $Line -Encoding utf8
}

$projGdb = ($Project -replace '\\', '/')
$genGdb = Join-Path $BinDir "gdb_editor_crash_run.gdb"
@"
set pagination off
set print thread-events on
set debuginfod enabled off
handle SIGSEGV stop print nopass
handle SIGABRT stop print nopass
file Editor.exe
set args --rhi $Rhi --project $projGdb
echo \n=== Editor GDB run (rhi=$Rhi) ===\n
run
echo \n=== backtrace ===\n
bt full
thread apply all bt full
info registers
quit
"@ | Set-Content -Path $genGdb -Encoding ASCII

Push-Location $BinDir
try {
    for ($i = 1; $i -le $Loop; $i++) {
        Write-Host "=== GDB run $i / $Loop (rhi=$Rhi) ===" -ForegroundColor Cyan
        if ($Batch) {
            $stamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
            Write-LogLine ""
            Write-LogLine "--- $stamp run $i rhi=$Rhi ---"

            if ($RunSeconds -gt 0) {
                $job = Start-Job -ScriptBlock {
                    param($Gdb, $Args)
                    & $Gdb @Args 2>&1 | ForEach-Object { "$_" }
                } -ArgumentList $GdbExe, @("-q", "-batch", "-x", $genGdb)

                $deadline = (Get-Date).AddSeconds($RunSeconds)
                $output = @()
                while ((Get-Date) -lt $deadline) {
                    Receive-Job $job -ErrorAction SilentlyContinue | ForEach-Object {
                        $output += $_
                        Write-LogLine $_
                    }
                    if ($job.State -eq "Completed") { break }
                    Start-Sleep -Milliseconds 200
                }
                if ($job.State -eq "Running") {
                    Stop-Job $job -Force
                    Write-LogLine "=== timed out after ${RunSeconds}s (no clean gdb exit) ==="
                    Write-Host "Timed out after ${RunSeconds}s — see log" -ForegroundColor Yellow
                }
                Remove-Job $job -Force -ErrorAction SilentlyContinue
            }
            else {
                $lines = Invoke-Gdb -GdbArgs @("-q", "-batch", "-x", $genGdb)
                foreach ($line in $lines) { Write-LogLine $line }
                $output = $lines
            }

            $tail = ($output | Select-Object -Last 40) -join "`n"
            if ($tail -match "SIGSEGV|received signal|Program received|EXCEPTION_ACCESS_VIOLATION|0xC0000005") {
                Write-Host "Crash captured — see $LogFile" -ForegroundColor Green
                break
            }
        }
        else {
            Write-Host "Interactive GDB. After crash: bt full" -ForegroundColor Yellow
            Write-Host "Re-run: run   Quit: quit" -ForegroundColor Yellow
            Invoke-Gdb -GdbArgs @("-q", "-x", $genGdb) | ForEach-Object { Write-Host $_ }
            break
        }
    }
}
finally {
    Pop-Location
}

Write-Host ""
Write-Host "Bin dir: $BinDir"
Write-Host "Log (batch): $LogFile"
Write-Host "Guide: docs/ai/bugs/DEBUG_EDITOR_MANUAL.md"
