#include "VertexAttributeSemanticsTest.h"

#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RHI/VertexAttributeSemantics.h"

#include "doctest.h"

TEST_CASE("vertex-attribute-semantics: canonical locations [smoke]")
{
    using minEngine::VertexAttr::Location;
    using minEngine::VertexAttr::LocationForName;
    using minEngine::VertexAttr::Make;
    using minEngine::VertexAttr::Name;
    using minEngine::VertexAttr::Semantic;

    CHECK(Location(Semantic::Position) == 0u);
    CHECK(Location(Semantic::TexCoord0) == 1u);
    CHECK(Location(Semantic::Normal) == 2u);
    CHECK(Location(Semantic::Tangent) == 3u);
    CHECK(Location(Semantic::Color) == 4u);
    CHECK(Location(Semantic::Custom0) == 5u);
    CHECK(Location(Semantic::Custom1) == 6u);
    CHECK(Location(Semantic::BoneIndices) == 9u);
    CHECK(Location(Semantic::BoneWeights) == 10u);

    CHECK(LocationForName("a_Color") == 4u);
    CHECK(LocationForName("a_BoneIndices") == 9u);
    CHECK(LocationForName("a_Unknown") == minEngine::VertexAttr::kUnsetLocation);

    const minEngine::RHIVertexElement color = Make(Semantic::Color);
    CHECK(color.Name == Name(Semantic::Color));
    CHECK(color.Location == 4u);
    CHECK(color.Type == minEngine::VertexElementType::Float4);

    // Name-based ctor resolves canonical location.
    const minEngine::RHIVertexElement bones("a_BoneIndices", minEngine::VertexElementType::Int4);
    CHECK(bones.Location == 9u);

    // Explicit location wins (DebugDraw-style specialty paths).
    const minEngine::RHIVertexElement debugColor(
        "a_Color",
        minEngine::VertexElementType::Float4,
        false,
        1u);
    CHECK(debugColor.Location == 1u);
}
