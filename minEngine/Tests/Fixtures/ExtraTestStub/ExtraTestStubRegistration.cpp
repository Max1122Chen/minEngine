#include "doctest.h"

#include "DoctestSuiteRunner.h"
#include "EngineTestFixture.h"
#include "ExtraTestRegistration.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Test/ITestSuite.h"
#include "Runtime/Test/TestContext.h"
#include "Runtime/Test/TestSuiteMetadata.h"
#include "Runtime/Test/TestSuiteRegistry.h"

// TEST-F05 stub: proves EXTRA sources can register a suite.
// Suite id must use the game.* prefix. InSmoke=false so verify.ps1 stays unchanged.
// Do NOT define DOCTEST_CONFIG_IMPLEMENT here (only TestMain.cpp may).

TEST_CASE("game.stub-extra: harness marker [full]")
{
    CHECK(1 + 1 == 2);
}

namespace minEngine
{
    namespace
    {
        template <typename SuiteType>
        class TypedExtraTestSuite : public ITestSuite
        {
        public:
            static TypedExtraTestSuite& Get()
            {
                static TypedExtraTestSuite instance;
                return instance;
            }

            TestSuiteMetadata GetMetadata() const override { return SuiteType::BuildMetadata(); }
            bool Run(TestContext& context) override { return SuiteType::RunSuite(context); }
        };

        struct GameStubExtraTestSuiteTraits
        {
            static TestSuiteMetadata BuildMetadata()
            {
                return TestSuiteMetadata{"game.stub-extra", "Game Stub Extra", false, true, false};
            }

            static bool RunSuite(TestContext& context)
            {
                ME_LOG(LogTest, Info, "TestRunner: starting suite 'game.stub-extra'.");
                EngineTestContextScope scope(context);
                return DoctestSuiteRunner::RunSuiteForContext(
                    "game.stub-extra",
                    context.GetCommandLine().TestKind);
            }
        };

        using GameStubExtraSuite = TypedExtraTestSuite<GameStubExtraTestSuiteTraits>;
    }

    void RegisterExtraTestSuites()
    {
        TestSuiteRegistry::Get().Register(GameStubExtraSuite::Get());
    }
}
