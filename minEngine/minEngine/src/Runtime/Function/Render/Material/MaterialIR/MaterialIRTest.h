#pragma once

#include <string>

namespace minEngine
{
    // Returns true when argv contains --material-ir-test (headless smoke, no application window).
    bool ShouldRunMaterialIRTestsOnly(int argc, char** argv);

    // Single graph smoke: IR + GLSL + optional GPU compile (see MaterialIRTest.cpp).
    // Returns true when compile succeeds, IR/GLSL golden substrings match, and GPU compile/link succeeds.
    bool RunMaterialIRSmokeTests();

    // Set when RunMaterialIRSmokeTests() successfully loads EngineConfig (same discovery rules as Editor).
    // Empty if load failed; intended for upcoming shader-template paths without coupling to Editor startup.
    const std::string& GetMaterialIRTestEngineDefaultAssetsRoot();
}
