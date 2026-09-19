#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/MaterialInstance.h"
#include "Runtime/Function/Render/MaterialInstanceDynamic.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Serialization/JsonArchive.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Access/ObjectManagerTestAccess.h"

#include "doctest.h"

#include <filesystem>
#include <memory>
#include <string>

namespace
{
    class ScopedObjectManager
    {
    public:
        ScopedObjectManager()
        {
            minEngine::Testing::TestAccess<minEngine::ObjectManager>::SetInstance(&m_Manager);
            m_Manager.Initialize();
        }

        ~ScopedObjectManager()
        {
            m_Manager.Shutdown();
            minEngine::Testing::TestAccess<minEngine::ObjectManager>::SetInstance(nullptr);
        }

    private:
        minEngine::ObjectManager m_Manager;
    };
}

TEST_CASE("material-ir: material instance memi roundtrip [smoke]")
{
    using namespace minEngine;

    ScopedObjectManager objectManagerScope;

    auto parent = NewObject<Material>("ParentMat");
    parent->m_ScalarParameters.push_back({"Roughness", 0, "Roughness", 0.25f});

    const std::shared_ptr<MaterialInstance> instance = MaterialInstance::Create(parent);
    REQUIRE(instance != nullptr);
    instance->SetScalarParameter("Roughness", 0.8f);

    const std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / "minEngine_mat_f06_roundtrip.memi";

    {
        Serialization::JsonWriterArchive writer;
        const Serialization::SerializeResult writeResult = Serialization::Serializer::ToFile(
            tempPath.string(),
            instance.get(),
            writer,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = false,
                .skipUnknownField = false,
            });
        REQUIRE(writeResult.ok);
    }

    auto loaded = NewObject<MaterialInstance>("LoadedMi");
    {
        Serialization::JsonReaderArchive reader;
        const Serialization::SerializeResult readResult = Serialization::Serializer::FromFile(
            tempPath.string(),
            loaded.get(),
            reader,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = false,
                .skipUnknownField = true,
            });
        REQUIRE(readResult.ok);
    }

    std::error_code removeError;
    std::filesystem::remove(tempPath, removeError);

    REQUIRE(loaded->FinalizeAfterLoad());
    CHECK(loaded->GetParent() == parent.get());

    float overridden = 0.0f;
    REQUIRE(loaded->GetRenderProxy()->GetScalarParameterValue("Roughness", overridden));
    CHECK(overridden == doctest::Approx(0.8f));

    float parentValue = 0.0f;
    REQUIRE(parent->GetRenderProxy()->GetScalarParameterValue("Roughness", parentValue));
    CHECK(parentValue == doctest::Approx(0.25f));

    const std::shared_ptr<MaterialInstanceDynamic> mid = MaterialInstanceDynamic::Create(loaded);
    const bool midOk = mid != nullptr;
    REQUIRE(midOk);
    CHECK(mid->GetParentShared() == parent);
}
