#pragma once

namespace minEngine
{
    // Headless smoke when argv contains --asset-manager-test.
    bool ShouldRunAssetManagerTestsOnly(int argc, char** argv);

    bool RunAssetManagerTests(int argc, char** argv);
}
