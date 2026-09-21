# Extra / external test harness (TEST-F05)

Wire domain tests from an external game repo into `minEngineTests` without copying
play sources into the engine tree.

## CMake (engine configure)

| Cache variable | Purpose |
|----------------|---------|
| `MINENGINE_EXTRA_TEST_SOURCES` | Semicolon-separated `.cpp` list (must define `minEngine::RegisterExtraTestSuites`) |
| `MINENGINE_EXTRA_TEST_DIR` | Optional directory; all `*.cpp` under it are appended |
| `MINENGINE_EXTRA_TEST_INCLUDE_DIRS` | Extra includes |
| `MINENGINE_EXTRA_TEST_LINK_LIBRARIES` | Extra link targets (often the game SHARED) |

Empty / unset → behavior identical to today (`verify.ps1` smoke unchanged).

### Engine stub example

```powershell
cmake -S minEngine -B minEngine/build `
  -DMINENGINE_EXTRA_TEST_DIR=D:/Dev/GitRepo/minEngine/minEngine/Tests/Fixtures/ExtraTestStub

cmake --build minEngine/build --target minEngineTests
.\minEngine\bin\minEngineTests.exe test game.stub-extra
```

## Game-side contract

1. Provide a `.cpp` that implements:

```cpp
namespace minEngine
{
    void RegisterExtraTestSuites()
    {
        TestSuiteRegistry::Get().Register(/* your ITestSuite */);
    }
}
```

2. Suite `Id` **must** start with `game.` (example: `game.voxel-world`).
3. Set `InSmoke=false` unless you intentionally want `verify.ps1` to run it.
4. Name doctest `TEST_CASE` strings so they contain the suite id (DoctestSuiteRunner filters by substring).
5. **Never** `#define DOCTEST_CONFIG_IMPLEMENT` in EXTRA sources — only `Tests/TestMain.cpp` may.

## Related

- Design: `docs/ai/Platform/Test/TEST-F05_EXTERNAL_TEST_HARNESS_DESIGN.md`
- Editor Dev host link: `docs/ai/Editor/ED-F20_OPTIONAL_GAME_MODULE_LINK_DESIGN.md`
