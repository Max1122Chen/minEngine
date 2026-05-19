#pragma once

#include "Core.h"
#include "Render/Material/MaterialIR/MaterialIR.h"

#include <array>
#include <string>
#include <vector>

namespace minEngine
{
    enum class MaterialShadingModel : uint8_t
    {
        Unlit = 0,
        DefaultLit,
    };

    enum class MaterialShaderLanguage : uint8_t
    {
        GLSL = 0,
    };

    struct MaterialCompileEnvironment
    {
        MaterialShadingModel ShadingModel = MaterialShadingModel::Unlit;
        MaterialShaderLanguage ShaderLanguage = MaterialShaderLanguage::GLSL;

        // If non-empty, used instead of RuntimeGlobalContext::GetEngineDefaultAssetsRoot() when resolving
        // Shaders/Template/<language>/ and Shaders/Include/<language>/ paths.
        std::string EngineDefaultAssetsRootOverride;
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
