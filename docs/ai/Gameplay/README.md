# Gameplay docs

Gameplay 机制设计（非强制 Gameplay Framework）。

| ID | 标题 | Status |
|----|------|--------|
| [GP-F01](./GP-F01_GAMEPLAY_TAG_DESIGN.md) | GameplayTag（含 Native 注册宏） | Done |
| [GP-F02](./GP-F02_GAMEPLAY_EVENT_SYSTEM_DESIGN.md) | GameplayEventSystem（Scene Component） | Planned |

代码：`Runtime/Function/GameplayFramework/`。  
验证：`minEngineTests.exe test gameplay-tags`。

哲学：机制进引擎基底；ASC / Pawn / 完整 GAS **不做**。
