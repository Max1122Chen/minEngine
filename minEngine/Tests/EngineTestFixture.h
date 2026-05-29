#pragma once

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

    // Provides TestContext argv only; does not touch ReflectionSystem (reflection-function suite).
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

    // Finalizes reflection once (idempotent); use for engine suites that call NewObject / serialize.
    class EngineReflectionFixture
    {
    public:
        EngineReflectionFixture();
        ~EngineReflectionFixture();

        bool IsReflectionReady() const { return m_ReflectionReady; }

        int GetArgc() const;
        char** GetArgv() const;

    private:
        TestContext* m_Context = nullptr;
        bool m_ReflectionReady = false;
    };
}
