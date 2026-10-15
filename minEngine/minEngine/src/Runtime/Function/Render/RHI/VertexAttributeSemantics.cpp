#include "Runtime/Function/Render/RHI/VertexAttributeSemantics.h"

#include "Runtime/Function/Render/RHI/RHIBuffers.h"

namespace minEngine
{
    namespace VertexAttr
    {
        VertexElementType DefaultType(Semantic semantic)
        {
            switch (semantic)
            {
            case Semantic::Position:
            case Semantic::Normal:
                return VertexElementType::Float3;
            case Semantic::TexCoord0:
                return VertexElementType::Float2;
            case Semantic::Tangent:
            case Semantic::Color:
            case Semantic::Custom0:
            case Semantic::Custom1:
            case Semantic::Custom2:
            case Semantic::Custom3:
            case Semantic::BoneWeights:
                return VertexElementType::Float4;
            case Semantic::BoneIndices:
                return VertexElementType::Int4;
            default:
                return VertexElementType::None;
            }
        }

        RHIVertexElement Make(Semantic semantic, VertexElementType type, bool normalized)
        {
            if (type == VertexElementType::None)
            {
                type = DefaultType(semantic);
            }
            return RHIVertexElement(Name(semantic), type, normalized, Location(semantic));
        }
    }
}
