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
                return TestSuiteMetadata{
                    "object-manager",
                    "Object Manager",
                    true,
                    true,
                    false,
                };
            }

            static bool RunSuite(TestContext& context)
            {
                ME_CORE_INFO("TestRunner: starting suite 'object-manager'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "object-manager",
                    context.GetCommandLine().TestKind);
            }
        };

        struct SerializationArchiveTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{
                    "serialization-archive",
                    "Serialization Archive",
                    true,
                    true,
                    false,
                };
            }

            static bool RunSuite(TestContext& context)
            {
                ME_CORE_INFO("TestRunner: starting suite 'serialization-archive'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "serialization-archive",
                    context.GetCommandLine().TestKind);
            }
        };

        struct AssetManagerTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{
                    "asset-manager",
                    "Asset Manager",
                    true,
                    true,
                    false,
                };
            }

            static bool RunSuite(TestContext& context)
            {
                ME_CORE_INFO("TestRunner: starting suite 'asset-manager'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "asset-manager",
                    context.GetCommandLine().TestKind);
            }
        };

        struct ReflectionFunctionTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{
                    "reflection-function",
                    "Reflection Function",
                    true,
                    true,
                    false,
                };
            }

            static bool RunSuite(TestContext& context)
            {
                ME_CORE_INFO("TestRunner: starting suite 'reflection-function'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "reflection-function",
                    context.GetCommandLine().TestKind);
            }
        };

        struct MaterialIRTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{
                    "material-ir",
                    "Material IR",
                    true,
                    true,
                    true,
                };
            }

            static bool RunSuite(TestContext& context)
            {
                ME_CORE_INFO("TestRunner: starting suite 'material-ir'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "material-ir",
                    context.GetCommandLine().TestKind);
            }
        };

        struct LuaScriptMvpTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                // Disposable feasibility suite — not part of smoke/full gates.
                return TestSuiteMetadata{
                    "lua-script-mvp",
                    "Lua Script MVP",
                    false,
                    false,
                    false,
                };
            }

            static bool RunSuite(TestContext& context)
            {
                ME_CORE_INFO("TestRunner: starting suite 'lua-script-mvp'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "lua-script-mvp",
                    context.GetCommandLine().TestKind);
            }
        };

        using ObjectManagerSuite = TypedTestSuite<ObjectManagerTestSuiteTraits>;
        using SerializationArchiveSuite = TypedTestSuite<SerializationArchiveTestSuiteTraits>;
        using AssetManagerSuite = TypedTestSuite<AssetManagerTestSuiteTraits>;
        using ReflectionFunctionSuite = TypedTestSuite<ReflectionFunctionTestSuiteTraits>;
        using MaterialIRSuite = TypedTestSuite<MaterialIRTestSuiteTraits>;
        using LuaScriptMvpSuite = TypedTestSuite<LuaScriptMvpTestSuiteTraits>;

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
            registry.Register(LuaScriptMvpSuite::Get());
            s_Registered = true;
        }
    }

    void EnsureTestSuitesRegistered()
    {
        RegisterAllTestSuites();
    }
}
