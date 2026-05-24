#pragma once

namespace minEngine
{
    // Headless smoke when argv contains --object-manager-test.
    bool ShouldRunObjectManagerTestsOnly(int argc, char** argv);

    bool RunObjectManagerTests(int argc, char** argv);
}
