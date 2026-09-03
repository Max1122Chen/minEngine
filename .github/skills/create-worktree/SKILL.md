---
name: create-worktree
description: "Create or bootstrap a minEngine git worktree (or placeholder branch). Triggers on create worktree, bootstrap worktree, 创建 worktree, 初始化 worktree, feat branch worktree. Runs submodule init, fixes submodule gitdirs, copies libassimp (and optional binaries), rewrites MyMEProject ProjectRoot."
---

# Create / bootstrap a minEngine git worktree

## Meta script
`scripts/create-worktree.ps1` — prefer this script; skill documents when and how.

## Purpose

Create a linked git worktree for a feature branch with the checklist this repo actually needs: submodules, submodule gitdir fix, runtime DLLs, and `.meproject` `ProjectRoot` pointing at **this** worktree.

## When to use

- User asks to create a worktree / 创建 worktree / bootstrap worktree
- Starting a parallel track (`feat/animation`, `feat/ui`, …) that needs its own working tree
- Repairing an existing worktree that fails `git status` (submodule gitdirs) or Editor project path

**Do not** use full bootstrap for “placeholder branch only” — use `-BranchOnly`.

## Inputs

| Param | Meaning | Default |
|-------|---------|---------|
| Branch | e.g. `feat/animation` | required |
| WorktreePath | absolute path | `D:/Dev/GitRepo/minEngine-<suffix>` from branch name |
| BaseRef | commitish to branch from | `master` |
| MainGitRoot | primary repo | `D:/Dev/GitRepo/minEngine` |
| BranchOnly | create local branch, no worktree | off |
| SeedBinaries | copy `libassimp-6.dll` (+ optional Editor/libminEngine*) from Main | on for full init |
| SkipSubmodules | skip submodule init | off |

## Steps (full worktree)

1. **Confirm need** — Is a separate worktree justified (parallel Feature, long-lived track)? Prefer one worktree per active track; avoid nesting.
2. **Run script** from MainGitRoot:
   ```powershell
   .\scripts\create-worktree.ps1 -Branch feat/animation -WorktreePath D:\Dev\GitRepo\minEngine-animation
   ```
3. Script responsibilities (verify each succeeded):
   - `git worktree add -b <Branch> <Path> <BaseRef>` (or attach existing branch)
   - `git submodule update --init --recursive` inside worktree
   - `.\scripts\fix-worktree-submodule-gitdirs.ps1 -MainGitRoot <Main>`
   - Ensure `minEngine/bin` exists; copy at least **`libassimp-6.dll`** from Main `minEngine/bin` (CMake/link expects it there)
   - Optionally seed `Editor.exe` / `libminEngine*.dll` for smoke without rebuild (`-SeedBinaries`)
   - Rewrite `minEngine/MyMEProject/MyMEProject.meproject` → `ProjectRoot` = `<Worktree>/minEngine/MyMEProject` (forward slashes)
4. **Register** worktree in `docs/ai/ACTIVE_WORK.md` Worktrees table.
5. **Do not commit** machine-local `ProjectRoot` on a shared branch unless maintainer wants that machine’s path — prefer leave as local dirty or commit only on personal experiment branches.
6. Tell user next: configure/build in worktree (`cmake` + `Editor` / `minEngineTests`) if binaries were not seeded.

## Steps (placeholder branch only)

```powershell
.\scripts\create-worktree.ps1 -Branch feat/network -BranchOnly
```

Creates `git branch <Branch> <BaseRef>` if missing. No directory, no submodule, no DLL copy.

## Output format

Report:

- Branch name + whether newly created
- Worktree path (or BranchOnly)
- Submodule / gitdir fix OK?
- DLLs copied (list)
- New `ProjectRoot` value
- ACTIVE_WORK updated? (yes/no)
- Suggested first command in that worktree

## Pitfalls

- Submodule `.git` files in linked worktrees often point at wrong gitdir → always run `fix-worktree-submodule-gitdirs.ps1`.
- Forgetting `libassimp-6.dll` → Assimp CMake / Editor load fails.
- Leaving `ProjectRoot` on another worktree path → Editor opens wrong content / missing assets.
- Creating worktree from stale BaseRef — default `master`; confirm ahead/behind.
- Huge Debug `libminEngined.dll` copy is optional; prefer rebuild in worktree for day-to-day.
- Do not `git worktree remove` with uncommitted work without user confirmation.
