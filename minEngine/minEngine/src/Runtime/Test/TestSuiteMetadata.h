#pragma once

#include <string_view>

namespace minEngine
{
    struct TestSuiteMetadata
    {
        std::string_view Id;
        std::string_view DisplayName;
        bool InSmoke = false;
        bool InFull = false;
        bool RequiresGpu = false;
    };
}
