#include "TestSuites.h"

#include "DoctestSuiteRunner.h"
#include "EngineTestFixture.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Test/ITestSuite.h"
#include "Runtime/Test/TestContext.h"
#include "Runtime/Test/TestSuiteRegistry.h"

#include "Suites/SerializationArchiveTest.h"
#include "Suites/AssetManagerTest.h"
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
#include "Suites/AudioSmokeTest.h"

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
                ME_CORE_INFO("TestRunner: starting suite 'object-manager'.");
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
                ME_CORE_INFO("TestRunner: starting suite 'serialization-archive'.");
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
                ME_CORE_INFO("TestRunner: starting suite 'asset-manager'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext("asset-manager", context.GetCommandLine().TestKind);
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
                ME_CORE_INFO("TestRunner: starting suite 'reflection-function'.");
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
                ME_CORE_INFO("TestRunner: starting suite 'material-ir'.");
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
                ME_CORE_INFO("TestRunner: starting suite 'render-graph'.");
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
                ME_CORE_INFO("TestRunner: starting suite 'lua-script-mvp'.");
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
                ME_CORE_INFO("TestRunner: starting suite 'physics-smoke'.");
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
                ME_CORE_INFO("TestRunner: starting suite 'physics-sync'.");
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
                ME_CORE_INFO("TestRunner: starting suite 'physics-load'.");
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
                ME_CORE_INFO("TestRunner: starting suite 'physics-contact'.");
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
                ME_CORE_INFO("TestRunner: starting suite 'physics-linetrace'.");
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
                ME_CORE_INFO("TestRunner: starting suite 'physics-shapes'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext("physics-shapes", context.GetCommandLine().TestKind);
            }
        };

        struct DelegateTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                // Not in smoke: native unit suite; run via `test delegates` or full.
                return TestSuiteMetadata{"delegates", "Native Multicast Delegates", false, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_CORE_INFO("TestRunner: starting suite 'delegates'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext("delegates", context.GetCommandLine().TestKind);
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
                ME_CORE_INFO("TestRunner: starting suite 'audio-smoke'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext("audio-smoke", context.GetCommandLine().TestKind);
            }
        };

        using ObjectManagerSuite = TypedTestSuite<ObjectManagerTestSuiteTraits>;
        using SerializationArchiveSuite = TypedTestSuite<SerializationArchiveTestSuiteTraits>;
        using AssetManagerSuite = TypedTestSuite<AssetManagerTestSuiteTraits>;
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
        using AudioSmokeSuite = TypedTestSuite<AudioSmokeTestSuiteTraits>;

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
            registry.Register(AudioSmokeSuite::Get());
            s_Registered = true;
        }
    }

    void EnsureTestSuitesRegistered()
    {
        RegisterAllTestSuites();
    }
}
