#pragma once

#include "Runtime/Core/EngineAPI.h"

#include "TestSuiteMetadata.h"

namespace minEngine
{
    class TestContext;

    class MINENGINE_API ITestSuite
    {
    public:
        virtual ~ITestSuite() = default;

        virtual TestSuiteMetadata GetMetadata() const = 0;
        virtual bool Run(TestContext& context) = 0;
    };
}
