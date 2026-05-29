#pragma once

#include <string>

namespace minEngine
{
    // Headless Material IR smoke (GPU compile/link); run via minEngineTests test material-ir.

    // Single graph smoke: IR + GLSL + optional GPU compile (see MaterialIRTest.cpp).
    // Returns true when compile succeeds, IR/GLSL golden substrings match, and GPU compile/link succeeds.
    bool RunMaterialIRSmokeTests(int argc, char** argv);

    // Set when RunMaterialIRSmokeTests() successfully loads EngineConfig (same discovery rules as Editor).
    // Empty if load failed; intended for upcoming shader-template paths without coupling to Editor startup.
    const std::string& GetMaterialIRTestEngineDefaultAssetsRoot();
}
