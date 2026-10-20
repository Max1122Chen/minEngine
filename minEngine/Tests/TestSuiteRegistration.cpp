#include "TestSuites.h"

#include "DoctestSuiteRunner.h"
#include "EngineTestFixture.h"
#include "ExtraTestRegistration.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Test/ITestSuite.h"
#include "Runtime/Test/TestContext.h"
#include "Runtime/Test/TestSuiteRegistry.h"

#include "Suites/SerializationArchiveTest.h"
#include "Suites/AssetManagerTest.h"
#include "Suites/DataAssetTest.h"
#include "Suites/AssetRefTypeTest.h"
#include "Suites/ScenePropertyRefTypeTest.h"
#include "Suites/ReflectionFunctionTest.h"
#include "Suites/MaterialIRTest.h"
#include "Suites/LuaScriptMvpTest.h"
#include "Suites/PhysicsSmokeTest.h"
#include "Suites/PhysicsSyncTest.h"
#include "Suites/PhysicsLoadTest.h"
#include "Suites/PhysicsContactTest.h"
#include "Suites/PhysicsLineTraceTest.h"
#include "Suites/PhysicsShapesTest.h"
#include "Suites/DelegateTest.h"
#include "Suites/ShaderCompilerTest.h"
#include "Suites/AudioSmokeTest.h"
#include "Suites/SceneCloneTest.h"
#include "Suites/ReflectionDisplayNamesTest.h"
#include "Suites/SpriteTranslucencyTest.h"
#include "Suites/ScreenUICoordsTest.h"
#include "Suites/ScreenUIGameViewportTest.h"
#include "Suites/ScreenUIOrientationTest.h"
#include "Suites/FrustumTest.h"
#include "Suites/NoiseTest.h"
#include "Suites/VertexAttributeSemanticsTest.h"
#include "Suites/BeginPlayTest.h"
#include "Suites/DeferredStructuralEditTest.h"
#include "Suites/GameplayInputGateTest.h"
#include "Suites/InputTriggerDownTest.h"
#include "Suites/InputKeyCatalogTest.h"
#include "Suites/PlayMainCameraTest.h"
#include "Suites/AutomationPipeInterruptTest.h"
#include "Suites/MeshDrawSortTest.h"
#include "Suites/UILayoutTest.h"
#include "Suites/ScreenUIHitTest.h"
#include "Suites/ScreenUIButtonTest.h"
#include "Suites/ScreenUIPointerTest.h"
#include "Suites/PointerPolicyTest.h"
#include "Suites/ScreenUIDragTest.h"
#include "Suites/ScreenUIImageTest.h"
#include "Suites/ScreenUITextTest.h"
#include "Suites/GameObjectHierarchyTest.h"
#include "Suites/CommandSystemTest.h"
#include "Suites/SkeletonPoseTest.h"
#include "Suites/AnimationClipTest.h"
#include "Suites/AnimationGraphTest.h"
#include "Suites/ParameterStoreTest.h"
#include "Suites/LoggingChannelsTest.h"
#include "Suites/EngineVersionTest.h"
#include "Suites/ProfilerTest.h"
#include "Suites/PrefabTest.h"
#include "Suites/PrefabOverridesTest.h"
#include "Suites/SpriteAtlasTest.h"
#include "Suites/SpriteRuntimeConsumeTest.h"
#include "Suites/SpriteAuthoringTest.h"
#include "Suites/MaterialCompileCacheTest.h"

namespace minEngine
{
    namespace
    {
        template <typename SuiteType>
        class TypedTestSuite : public ITestSuite
        {
        public:
            static TypedTestSuite& Get()
            {
                static TypedTestSuite instance;
                return instance;
            }

            TestSuiteMetadata GetMetadata() const override { return SuiteType::BuildMetadata(); }
            bool Run(TestContext& context) override { return SuiteType::RunSuite(context); }
        };

