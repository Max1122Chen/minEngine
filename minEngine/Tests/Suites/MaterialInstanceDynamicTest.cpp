#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/MaterialInstance.h"
#include "Runtime/Function/Render/MaterialInstanceDynamic.h"

#include "doctest.h"

#include <memory>

TEST_CASE("material-ir: dynamic instance create and per-frame set [smoke]")
{
    using namespace minEngine;

    auto parent = std::make_shared<Material>();
    parent->m_ScalarParameters.push_back({"Tint", 0, "Tint", 0.2f});

    const std::shared_ptr<MaterialInstanceDynamic> mid =
        MaterialInstanceDynamic::Create(parent, "MidSmoke");
    const bool created = mid != nullptr;
    REQUIRE(created);
    CHECK(mid->GetName() == "MidSmoke");

    MaterialInterface* asInterface = mid.get();
    CHECK(asInterface != nullptr);

    MaterialRenderProxy* proxy = mid->GetRenderProxy();
    REQUIRE(proxy != nullptr);

    MaterialRenderProxy* parentProxy = parent->GetRenderProxy();
    CHECK(proxy != parentProxy);
    CHECK(proxy->GetGPUShader() == parentProxy->GetGPUShader());

    for (int frame = 0; frame < 8; ++frame)
    {
        const float value = 0.1f * static_cast<float>(frame);
        mid->SetScalarParameter("Tint", value);

        float midValue = 0.0f;
        REQUIRE(proxy->GetScalarParameterValue("Tint", midValue));
        CHECK(midValue == doctest::Approx(value));

        float parentValue = 0.0f;
        REQUIRE(parentProxy->GetScalarParameterValue("Tint", parentValue));
        CHECK(parentValue == doctest::Approx(0.2f));
    }

    const std::shared_ptr<MaterialInstance> staticInstance = MaterialInstance::Create(parent);
    const bool staticOk = staticInstance != nullptr;
    REQUIRE(staticOk);
    const std::shared_ptr<MaterialInstanceDynamic> fromInstance =
        MaterialInstanceDynamic::Create(staticInstance, "FromMi");
    const bool fromInstanceOk = fromInstance != nullptr;
    REQUIRE(fromInstanceOk);
    CHECK(fromInstance->GetBaseMaterial() == parent.get());
    const bool sameParentShared = fromInstance->GetParentShared() == parent;
    CHECK(sameParentShared);
}
