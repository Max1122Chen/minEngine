#pragma once

#include "Core.h"
#include "Render/Material/MaterialIR/MaterialIR.h"

#include <array>
#include <string>
#include <vector>

namespace minEngine
{
    enum class MaterialShadingMode : uint8_t
    {
        Unlit = 0,
        DefaultLit,
    };

    struct MaterialCompileEnvironment
    {
        MaterialShadingMode ShadingMode = MaterialShadingMode::Unlit;
    };

    struct MaterialCompileDiagnostic
    {
        enum Severity
        {
            Info,
            Warning,
            Error,
        };

        Severity Level = Error;
        std::string Message;
    };

    struct MaterialStageSource
    {
        ShaderStage Stage = Stage_Fragment;
        std::string Preamble;
        std::string Body;
    };

    struct MaterialCompiledShader
    {
        bool Succeeded = false;
        std::array<MaterialStageSource, NumStages> Stages{};
        std::string FullVertexShader;
        std::string FullFragmentShader;
        std::string IRDump;
        std::vector<MaterialCompileDiagnostic> Diagnostics;
        bool UsesTexCoord0 = false;
    };
}