        struct ObjectManagerTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"object-manager", "Object Manager", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'object-manager'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext("object-manager", context.GetCommandLine().TestKind);
            }
        };

        struct SerializationArchiveTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"serialization-archive", "Serialization Archive", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'serialization-archive'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext("serialization-archive", context.GetCommandLine().TestKind);
            }
        };

        struct AssetManagerTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"asset-manager", "Asset Manager", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'asset-manager'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext("asset-manager", context.GetCommandLine().TestKind);
            }
        };

        struct DataAssetTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"data-asset", "Data Asset", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'data-asset'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext("data-asset", context.GetCommandLine().TestKind);
            }
        };

        struct AssetRefTypeTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"asset-ref-type", "Asset Reference Type", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'asset-ref-type'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext("asset-ref-type", context.GetCommandLine().TestKind);
            }
        };

        struct ScenePropertyRefTypeTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"scene-property-ref-type", "Scene Property Reference Type", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'scene-property-ref-type'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext("scene-property-ref-type", context.GetCommandLine().TestKind);
            }
        };

        struct ReflectionFunctionTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"reflection-function", "Reflection Function", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'reflection-function'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext("reflection-function", context.GetCommandLine().TestKind);
            }
        };

        struct MaterialIRTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"material-ir", "Material IR", true, true, true};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'material-ir'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext("material-ir", context.GetCommandLine().TestKind);
            }
        };

        struct RenderGraphTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"render-graph", "Render Graph", false, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'render-graph'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext("render-graph", context.GetCommandLine().TestKind);
            }
        };

        struct LuaScriptMvpTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"lua-script-mvp", "Lua Script MVP", false, false, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'lua-script-mvp'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext("lua-script-mvp", context.GetCommandLine().TestKind);
            }
        };

        struct PhysicsSmokeTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"physics-smoke", "Physics Smoke", false, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'physics-smoke'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext("physics-smoke", context.GetCommandLine().TestKind);
            }
        };

        struct PhysicsSyncTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"physics-sync", "Physics Sync", false, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'physics-sync'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext("physics-sync", context.GetCommandLine().TestKind);
            }
        };

        struct PhysicsLoadTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"physics-load", "Physics Load", false, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'physics-load'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext("physics-load", context.GetCommandLine().TestKind);
            }
        };

        struct PhysicsContactTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"physics-contact", "Physics Contact", false, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'physics-contact'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext("physics-contact", context.GetCommandLine().TestKind);
            }
        };

        struct PhysicsLineTraceTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"physics-linetrace", "Physics LineTrace", false, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'physics-linetrace'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext("physics-linetrace", context.GetCommandLine().TestKind);
            }
        };

        struct PhysicsShapesTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"physics-shapes", "Physics Shapes", false, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'physics-shapes'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext("physics-shapes", context.GetCommandLine().TestKind);
            }
        };

        struct DelegateTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                // Not in smoke: native unit suite; run via `test delegates` or full.
                return TestSuiteMetadata{"delegates", "Native + Dynamic Multicast Delegates", false, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'delegates'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext("delegates", context.GetCommandLine().TestKind);
            }
        };

        struct GameplayTagTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"gameplay-tags", "Gameplay Tags", false, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'gameplay-tags'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext("gameplay-tags", context.GetCommandLine().TestKind);
            }
        };

        struct GameplayEventTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"gameplay-events", "Gameplay Event System", false, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'gameplay-events'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext("gameplay-events", context.GetCommandLine().TestKind);
            }
        };

        struct ShaderCompilerTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"shader-compiler", "ShaderCompiler SPIR-V", false, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'shader-compiler'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext("shader-compiler", context.GetCommandLine().TestKind);
            }
        };

        struct SceneCloneTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"scene-clone", "Scene Clone", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'scene-clone'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext("scene-clone", context.GetCommandLine().TestKind);
            }
        };

        struct AudioSmokeTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"audio-smoke", "Audio Smoke", false, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'audio-smoke'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext("audio-smoke", context.GetCommandLine().TestKind);
            }
        };

        struct ReflectionDisplayNamesTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{
                    "reflection-display-names",
                    "Reflection Display Names",
                    true,
                    true,
                    false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'reflection-display-names'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "reflection-display-names",
                    context.GetCommandLine().TestKind);
            }
        };

        struct SpriteTranslucencyTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"sprite-translucency", "Sprite Translucency", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'sprite-translucency'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "sprite-translucency",
                    context.GetCommandLine().TestKind);
            }
        };

        struct ScreenUICoordsTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"screen-ui-coords", "Screen UI Coords", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'screen-ui-coords'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "screen-ui-coords",
                    context.GetCommandLine().TestKind);
            }
        };

        struct ScreenUIGameViewportTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{
                    "screen-ui-game-viewport",
                    "Screen UI game-host viewport / pointer reads",
                    true,
                    true,
                    false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'screen-ui-game-viewport'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "screen-ui-game-viewport",
                    context.GetCommandLine().TestKind);
            }
        };

        struct ScreenUIOrientationTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"screen-ui-orientation", "Screen UI Orientation", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'screen-ui-orientation'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "screen-ui-orientation",
                    context.GetCommandLine().TestKind);
            }
        };

        struct FrustumTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"frustum", "Frustum Math", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'frustum'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "frustum",
                    context.GetCommandLine().TestKind);
            }
        };

        struct NoiseTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"noise", "Noise Math", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'noise'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "noise",
                    context.GetCommandLine().TestKind);
            }
        };

        struct VertexAttributeSemanticsTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{
                    "vertex-attribute-semantics",
                    "Canonical mesh vertex attribute locations",
                    true,
                    true,
                    false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'vertex-attribute-semantics'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "vertex-attribute-semantics",
                    context.GetCommandLine().TestKind);
            }
        };

        struct BeginPlayTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"begin-play", "BeginPlay / EndPlay", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'begin-play'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "begin-play",
                    context.GetCommandLine().TestKind);
            }
        };

        struct DeferredStructuralEditTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{
                    "deferred-structural-edit",
                    "Deferred Structural Edit",
                    true,
                    true,
                    false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'deferred-structural-edit'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "deferred-structural-edit",
                    context.GetCommandLine().TestKind);
            }
        };

        struct GameplayInputGateTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"gameplay-input-gate", "Gameplay Input Gate", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'gameplay-input-gate'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "gameplay-input-gate",
                    context.GetCommandLine().TestKind);
            }
        };

        struct InputTriggerDownTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"input-trigger-down", "Input Trigger Down Hold Events", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'input-trigger-down'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "input-trigger-down",
                    context.GetCommandLine().TestKind);
            }
        };

        struct InputKeyCatalogTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"input-key-catalog", "Input Key Catalog / Synthetic Modifiers", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'input-key-catalog'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "input-key-catalog",
                    context.GetCommandLine().TestKind);
            }
        };

        struct PlayMainCameraTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"play-main-camera", "Play Main Camera", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'play-main-camera'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "play-main-camera",
                    context.GetCommandLine().TestKind);
            }
        };

        struct AutomationPipeInterruptTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"automation-pipe-interrupt", "Automation Pipe Interrupt", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'automation-pipe-interrupt'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "automation-pipe-interrupt",
                    context.GetCommandLine().TestKind);
            }
        };

        struct MeshDrawSortTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"mesh-draw-sort", "Mesh Draw Sort", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'mesh-draw-sort'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "mesh-draw-sort",
                    context.GetCommandLine().TestKind);
            }
        };


        struct Texture2DCookTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"texture2d-cook", "Texture2D Cook (.metex)", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'texture2d-cook'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "texture2d-cook",
                    context.GetCommandLine().TestKind);
            }
        };

        struct SpriteAtlasTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"sprite-atlas", "Sprite Atlas Asset", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'sprite-atlas'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "sprite-atlas",
                    context.GetCommandLine().TestKind);
            }
        };

        struct SpriteRuntimeConsumeTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"sprite-runtime-consume", "Sprite Runtime Consume", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'sprite-runtime-consume'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "sprite-runtime-consume",
                    context.GetCommandLine().TestKind);
            }
        };

        struct SpriteAuthoringTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"sprite-authoring", "Sprite Authoring Services", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'sprite-authoring'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "sprite-authoring",
                    context.GetCommandLine().TestKind);
            }
        };

        struct GameModuleStubReflectionTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                // Not in default smoke: needs GameModuleStub linked (BUILD_GAME_MODULE_SAMPLE).
                return TestSuiteMetadata{"game-module-stub", "Game Module Stub Reflection", false, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'game-module-stub'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "game-module-stub",
                    context.GetCommandLine().TestKind);
            }
        };

        struct UILayoutTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"ui-layout", "UI Layout", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'ui-layout'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "ui-layout",
                    context.GetCommandLine().TestKind);
            }
        };

        struct ScreenUIHitTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"screen-ui-hit", "Screen UI Hit-test", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'screen-ui-hit'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "screen-ui-hit",
                    context.GetCommandLine().TestKind);
            }
        };

        struct ScreenUIButtonTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"screen-ui-button", "Screen UI Button", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'screen-ui-button'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "screen-ui-button",
                    context.GetCommandLine().TestKind);
            }
        };

        struct ScreenUIPointerTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"screen-ui-pointer", "Screen UI Widget Pointer", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'screen-ui-pointer'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "screen-ui-pointer",
                    context.GetCommandLine().TestKind);
            }
        };

        struct PointerPolicyTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"pointer-policy", "Pointer Policy (focus-scoped constraints)", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'pointer-policy'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "pointer-policy",
                    context.GetCommandLine().TestKind);
            }
        };

        struct ScreenUIDragTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"screen-ui-drag", "Screen UI Drag-Drop", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'screen-ui-drag'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "screen-ui-drag",
                    context.GetCommandLine().TestKind);
            }
        };

        struct ScreenUIImageTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"screen-ui-image", "Screen UI Image Sprite/Raw", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'screen-ui-image'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "screen-ui-image",
                    context.GetCommandLine().TestKind);
            }
        };

        struct ScreenUITextTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"screen-ui-text", "Screen UI Text", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'screen-ui-text'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "screen-ui-text",
                    context.GetCommandLine().TestKind);
            }
        };

        struct GameObjectHierarchyTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"gameobject-hierarchy", "GameObject Hierarchy", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'gameobject-hierarchy'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "gameobject-hierarchy",
                    context.GetCommandLine().TestKind);
            }
        };

        struct CommandSystemTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"command-system", "Command System", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'command-system'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "command-system",
                    context.GetCommandLine().TestKind);
            }
        };

        struct SkeletonPoseTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"skeleton-pose", "Skeleton Pose", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'skeleton-pose'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "skeleton-pose",
                    context.GetCommandLine().TestKind);
            }
        };

        struct AnimationClipTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"animation-clip", "Animation Clip", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'animation-clip'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "animation-clip",
                    context.GetCommandLine().TestKind);
            }
        };

        struct AnimationGraphTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"animation-graph", "Animation Graph", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'animation-graph'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "animation-graph",
                    context.GetCommandLine().TestKind);
            }
        };

        struct ParameterStoreTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                // Unit suite; smoke runs the empty-schema case via [smoke] filter.
                return TestSuiteMetadata{"parameter-store", "Parameter Schema/Layout/Store", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'parameter-store'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "parameter-store",
                    context.GetCommandLine().TestKind);
            }
        };

        struct LoggingChannelsTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"logging-channels", "LogChannel / LogRecord", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'logging-channels'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "logging-channels",
                    context.GetCommandLine().TestKind);
            }
        };

        struct EngineVersionTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"engine-version", "EngineVersion / disk schema", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'engine-version'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "engine-version",
                    context.GetCommandLine().TestKind);
            }
        };

        struct ProfilerTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"profiler", "CPU Profiler Harness", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'profiler'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext("profiler", context.GetCommandLine().TestKind);
            }
        };

        struct PrefabTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"prefab", "Prefab Asset + Instantiate", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'prefab'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext("prefab", context.GetCommandLine().TestKind);
            }
        };

        struct PrefabOverridesTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"prefab-overrides", "Prefab Overrides + Propagation", true, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'prefab-overrides'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext("prefab-overrides", context.GetCommandLine().TestKind);
            }
        };

        struct GameInstanceSubsystemTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{
                    "game-instance-subsystem",
                    "GameInstance / SubsystemCollection",
                    true,
                    true,
                    false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'game-instance-subsystem'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "game-instance-subsystem",
                    context.GetCommandLine().TestKind);
            }
        };

        struct WorldSubsystemTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{
                    "world-subsystem",
                    "World Subsystem / Scene Collection",
                    true,
                    true,
                    false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'world-subsystem'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "world-subsystem",
                    context.GetCommandLine().TestKind);
            }
        };

        struct TickableSubsystemTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{
                    "tickable-subsystem",
                    "Tickable World / GI Subsystems",
                    true,
                    true,
                    false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'tickable-subsystem'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "tickable-subsystem",
                    context.GetCommandLine().TestKind);
            }
        };

        struct GameplayFixedTickTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{
                    "gameplay-fixed-tick",
                    "Gameplay FixedTick clock and phase dispatch",
                    true,
                    true,
                    false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'gameplay-fixed-tick'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "gameplay-fixed-tick",
                    context.GetCommandLine().TestKind);
            }
        };

        struct MaterialCompileCacheTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{
                    "material-compile-cache",
                    "Material compile / program cache",
                    false,
                    true,
                    true};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'material-compile-cache'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "material-compile-cache",
                    context.GetCommandLine().TestKind);
            }
        };

        using ObjectManagerSuite = TypedTestSuite<ObjectManagerTestSuiteTraits>;
        using SerializationArchiveSuite = TypedTestSuite<SerializationArchiveTestSuiteTraits>;
        using AssetManagerSuite = TypedTestSuite<AssetManagerTestSuiteTraits>;
        using DataAssetSuite = TypedTestSuite<DataAssetTestSuiteTraits>;
        using AssetRefTypeSuite = TypedTestSuite<AssetRefTypeTestSuiteTraits>;
        using ScenePropertyRefTypeSuite = TypedTestSuite<ScenePropertyRefTypeTestSuiteTraits>;
        using ReflectionFunctionSuite = TypedTestSuite<ReflectionFunctionTestSuiteTraits>;
        using MaterialIRSuite = TypedTestSuite<MaterialIRTestSuiteTraits>;
        using RenderGraphSuite = TypedTestSuite<RenderGraphTestSuiteTraits>;
        using LuaScriptMvpSuite = TypedTestSuite<LuaScriptMvpTestSuiteTraits>;
        using PhysicsSmokeSuite = TypedTestSuite<PhysicsSmokeTestSuiteTraits>;
        using PhysicsSyncSuite = TypedTestSuite<PhysicsSyncTestSuiteTraits>;
        using PhysicsLoadSuite = TypedTestSuite<PhysicsLoadTestSuiteTraits>;
        using PhysicsContactSuite = TypedTestSuite<PhysicsContactTestSuiteTraits>;
        using PhysicsLineTraceSuite = TypedTestSuite<PhysicsLineTraceTestSuiteTraits>;
        using PhysicsShapesSuite = TypedTestSuite<PhysicsShapesTestSuiteTraits>;
        using DelegateSuite = TypedTestSuite<DelegateTestSuiteTraits>;
        using GameplayTagSuite = TypedTestSuite<GameplayTagTestSuiteTraits>;
        using GameplayEventSuite = TypedTestSuite<GameplayEventTestSuiteTraits>;
        using ShaderCompilerSuite = TypedTestSuite<ShaderCompilerTestSuiteTraits>;
        using AudioSmokeSuite = TypedTestSuite<AudioSmokeTestSuiteTraits>;
        using SceneCloneSuite = TypedTestSuite<SceneCloneTestSuiteTraits>;
        using ReflectionDisplayNamesSuite = TypedTestSuite<ReflectionDisplayNamesTestSuiteTraits>;
        using SpriteTranslucencySuite = TypedTestSuite<SpriteTranslucencyTestSuiteTraits>;
        using ScreenUICoordsSuite = TypedTestSuite<ScreenUICoordsTestSuiteTraits>;
        using ScreenUIGameViewportSuite = TypedTestSuite<ScreenUIGameViewportTestSuiteTraits>;
        using ScreenUIOrientationSuite = TypedTestSuite<ScreenUIOrientationTestSuiteTraits>;
        using FrustumSuite = TypedTestSuite<FrustumTestSuiteTraits>;
        using NoiseSuite = TypedTestSuite<NoiseTestSuiteTraits>;
        using VertexAttributeSemanticsSuite = TypedTestSuite<VertexAttributeSemanticsTestSuiteTraits>;
        using BeginPlaySuite = TypedTestSuite<BeginPlayTestSuiteTraits>;
        using DeferredStructuralEditSuite = TypedTestSuite<DeferredStructuralEditTestSuiteTraits>;
        using GameplayInputGateSuite = TypedTestSuite<GameplayInputGateTestSuiteTraits>;
        using InputTriggerDownSuite = TypedTestSuite<InputTriggerDownTestSuiteTraits>;
        using InputKeyCatalogSuite = TypedTestSuite<InputKeyCatalogTestSuiteTraits>;
        using PlayMainCameraSuite = TypedTestSuite<PlayMainCameraTestSuiteTraits>;
        using AutomationPipeInterruptSuite = TypedTestSuite<AutomationPipeInterruptTestSuiteTraits>;
        using MeshDrawSortSuite = TypedTestSuite<MeshDrawSortTestSuiteTraits>;
        using Texture2DCookSuite = TypedTestSuite<Texture2DCookTestSuiteTraits>;
        using SpriteAtlasSuite = TypedTestSuite<SpriteAtlasTestSuiteTraits>;
        using SpriteRuntimeConsumeSuite = TypedTestSuite<SpriteRuntimeConsumeTestSuiteTraits>;
        using SpriteAuthoringSuite = TypedTestSuite<SpriteAuthoringTestSuiteTraits>;
        using GameModuleStubReflectionSuite = TypedTestSuite<GameModuleStubReflectionTestSuiteTraits>;
        using UILayoutSuite = TypedTestSuite<UILayoutTestSuiteTraits>;
        using ScreenUIHitSuite = TypedTestSuite<ScreenUIHitTestSuiteTraits>;
        using ScreenUIButtonSuite = TypedTestSuite<ScreenUIButtonTestSuiteTraits>;
        using ScreenUIPointerSuite = TypedTestSuite<ScreenUIPointerTestSuiteTraits>;
        using PointerPolicySuite = TypedTestSuite<PointerPolicyTestSuiteTraits>;
        using ScreenUIDragSuite = TypedTestSuite<ScreenUIDragTestSuiteTraits>;
        using ScreenUIImageSuite = TypedTestSuite<ScreenUIImageTestSuiteTraits>;
        using ScreenUITextSuite = TypedTestSuite<ScreenUITextTestSuiteTraits>;
        using GameObjectHierarchySuite = TypedTestSuite<GameObjectHierarchyTestSuiteTraits>;
        using CommandSystemSuite = TypedTestSuite<CommandSystemTestSuiteTraits>;
        using SkeletonPoseSuite = TypedTestSuite<SkeletonPoseTestSuiteTraits>;
        using AnimationClipSuite = TypedTestSuite<AnimationClipTestSuiteTraits>;
        using AnimationGraphSuite = TypedTestSuite<AnimationGraphTestSuiteTraits>;
        using ParameterStoreSuite = TypedTestSuite<ParameterStoreTestSuiteTraits>;
        using LoggingChannelsSuite = TypedTestSuite<LoggingChannelsTestSuiteTraits>;
        using EngineVersionSuite = TypedTestSuite<EngineVersionTestSuiteTraits>;
        using ProfilerSuite = TypedTestSuite<ProfilerTestSuiteTraits>;
        using PrefabSuite = TypedTestSuite<PrefabTestSuiteTraits>;
        using PrefabOverridesSuite = TypedTestSuite<PrefabOverridesTestSuiteTraits>;
        using GameInstanceSubsystemSuite = TypedTestSuite<GameInstanceSubsystemTestSuiteTraits>;
        using WorldSubsystemSuite = TypedTestSuite<WorldSubsystemTestSuiteTraits>;
        using TickableSubsystemSuite = TypedTestSuite<TickableSubsystemTestSuiteTraits>;
        using GameplayFixedTickSuite = TypedTestSuite<GameplayFixedTickTestSuiteTraits>;
        using MaterialCompileCacheSuite = TypedTestSuite<MaterialCompileCacheTestSuiteTraits>;

        void RegisterAllTestSuites()
        {
            static bool s_Registered = false;
            if (s_Registered)
            {
                return;
            }

            TestSuiteRegistry& registry = TestSuiteRegistry::Get();
            registry.Register(ObjectManagerSuite::Get());
            registry.Register(SerializationArchiveSuite::Get());
            registry.Register(AssetManagerSuite::Get());
            registry.Register(DataAssetSuite::Get());
            registry.Register(AssetRefTypeSuite::Get());
            registry.Register(ScenePropertyRefTypeSuite::Get());
            registry.Register(ReflectionFunctionSuite::Get());
            registry.Register(MaterialIRSuite::Get());
            registry.Register(RenderGraphSuite::Get());
            registry.Register(LuaScriptMvpSuite::Get());
            registry.Register(PhysicsSmokeSuite::Get());
            registry.Register(PhysicsSyncSuite::Get());
            registry.Register(PhysicsLoadSuite::Get());
            registry.Register(PhysicsContactSuite::Get());
            registry.Register(PhysicsLineTraceSuite::Get());
            registry.Register(PhysicsShapesSuite::Get());
            registry.Register(DelegateSuite::Get());
            registry.Register(GameplayTagSuite::Get());
            registry.Register(GameplayEventSuite::Get());
            registry.Register(ShaderCompilerSuite::Get());
            registry.Register(AudioSmokeSuite::Get());
            registry.Register(SceneCloneSuite::Get());
            registry.Register(ReflectionDisplayNamesSuite::Get());
            registry.Register(SpriteTranslucencySuite::Get());
            registry.Register(ScreenUICoordsSuite::Get());
            registry.Register(ScreenUIGameViewportSuite::Get());
            registry.Register(ScreenUIOrientationSuite::Get());
            registry.Register(FrustumSuite::Get());
            registry.Register(NoiseSuite::Get());
            registry.Register(VertexAttributeSemanticsSuite::Get());
            registry.Register(BeginPlaySuite::Get());
            registry.Register(DeferredStructuralEditSuite::Get());
            registry.Register(GameplayInputGateSuite::Get());
            registry.Register(InputTriggerDownSuite::Get());
            registry.Register(InputKeyCatalogSuite::Get());
            registry.Register(PlayMainCameraSuite::Get());
            registry.Register(AutomationPipeInterruptSuite::Get());
            registry.Register(MeshDrawSortSuite::Get());
            registry.Register(Texture2DCookSuite::Get());
            registry.Register(SpriteAtlasSuite::Get());
            registry.Register(SpriteRuntimeConsumeSuite::Get());
            registry.Register(SpriteAuthoringSuite::Get());
            registry.Register(GameModuleStubReflectionSuite::Get());
            registry.Register(UILayoutSuite::Get());
            registry.Register(ScreenUIHitSuite::Get());
            registry.Register(ScreenUIButtonSuite::Get());
            registry.Register(ScreenUIPointerSuite::Get());
            registry.Register(PointerPolicySuite::Get());
            registry.Register(ScreenUIDragSuite::Get());
            registry.Register(ScreenUIImageSuite::Get());
            registry.Register(ScreenUITextSuite::Get());
            registry.Register(GameObjectHierarchySuite::Get());
            registry.Register(CommandSystemSuite::Get());
            registry.Register(SkeletonPoseSuite::Get());
            registry.Register(AnimationClipSuite::Get());
            registry.Register(AnimationGraphSuite::Get());
            registry.Register(ParameterStoreSuite::Get());
            registry.Register(LoggingChannelsSuite::Get());
            registry.Register(EngineVersionSuite::Get());
            registry.Register(ProfilerSuite::Get());
            registry.Register(PrefabSuite::Get());
            registry.Register(PrefabOverridesSuite::Get());
            registry.Register(GameInstanceSubsystemSuite::Get());
            registry.Register(WorldSubsystemSuite::Get());
            registry.Register(TickableSubsystemSuite::Get());
            registry.Register(GameplayFixedTickSuite::Get());
            registry.Register(MaterialCompileCacheSuite::Get());
            s_Registered = true;

#if defined(MINENGINE_ENABLE_EXTRA_TESTS) && MINENGINE_ENABLE_EXTRA_TESTS
            RegisterExtraTestSuites();
#endif
        }
    }

    void EnsureTestSuitesRegistered()
    {
        RegisterAllTestSuites();
    }
}
