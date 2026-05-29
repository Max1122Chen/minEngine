#include "DoctestSuiteRunner.h"

#include <cstdio>
#include <string>

#include "doctest.h"

namespace minEngine
{
    bool DoctestSuiteRunner::RunTestCaseSubstring(const char* testCaseSubstring)
    {
        if (testCaseSubstring == nullptr || testCaseSubstring[0] == '\0')
        {
            return false;
        }

        doctest::Context context;
        context.setOption("no-intro", true);
        context.setOption("abort-after", 1);

        const std::string filter = std::string("*") + testCaseSubstring + "*";
        context.setOption("test-case", filter.c_str());

        return context.run() == 0;
    }

    bool DoctestSuiteRunner::RunSuiteCases(const char* suiteNamePrefix, bool smokeCasesOnly)
    {
        if (suiteNamePrefix == nullptr || suiteNamePrefix[0] == '\0')
        {
            return false;
        }

        doctest::Context context;
        context.setOption("no-intro", true);
        context.setOption("abort-after", 1);

        std::string filter = std::string("*") + suiteNamePrefix + "*";
        if (smokeCasesOnly)
        {
            filter += "[smoke]*";
        }

        context.setOption("test-case", filter.c_str());
        return context.run() == 0;
    }

    bool DoctestSuiteRunner::RunSuiteForContext(const char* suiteNamePrefix, TestRunKind testRunKind)
    {
        const bool smokeCasesOnly = testRunKind == TestRunKind::Smoke;
        return RunSuiteCases(suiteNamePrefix, smokeCasesOnly);
    }
}
