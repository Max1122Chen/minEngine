#pragma once

#include "ITestSuite.h"

#include <string_view>
#include <vector>

namespace minEngine
{
    class TestSuiteRegistry
    {
    public:
        static TestSuiteRegistry& Get();

        void Register(ITestSuite& suite);
        ITestSuite* FindById(std::string_view suiteId) const;

        std::vector<ITestSuite*> GetSmokeSuites() const;
        std::vector<ITestSuite*> GetFullSuites() const;

    private:
        TestSuiteRegistry() = default;

        std::vector<ITestSuite*> m_Suites;
    };
}
