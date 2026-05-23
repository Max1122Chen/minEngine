#pragma once

#include "Core.h"
#include "Render/Material/MaterialIR/MaterialIR.h"

#include <array>
#include <string>
#include <vector>

namespace minEngine
{
    class RHI;

    ME_ENUM()
    enum class MaterialShadingModel : uint8_t
    {
        Unlit = 0,
        BlinnPhong,
        PBR,
    };

    ME_ENUM()
    enum class MaterialBlendMode : uint8_t
    {
        Opaque = 0,
        Masked,
        Translucent,
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
        MaterialBlendMode BlendMode = MaterialBlendMode::Opaque;
        MaterialShaderLanguage ShaderLanguage = MaterialShaderLanguage::GLSL;
        /** BlinnPhong: TBN + a_Tangent; MP_Normal is tangent-space (default +Z). */
        bool UsesTangentFrame = false;
        std::string EngineDefaultAssetsRootOverride;
    };

    /** Alpha clip threshold for Masked materials (Phase 1 constant). */
    constexpr float kMaskedClipThreshold = 0.5f;

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

#include "Generated/Reflection/MaterialCompileTypes.gen.h"
