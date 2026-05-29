#pragma once

#include <string>

namespace minEngine
{
    // Headless Material IR smoke (GPU compile/link); run via minEngineTests test material-ir.

    bool RunMaterialIRSmokeTests(int argc, char** argv);
    bool RunMaterialIRFullTests(int argc, char** argv);

    // Set when RunMaterialIRSmokeTests() successfully loads EngineConfig (same discovery rules as Editor).
    // Empty if load failed; intended for upcoming shader-template paths without coupling to Editor startup.
    const std::string& GetMaterialIRTestEngineDefaultAssetsRoot();
}
