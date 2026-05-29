#include "EngineTestFixture.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Reflection/Reflection.h"

namespace minEngine
{
    namespace
    {
        thread_local TestContext* g_ActiveTestContext = nullptr;

        bool FinalizeReflectionForEngineTests()
        {
            Reflection::ReflectionSystem& reflection = Reflection::ReflectionSystem::Get();
            if (reflection.IsReady())
            {
                return true;
            }

            if (!reflection.FinalizeReflection())
            {
                for (const std::string& error : reflection.GetLastErrors())
                {
                    ME_CORE_ERROR("EngineReflectionFixture: {}", error);
                }
                return false;
            }

            reflection.ClearErrors();
            return true;
        }
    }

    EngineTestContextScope::EngineTestContextScope(TestContext& context)
    {
        m_PreviousContext = g_ActiveTestContext;
        g_ActiveTestContext = &context;
    }

    EngineTestContextScope::~EngineTestContextScope()
    {
        g_ActiveTestContext = m_PreviousContext;
    }

    TestContext* EngineTestContextScope::GetActiveContext()
    {
        return g_ActiveTestContext;
    }

    EngineTestFixture::EngineTestFixture()
    {
        m_Context = g_ActiveTestContext;
    }

    EngineTestFixture::~EngineTestFixture() = default;

    int EngineTestFixture::GetArgc() const
    {
        return m_Context != nullptr ? m_Context->GetArgc() : 0;
    }

    char** EngineTestFixture::GetArgv() const
    {
        return m_Context != nullptr ? m_Context->GetArgv() : nullptr;
    }

    EngineReflectionFixture::EngineReflectionFixture()
    {
        m_Context = g_ActiveTestContext;
        m_ReflectionReady = FinalizeReflectionForEngineTests();
    }

    EngineReflectionFixture::~EngineReflectionFixture() = default;

    int EngineReflectionFixture::GetArgc() const
    {
        return m_Context != nullptr ? m_Context->GetArgc() : 0;
    }

    char** EngineReflectionFixture::GetArgv() const
    {
        return m_Context != nullptr ? m_Context->GetArgv() : nullptr;
    }
}
