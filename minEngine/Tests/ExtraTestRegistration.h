#pragma once

// TEST-F05: optional registration entry provided by MINENGINE_EXTRA_TEST_SOURCES.
// Only declared when minEngineTests is built with MINENGINE_ENABLE_EXTRA_TESTS=1.

namespace minEngine
{
    void RegisterExtraTestSuites();
}
