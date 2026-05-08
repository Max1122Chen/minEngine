#pragma once

#include "Core.h"

namespace minEngine
{
    enum class MaterialValueType : uint8_t
    {
        Unknown = 0,
        Float,
        Vector2,
        Vector3,
        Vector4,
        Texture2D,
        TextureCube,
    };

    enum class MaterialProperty : uint8_t
    {
        Albedo = 0,
        Metallic,
        Roughness,
        Emissive,
        Opacity,
    };

    enum class MaterialOp : uint8_t
    {
        Constant,
        Constant2,
        Constant3,
        Constant4,

        Add,
        Multiply,

        TextureObject,
        TextureSample,
    };

    struct MaterialLiteralValue
    {
        MaterialValueType Type = MaterialValueType::Unknown;
        float Data[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

        static MaterialLiteralValue MakeFloat(float x)
        {
            MaterialLiteralValue value;
            value.Type = MaterialValueType::Float;
            value.Data[0] = x;
            return value;
        }

        static MaterialLiteralValue MakeVector2(float x, float y)
        {
            MaterialLiteralValue value;
            value.Type = MaterialValueType::Vector2;
            value.Data[0] = x;
            value.Data[1] = y;
            return value;
        }

        static MaterialLiteralValue MakeVector3(float x, float y, float z)
        {
            MaterialLiteralValue value;
            value.Type = MaterialValueType::Vector3;
            value.Data[0] = x;
            value.Data[1] = y;
            value.Data[2] = z;
            return value;
        }

        static MaterialLiteralValue MakeVector4(float x, float y, float z, float w)
        {
            MaterialLiteralValue value;
            value.Type = MaterialValueType::Vector4;
            value.Data[0] = x;
            value.Data[1] = y;
            value.Data[2] = z;
            value.Data[3] = w;
            return value;
        }
    };

    struct MaterialCompileDiagnostic
    {
        enum class Severity : uint8_t
        {
            Info = 0,
            Warning,
            Error,
        };

        Severity Level = Severity::Info;
        std::string Message;
    };

    struct MaterialCompileResult
    {
        std::string ShaderSource;
        std::vector<MaterialCompileDiagnostic> Diagnostics;

        bool IsSuccess() const
        {
            for (const MaterialCompileDiagnostic& diagnostic : Diagnostics)
            {
                if (diagnostic.Level == MaterialCompileDiagnostic::Severity::Error)
                {
                    return false;
                }
            }
            return true;
        }
    };
}