#pragma once

namespace minEngine::Testing
{
    /**
     * Test-only private access hook.
     * Production types friend a specialization: Testing::TestAccess<ThisType>.
     * Specializations live under Tests/Access/ and must NOT be included by Runtime TUs.
     */
    template<typename T>
    class TestAccess;
}
