#include "EngineTestFixture.h"

namespace minEngine
{
    namespace
    {
        thread_local TestContext* g_ActiveTestContext = nullptr;
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
}
