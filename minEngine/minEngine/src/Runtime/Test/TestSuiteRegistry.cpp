#include "TestSuiteRegistry.h"

#include "Runtime/Core/Log/LogSystem.h"

#include <algorithm>

namespace minEngine
{
    TestSuiteRegistry& TestSuiteRegistry::Get()
    {
        static TestSuiteRegistry instance;
        return instance;
    }

    void TestSuiteRegistry::Register(ITestSuite& suite)
    {
        const TestSuiteMetadata metadata = suite.GetMetadata();
        for (ITestSuite* existing : m_Suites)
        {
            if (existing != nullptr && existing->GetMetadata().Id == metadata.Id)
            {
                ME_LOG(
                    LogTest,
                    Error,
                    "TestSuiteRegistry: duplicate suite id '{}'; ignoring second registration.",
                    metadata.Id);
                return;
            }
        }

        m_Suites.push_back(&suite);
    }

    ITestSuite* TestSuiteRegistry::FindById(const std::string_view suiteId) const
    {
        for (ITestSuite* suite : m_Suites)
        {
            if (suite != nullptr && suite->GetMetadata().Id == suiteId)
            {
                return suite;
            }
        }
        return nullptr;
    }

    std::vector<ITestSuite*> TestSuiteRegistry::GetSmokeSuites() const
    {
        // Deterministic smoke order: the suites below run in this fixed sequence.
        static constexpr std::string_view kSmokeOrder[] = {
            "object-manager",
            "serialization-archive",
            "asset-ref-type",
            "scene-property-ref-type",
            "scene-property-string",
            "asset-manager",
            "reflection-function",
            "material-ir",
            "screen-ui-game-viewport",
            "hierarchy-activity",
            "activity-consumers",
            "widget-graphic",
            "input-trigger-lifecycle",
            "texture-sampler",
            "task-system",
            "frame-threading",
            "render-command-pipe",
            "rhi-command-list",
            "rhi-deferred-create",
            "rhi-deferred-destroy",
            "scene-retirement",
            "deferred-release",
            "scene-proxy-lifetime",
            "scene-proxy-render-read",
            "scene-proxy-identity",
            "editor-imgui-texture",
            "scene-debug-draw-snapshot",
            "mesh-assembly-determinism",
        };

        std::vector<ITestSuite*> ordered;
        ordered.reserve(m_Suites.size());
        for (const std::string_view suiteId : kSmokeOrder)
        {
            ITestSuite* suite = FindById(suiteId);
            if (suite != nullptr && suite->GetMetadata().InSmoke)
            {
                ordered.push_back(suite);
            }
        }
        return ordered;
    }

    std::vector<ITestSuite*> TestSuiteRegistry::GetFullSuites() const
    {
        std::vector<ITestSuite*> suites = GetSmokeSuites();
        for (ITestSuite* suite : m_Suites)
        {
            if (suite == nullptr || !suite->GetMetadata().InFull)
            {
                continue;
            }

            if (std::find(suites.begin(), suites.end(), suite) == suites.end())
            {
                suites.push_back(suite);
            }
        }
        return suites;
    }
}
