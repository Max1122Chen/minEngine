#pragma once

#include "Runtime/Core/CLI/CommandLineResult.h"

namespace minEngine
{
    class DoctestSuiteRunner
    {
    public:
        static bool RunTestCaseSubstring(const char* testCaseSubstring);

        static bool RunSuiteCases(const char* suiteNamePrefix, bool smokeCasesOnly);

        static bool RunSuiteForContext(const char* suiteNamePrefix, TestRunKind testRunKind);
    };
}
