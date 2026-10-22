#pragma once

#include "Runtime/Core/EngineAPI.h"

#include <cstdint>
#include <string>

namespace minEngine
{
    enum class VertexElementType;
    struct RHIVertexElement;

    /**
     * Canonical vertex attribute locations for mesh + material shaders.
     * Shader `layout(location = N)` and RHIVertexElement::Location must match this table.
     * Optional attributes keep their location even when omitted from a mesh.
     */
    namespace VertexAttr
    {
        enum class Semantic : uint8_t
        {
            Position = 0,
            TexCoord0,
            Normal,
            Tangent,
            Color,
            Custom0,
            Custom1,
            Custom2,
            Custom3,
            BoneIndices,
            BoneWeights,
            Count
        };

        constexpr uint32_t kUnsetLocation = 0xFFFFFFFFu;

        constexpr uint32_t Location(Semantic semantic)
        {
            switch (semantic)
            {
            case Semantic::Position:
                return 0;
            case Semantic::TexCoord0:
                return 1;
            case Semantic::Normal:
                return 2;
            case Semantic::Tangent:
                return 3;
            case Semantic::Color:
                return 4;
            case Semantic::Custom0:
                return 5;
            case Semantic::Custom1:
                return 6;
            case Semantic::Custom2:
                return 7;
            case Semantic::Custom3:
                return 8;
            case Semantic::BoneIndices:
                return 9;
            case Semantic::BoneWeights:
                return 10;
            default:
                return kUnsetLocation;
            }
        }

        constexpr const char* Name(Semantic semantic)
        {
            switch (semantic)
            {
            case Semantic::Position:
                return "a_Position";
            case Semantic::TexCoord0:
                return "a_TexCoord";
            case Semantic::Normal:
                return "a_Normal";
            case Semantic::Tangent:
                return "a_Tangent";
            case Semantic::Color:
                return "a_Color";
            case Semantic::Custom0:
                return "a_Custom0";
            case Semantic::Custom1:
                return "a_Custom1";
            case Semantic::Custom2:
                return "a_Custom2";
            case Semantic::Custom3:
                return "a_Custom3";
            case Semantic::BoneIndices:
                return "a_BoneIndices";
            case Semantic::BoneWeights:
                return "a_BoneWeights";
            default:
                return "";
            }
        }

        inline bool TryParseSemantic(const std::string& name, Semantic& outSemantic)
        {
            for (uint8_t i = 0; i < static_cast<uint8_t>(Semantic::Count); ++i)
            {
                const Semantic semantic = static_cast<Semantic>(i);
                if (name == Name(semantic))
                {
                    outSemantic = semantic;
                    return true;
                }
            }
            return false;
        }

        inline uint32_t LocationForName(const std::string& name)
        {
            Semantic semantic = Semantic::Position;
            if (!TryParseSemantic(name, semantic))
            {
                return kUnsetLocation;
            }
            return Location(semantic);
        }

        MINENGINE_API VertexElementType DefaultType(Semantic semantic);

        MINENGINE_API RHIVertexElement Make(
            Semantic semantic,
            VertexElementType type = static_cast<VertexElementType>(0),
            bool normalized = false);
    }
}
