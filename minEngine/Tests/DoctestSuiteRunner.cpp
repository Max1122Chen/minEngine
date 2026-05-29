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
}
