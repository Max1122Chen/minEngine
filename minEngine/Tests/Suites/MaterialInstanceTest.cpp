#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/MaterialInstance.h"

#include "doctest.h"

#include <memory>

TEST_CASE("material-ir: instance override leaves parent defaults [smoke]")
{
    using namespace minEngine;

    auto parent = std::make_shared<Material>();
    parent->m_ScalarParameters.push_back({"Roughness", 0, "Roughness", 0.25f});
    parent->m_TextureParameters.push_back({"BaseColor", 0, "BaseColor", nullptr});

    const std::shared_ptr<MaterialInstance> left = MaterialInstance::Create(parent);
    const std::shared_ptr<MaterialInstance> right = MaterialInstance::Create(parent);
    const bool createdBoth = left != nullptr && right != nullptr;
    REQUIRE(createdBoth);

    left->SetScalarParameter("Roughness", 0.9f);
    right->SetScalarParameter("Roughness", 0.1f);

    float parentValue = 0.0f;
    REQUIRE(parent->GetRenderProxy()->GetScalarParameterValue("Roughness", parentValue));
    CHECK(parentValue == doctest::Approx(0.25f));

    parent->SetScalarParameter("Roughness", 0.5f);
    REQUIRE(parent->GetRenderProxy()->GetScalarParameterValue("Roughness", parentValue));
    CHECK(parentValue == doctest::Approx(0.5f));

    float leftValue = 0.0f;
    float rightValue = 0.0f;
    REQUIRE(left->GetRenderProxy()->GetScalarParameterValue("Roughness", leftValue));
    REQUIRE(right->GetRenderProxy()->GetScalarParameterValue("Roughness", rightValue));
    CHECK(leftValue == doctest::Approx(0.9f));
    CHECK(rightValue == doctest::Approx(0.1f));

    float leftSlot = 0.0f;
    REQUIRE(left->GetRenderProxy()->GetScalarParameterValueBySlot(0, leftSlot));
    CHECK(leftSlot == doctest::Approx(0.9f));

    parent->m_ScalarParameters.push_back({"Metallic", 1, "Metallic", 0.4f});
    float metallic = 0.0f;
    REQUIRE(left->GetRenderProxy()->GetScalarParameterValue("Metallic", metallic));
    CHECK(metallic == doctest::Approx(0.4f));

    std::shared_ptr<Texture2D> texture;
    REQUIRE(right->GetRenderProxy()->GetTextureParameterValue("BaseColor", texture));
    const bool textureMissed = texture == nullptr;
    CHECK(textureMissed);
    REQUIRE(right->GetRenderProxy()->GetTextureParameterValueBySlot(0, texture));
    const bool textureSlotMissed = texture == nullptr;
    CHECK(textureSlotMissed);

    const bool sameBase = left->GetBaseMaterial() == parent.get();
    const bool distinctFromParent = left->GetRenderProxy() != parent->GetRenderProxy();
    const bool distinctInstances = left->GetRenderProxy() != right->GetRenderProxy();
    const bool sameShader = left->GetRenderProxy()->GetGPUShader() == parent->GetRenderProxy()->GetGPUShader();
    const bool sameLayout = left->GetRenderProxy()->GetMaterialShaderBindingSetLayout() ==
                            parent->GetRenderProxy()->GetMaterialShaderBindingSetLayout();
    CHECK(sameBase);
    CHECK(distinctFromParent);
    CHECK(distinctInstances);
    CHECK(sameShader);
    CHECK(sameLayout);
}
