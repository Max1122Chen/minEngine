# Fix submodule .git gitdir paths for linked worktrees (e.g. minEngine-physics).
# Submodule metadata lives in the main repo's .git/modules; worktree-relative paths break git status.
#
# Usage (from physics worktree root):
#   .\scripts\fix-worktree-submodule-gitdirs.ps1
#   .\scripts\fix-worktree-submodule-gitdirs.ps1 -MainGitRoot D:\Dev\GitRepo\minEngine

param(
    [string]$MainGitRoot = "D:\Dev\GitRepo\minEngine",
    [string]$ThirdPartyRoot = "minEngine\minEngine\Third-Party"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$modulesRoot = Join-Path $MainGitRoot ".git\modules\minEngine\minEngine\Third-Party"
$thirdPartyPath = Join-Path $repoRoot $ThirdPartyRoot

if (-not (Test-Path $modulesRoot)) {
    Write-Error "Modules root not found: $modulesRoot"
}

Get-ChildItem $thirdPartyPath -Directory | ForEach-Object {
    $gitFile = Join-Path $_.FullName ".git"
    if (-not (Test-Path $gitFile)) {
        return
    }
    $item = Get-Item $gitFile -Force
    if (-not $item.PSIsContainer) {
        $moduleName = $_.Name
        $target = Join-Path $modulesRoot $moduleName
        if (-not (Test-Path $target)) {
            Write-Warning "Skip $moduleName — module dir missing at $target"
            return
        }
        $content = "gitdir: $($target -replace '\\','/')"
        Set-Content -Path $gitFile -Value $content -NoNewline
        Write-Host "fixed $moduleName"
    }
}

Write-Host "Done. Run: git status"
