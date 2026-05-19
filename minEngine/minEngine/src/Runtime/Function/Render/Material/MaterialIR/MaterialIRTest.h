#pragma once

namespace minEngine
{
    // Returns true when argv contains --material-ir-test (headless smoke, no application window).
    bool ShouldRunMaterialIRTestsOnly(int argc, char** argv);

    // Single graph: texture sample -> Albedo, Emissive constants, Metallic scalar uniform.
    // Expected FragColor: vec4(1.2, 0.8, 0.2, 1).
    // Returns true when compile succeeds and IR/GLSL golden substrings match.
    bool RunMaterialIRSmokeTests();
}
