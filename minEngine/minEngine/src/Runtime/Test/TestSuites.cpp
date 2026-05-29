#include "ITestSuite.h"
#include "TestContext.h"
#include "TestSuiteRegistry.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManagerTest.h"
#include "Runtime/Core/Reflection/ReflectionFunctionTest.h"
#include "Runtime/Core/Serialization/SerializationArchiveTest.h"
#include "Runtime/Function/Render/Material/MaterialIR/MaterialIRTest.h"
#include "Runtime/Resource/AssetManagerTest.h"

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
                return RunObjectManagerTests(context.GetArgc(), context.GetArgv());
            }
        };

        struct SerializationArchiveTestSuite
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
                return RunSerializationArchiveTests(context.GetArgc(), context.GetArgv());
            }
        };

        struct AssetManagerTestSuite
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
                return RunAssetManagerTests(context.GetArgc(), context.GetArgv());
            }
        };

        struct ReflectionFunctionTestSuite
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

                std::vector<std::string> argumentStorage;
                std::vector<char*> argumentPointers;
                if (!context.BuildReflectionArgv(argumentStorage, argumentPointers))
                {
                    ME_CORE_ERROR("TestRunner: failed to build reflection-function argv.");
                    return false;
                }

                const int reflectionArgc = static_cast<int>(argumentPointers.size());
                return RunReflectionFunctionTests(reflectionArgc, argumentPointers.data());
            }
        };

        struct MaterialIRTestSuite
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
                return RunMaterialIRSmokeTests(context.GetArgc(), context.GetArgv());
            }
        };

        using ObjectManagerSuite = TypedTestSuite<ObjectManagerTestSuiteTraits>;
        using SerializationArchiveSuite = TypedTestSuite<SerializationArchiveTestSuite>;
        using AssetManagerSuite = TypedTestSuite<AssetManagerTestSuite>;
        using ReflectionFunctionSuite = TypedTestSuite<ReflectionFunctionTestSuite>;
        using MaterialIRSuite = TypedTestSuite<MaterialIRTestSuite>;
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
            s_Registered = true;
        }
    }

    void EnsureTestSuitesRegistered()
    {
        RegisterAllTestSuites();
    }
}
