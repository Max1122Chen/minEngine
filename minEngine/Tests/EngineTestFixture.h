#pragma once

#include "Runtime/Core/CLI/CommandLineResult.h"
#include "Runtime/Test/TestContext.h"

namespace minEngine
{
    class EngineTestContextScope
    {
    public:
        explicit EngineTestContextScope(TestContext& context);
        ~EngineTestContextScope();

        EngineTestContextScope(const EngineTestContextScope&) = delete;
        EngineTestContextScope& operator=(const EngineTestContextScope&) = delete;

        static TestContext* GetActiveContext();

    private:
        TestContext* m_PreviousContext = nullptr;
    };

    class EngineTestFixture
    {
    public:
        EngineTestFixture();
        ~EngineTestFixture();

        int GetArgc() const;
        char** GetArgv() const;

    private:
        TestContext* m_Context = nullptr;
    };
}
