#pragma once

#include "Core.h"
#include "Render/Material/MaterialIR/MaterialIR.h"

#include <array>
#include <string>
#include <vector>

namespace minEngine
{
    class RHI;

    enum class MaterialShadingModel : uint8_t
    {
        Unlit = 0,
        DefaultLit,
    };

    enum class MaterialShaderLanguage : uint8_t
    {
        GLSL = 0,
    };

    enum class MaterialShaderParameterType : uint8_t
    {
        Texture2D = 0,
        Scalar,
    };

    struct MaterialShaderParameterDesc
    {
        MaterialShaderParameterType Type = MaterialShaderParameterType::Scalar;
        int SlotIndex = 0;
        std::string ParameterName;
        std::string ShaderSymbolName;
    };

    struct MaterialShaderParameterLayout
    {
        std::vector<MaterialShaderParameterDesc> Parameters;
    };

    struct MaterialCompileContext
    {
        RHI* RHI = nullptr;
        std::string EngineDefaultAssetsRootOverride;
    };

    struct MaterialCompileEnvironment
    {
        MaterialShadingModel ShadingModel = MaterialShadingModel::Unlit;
        MaterialShaderLanguage ShaderLanguage = MaterialShaderLanguage::GLSL;
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

    struct MaterialCompileResult
    {
        bool Succeeded = false;
        std::array<MaterialStageSource, NumStages> Stages{};
        std::string FullVertexShader;
        std::string FullFragmentShader;
        std::string IRDump;
        std::vector<MaterialCompileDiagnostic> Diagnostics;
        bool UsesTexCoord0 = false;
        MaterialShaderParameterLayout ParameterLayout;
    };
}
