#include "TestSuiteRegistry.h"

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
        // Deterministic smoke order (TEST_UNIFIED_DESIGN §3.5).
        static constexpr std::string_view kSmokeOrder[] = {
            "object-manager",
            "serialization-archive",
            "asset-manager",
            "reflection-function",
            "material-ir",
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
