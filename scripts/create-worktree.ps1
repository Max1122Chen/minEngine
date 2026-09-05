# Create a linked worktree (or placeholder branch) for minEngine.
# Usage examples:
#   .\scripts\create-worktree.ps1 -Branch feat/animation -WorktreePath D:\Dev\GitRepo\minEngine-animation
#   .\scripts\create-worktree.ps1 -Branch feat/network -BranchOnly
#   .\scripts\create-worktree.ps1 -Branch feat/ui -SeedBinaries:$false

param(
    [Parameter(Mandatory = $true)]
    [string]$Branch,

    [string]$WorktreePath = "",

    [string]$BaseRef = "master",

    [string]$MainGitRoot = "D:\Dev\GitRepo\minEngine",

    [switch]$BranchOnly,

    [switch]$SkipSubmodules,

    # Copy libassimp-6.dll always when seeding; also Editor/libminEngine* when true.
    [bool]$SeedBinaries = $true,

    # Also copy large Debug DLL / Editor for quick smoke (can be slow / huge).
    [switch]$SeedHeavyBinaries
)

$ErrorActionPreference = "Stop"
# Git writes progress to stderr; do not treat native stderr as terminating errors (PS 7+).
if (Get-Variable -Name PSNativeCommandUseErrorActionPreference -ErrorAction SilentlyContinue) {
    $PSNativeCommandUseErrorActionPreference = $false
}

function Get-DefaultWorktreePath([string]$branchName) {
    $suffix = $branchName
    if ($suffix.StartsWith("feat/")) {
        $suffix = $suffix.Substring(5)
    }
    $suffix = $suffix -replace '[\\/]+', '-'
    return "D:\Dev\GitRepo\minEngine-$suffix"
}

function Ensure-BranchExists([string]$branchName, [string]$base) {
    git -C $MainGitRoot show-ref --verify --quiet "refs/heads/$branchName"
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Branch exists: $branchName"
        return $false
    }
    git -C $MainGitRoot branch $branchName $base
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to create branch $branchName from $base"
    }
    Write-Host "Created branch: $branchName <- $base"
    return $true
}

function Update-ProjectRoot([string]$worktreeRoot) {
    $meproject = Join-Path $worktreeRoot "minEngine\MyMEProject\MyMEProject.meproject"
    if (-not (Test-Path $meproject)) {
        Write-Warning "No .meproject at $meproject — skip ProjectRoot update"
        return $null
    }
    $projectRoot = (Join-Path $worktreeRoot "minEngine\MyMEProject") -replace '\\', '/'
    $json = Get-Content -Raw -Path $meproject | ConvertFrom-Json
    $json.ProjectRoot = $projectRoot
    $json | ConvertTo-Json -Depth 8 | Set-Content -Path $meproject -Encoding utf8
    Write-Host "ProjectRoot -> $projectRoot"
    return $projectRoot
}

function Copy-RuntimeBinaries([string]$worktreeRoot) {
    $srcBin = Join-Path $MainGitRoot "minEngine\bin"
    $dstBin = Join-Path $worktreeRoot "minEngine\bin"
    if (-not (Test-Path $srcBin)) {
        Write-Warning "Main bin missing: $srcBin — skip binary copy"
        return @()
    }
    New-Item -ItemType Directory -Force -Path $dstBin | Out-Null

    $copied = @()
    $required = @("libassimp-6.dll")
    foreach ($name in $required) {
        $src = Join-Path $srcBin $name
        if (Test-Path $src) {
            Copy-Item -Force $src (Join-Path $dstBin $name)
            $copied += $name
        }
        else {
            Write-Warning "Missing required DLL: $src"
        }
    }

    if ($SeedBinaries -or $SeedHeavyBinaries) {
        $light = @("libminEngine.dll", "libminEngine.dll.a", "Editor.exe", "minEngineTests.exe")
        foreach ($name in $light) {
            $src = Join-Path $srcBin $name
            if (Test-Path $src) {
                Copy-Item -Force $src (Join-Path $dstBin $name)
                $copied += $name
            }
        }
    }

    if ($SeedHeavyBinaries) {
        $heavy = @("libminEngined.dll", "libminEngined.dll.a")
        foreach ($name in $heavy) {
            $src = Join-Path $srcBin $name
            if (Test-Path $src) {
                Write-Host "Copying heavy binary $name ..."
                Copy-Item -Force $src (Join-Path $dstBin $name)
                $copied += $name
            }
        }
    }

    return $copied
}

Push-Location $MainGitRoot
try {
    git rev-parse --is-inside-work-tree | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "MainGitRoot is not a git repo: $MainGitRoot"
    }

    if ($BranchOnly) {
        $created = Ensure-BranchExists -branchName $Branch -base $BaseRef
        Write-Host "Done (BranchOnly). created=$created"
        return
    }

    if ([string]::IsNullOrWhiteSpace($WorktreePath)) {
        $WorktreePath = Get-DefaultWorktreePath $Branch
    }

    if (Test-Path $WorktreePath) {
        throw "Worktree path already exists: $WorktreePath"
    }

    $branchRef = "refs/heads/$Branch"
    git show-ref --verify --quiet $branchRef
    $branchExists = ($LASTEXITCODE -eq 0)

    if ($branchExists) {
        Write-Host "Adding worktree for existing branch $Branch -> $WorktreePath"
        git worktree add $WorktreePath $Branch
    }
    else {
        Write-Host "Adding worktree + new branch $Branch from $BaseRef -> $WorktreePath"
        git worktree add -b $Branch $WorktreePath $BaseRef
    }
    if ($LASTEXITCODE -ne 0) {
        throw "git worktree add failed"
    }

    if (-not $SkipSubmodules) {
        Write-Host "Initializing submodules..."
        git -C $WorktreePath submodule update --init --recursive
        if ($LASTEXITCODE -ne 0) {
            throw "submodule update failed"
        }

        $fixScript = Join-Path $WorktreePath "scripts\fix-worktree-submodule-gitdirs.ps1"
        if (Test-Path $fixScript) {
            & $fixScript -MainGitRoot $MainGitRoot
        }
        else {
            Write-Warning "fix-worktree-submodule-gitdirs.ps1 missing in worktree"
        }
    }

    $copied = Copy-RuntimeBinaries -worktreeRoot $WorktreePath
    $projectRoot = Update-ProjectRoot -worktreeRoot $WorktreePath

    Write-Host ""
    Write-Host "=== Worktree ready ==="
    Write-Host "Branch:      $Branch"
    Write-Host "Path:        $WorktreePath"
    Write-Host "Copied:      $($copied -join ', ')"
    Write-Host "ProjectRoot: $projectRoot"
    Write-Host "Next: update docs/ai/ACTIVE_WORK.md Worktrees table; cmake build in worktree if needed."
}
finally {
    Pop-Location
}
