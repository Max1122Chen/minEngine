# engine-system-review — quick reference

Companion to `SKILL.md`. Keep the Skill body authoritative; this file is optional depth.

## False-positive traps

| Trap | Mitigation |
|------|------------|
| Preferring UE/Unity structure as “correct” | Judge against **this** repo’s contracts + philosophy |
| Stale Design checkboxes as “bugs” | Check Meta Status; code/tests win |
| Style / naming nit as Correctness | Maintainability or Informational only |
| Missing future feature as defect | Future Enhancement / Known Limitation |
| Editor-only convenience called Runtime bug | Confirm which path is in scope |
| Generated code “wrong” without generator source | Trace header-tool / generators first |

## Good first targets for 0.1.0

High leverage foundation systems: Serialization, Reflection, AssetManager, Object/GUID, Scene/GO/Component, Log, Delegates, Lua bridge, Editor document/session shell.

## After the report

1. User picks Must-Fix / Soon items  
2. File `BUG-*` or TECH_DEBT rows as appropriate  
3. Fix in slices with mentor Pre-flight if structural  
4. Optional verification pass with this skill (delta review)
