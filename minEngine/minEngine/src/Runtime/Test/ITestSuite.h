#pragma once

#include "TestSuiteMetadata.h"

namespace minEngine
{
    class TestContext;

    class ITestSuite
    {
    public:
        virtual ~ITestSuite() = default;

        virtual TestSuiteMetadata GetMetadata() const = 0;
        virtual bool Run(TestContext& context) = 0;
    };
}
