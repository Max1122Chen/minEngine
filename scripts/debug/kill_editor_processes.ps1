# Force-stop all Editor.exe instances (fixes locked libminEngined.dll during build).
# Run from an elevated PowerShell if normal taskkill reports "Access denied".
#
# Usage:
#   .\scripts\debug\kill_editor_processes.ps1

$ErrorActionPreference = "Continue"

$processes = @(Get-Process -Name Editor -ErrorAction SilentlyContinue)
if ($processes.Count -eq 0) {
    Write-Host "No Editor.exe processes found."
    exit 0
}

Write-Host "Found $($processes.Count) Editor.exe process(es). Attempting to stop..."
$taskkill = Join-Path $env:SystemRoot "System32\taskkill.exe"

foreach ($proc in $processes) {
    try {
        Stop-Process -Id $proc.Id -Force -ErrorAction Stop
        Write-Host "  Stopped PID $($proc.Id)"
    }
    catch {
        Write-Warning "  Stop-Process failed for PID $($proc.Id): $($_.Exception.Message)"
        & $taskkill /F /PID $proc.Id 2>&1 | ForEach-Object { "    $_" }
    }
}

Start-Sleep -Seconds 2
$remaining = @(Get-Process -Name Editor -ErrorAction SilentlyContinue)
if ($remaining.Count -eq 0) {
    Write-Host "All Editor.exe processes stopped. You can rebuild now."
    exit 0
}

Write-Host ""
Write-Host "WARNING: $($remaining.Count) Editor.exe still running (likely elevated or zombie):"
$remaining | Format-Table Id, StartTime, Path -AutoSize
Write-Host "Try: Task Manager -> End task, or re-run this script as Administrator."
exit 1
