#include "AssetRefTypeTest.h"

#include "Access/AssetManagerTestAccess.h"
#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"
#include "EngineTestFixture.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Core/Serialization/Json.h"
#include "Runtime/Core/Serialization/JsonArchive.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Function/Framework/Components/ImageComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/UI/UITypes.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/DataAsset.h"

#include "doctest.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace
{
    // BUG-EDITOR-008: the asset reference guard is only reached when the target is NOT a live
    // registry object, because a live object is resolved by the (already guarded) object branch.
    class ScopedManagers
    {
    public:
        ScopedManagers()
        {
            minEngine::Testing::TestAccess<minEngine::ObjectManager>::SetInstance(&m_ObjectManager);
            minEngine::ObjectManager::Initialize();
            minEngine::Testing::TestAccess<minEngine::SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();
            minEngine::Testing::TestAccess<minEngine::AssetManager>::SetInstance(&m_AssetManager);
            m_AssetManager.Initialize();
        }

        ~ScopedManagers()
        {
            m_AssetManager.Shutdown();
            minEngine::Testing::TestAccess<minEngine::AssetManager>::SetInstance(nullptr);
            m_SceneManager.Shutdown();
            minEngine::Testing::TestAccess<minEngine::SceneManager>::SetInstance(nullptr);
            minEngine::ObjectManager::Shutdown();
            minEngine::Testing::TestAccess<minEngine::ObjectManager>::SetInstance(nullptr);
        }

    private:
        minEngine::ObjectManager m_ObjectManager;
        minEngine::AssetManager m_AssetManager;
        minEngine::SceneManager m_SceneManager;
    };

    class ScopedTempProject
    {
    public:
        ScopedTempProject()
        {
            const std::filesystem::path tempRoot =
                std::filesystem::temp_directory_path() / "minEngine_AssetRefTypeTest";
            std::error_code removeError;
            std::filesystem::remove_all(tempRoot, removeError);
            m_ProjectRoot = tempRoot / "Project";
            std::filesystem::create_directories(m_ProjectRoot / "Assets" / "RefType");
            minEngine::PathRegistry::Get().SetProjectRoots(m_ProjectRoot);
        }

        ~ScopedTempProject()
        {
            minEngine::PathRegistry::Get().ClearProjectRoots();
            std::error_code removeError;
            std::filesystem::remove_all(
                std::filesystem::temp_directory_path() / "minEngine_AssetRefTypeTest",
                removeError);
        }

    private:
        std::filesystem::path m_ProjectRoot;
    };

    struct UnloadedAsset
    {
        minEngine::GUID Guid;
        std::string Path;
    };

    // Creates a DataAsset and drops its live instance, so a reference resolve has to reach the
    // asset path (LoadAssetByGUID) instead of the live object registry.
    bool CreateUnloadedDataAsset(
        minEngine::AssetManager& assetManager,
        const std::string& assetName,
        UnloadedAsset& outAsset)
    {
        std::string createError;
        std::shared_ptr<minEngine::Asset> asset = assetManager.CreateDataAsset(
            minEngine::DataAsset::StaticClass(),
            assetName,
            "Assets/RefType",
            createError);
        if (asset == nullptr)
        {
            ME_LOG(minEngine::LogTest, Error, "AssetRefType: CreateDataAsset failed ({})", createError);
            return false;
        }

        const minEngine::AssetMeta* meta = asset->GetMeta();
        if (meta == nullptr)
        {
            ME_LOG(minEngine::LogTest, Error, "AssetRefType: created asset has no meta.");
            return false;
        }

        outAsset.Guid = meta->Guid;
        outAsset.Path = meta->AssetPath;

        minEngine::ObjectManager::Get().UnregisterObject(asset.get());
        asset.reset();
        assetManager.EvictLoadedAssetCache(outAsset.Path);

        if (minEngine::ObjectManager::Get().FindObject(outAsset.Guid) != nullptr)
        {
            ME_LOG(minEngine::LogTest, Error, "AssetRefType: asset is still a live object.");
            return false;
        }
        return true;
    }

    Json MakeGuidNode(const minEngine::GUID& guid)
    {
        return Json{{"$guid", Json{{"high", guid.High}, {"low", guid.Low}}}};
    }
}

TEST_CASE("asset-ref-type: deserialize of wrong asset class fails resolve [smoke]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedManagers managers;
    ScopedTempProject project;

    UnloadedAsset wrongClassAsset;
    REQUIRE(CreateUnloadedDataAsset(AssetManager::Get(), "RefType_WrongClass", wrongClassAsset));

    const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("asset-ref-type-mismatch");
    REQUIRE(scene != nullptr);
    const std::shared_ptr<GameObject> gameObject = scene->CreateGameObject();
    REQUIRE(gameObject != nullptr);
    const std::shared_ptr<ImageComponent> image = gameObject->AddComponent<ImageComponent>();
    REQUIRE(image != nullptr);

    // The reader binds the Json by reference, so the value must outlive it.
    const Json referenceValue = MakeGuidNode(wrongClassAsset.Guid);
    Serialization::JsonReaderArchive reader(referenceValue);
    std::vector<Serialization::PendingObjectRef> pendingRefs;
    const Serialization::SerializerOptions options{};
    const Serialization::SerializeResult readResult = Serialization::Serializer::DeserializeProperty(
        image.get(),
        ImageComponent::StaticClass(),
        "m_Sprite",
        reader,
        pendingRefs,
        options);
    if (!readResult.ok)
    {
        MESSAGE("deserialize failure: " << readResult.message);
    }
    REQUIRE(readResult.ok);
    REQUIRE(pendingRefs.size() == 1u);

    const Serialization::SerializeResult resolveResult =
        Serialization::Serializer::ResolvePendingObjectRefs(pendingRefs);

    CHECK_FALSE(resolveResult.ok);
    CHECK(resolveResult.message.find("type mismatch") != std::string::npos);
    CHECK(resolveResult.message.find("Sprite") != std::string::npos);
    CHECK(resolveResult.message.find("DataAsset") != std::string::npos);

    // The guard runs before any binding: reading the Sprite slot back through the draw-side call
    // that dereferences m_Sprite neither crashes nor reports the foreign asset's UV rect.
    image->SetSourceMode(EUIImageSourceMode::Sprite);
    const Vector4 uv = image->ResolveDrawUVRect();
    CHECK(uv.x == doctest::Approx(0.0f));
    CHECK(uv.y == doctest::Approx(0.0f));
    CHECK(uv.z == doctest::Approx(0.0f));
    CHECK(uv.w == doctest::Approx(0.0f));
    CHECK_FALSE(image->CanDraw());
}

TEST_CASE("asset-ref-type: derived asset class still resolves [smoke]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedManagers managers;
    ScopedTempProject project;

    UnloadedAsset derivedAsset;
    REQUIRE(CreateUnloadedDataAsset(AssetManager::Get(), "RefType_Derived", derivedAsset));

    // A property typed shared_ptr<Asset> must keep accepting a DataAsset (DataAsset : Asset).
    std::shared_ptr<Asset> assetSlot;
    Serialization::PendingObjectRef pendingRef;
    pendingRef.ptrToPtr = &assetSlot;
    pendingRef.refGuid = derivedAsset.Guid;
    pendingRef.expectedClass = Asset::StaticClass();
    pendingRef.expectsMEObject = true;
    pendingRef.isRawPointer = false;
    pendingRef.fieldPath = "AssetSlot";

    std::vector<Serialization::PendingObjectRef> pendingRefs{pendingRef};
    const Serialization::SerializeResult resolveResult =
        Serialization::Serializer::ResolvePendingObjectRefs(pendingRefs);

    CHECK(resolveResult.ok);
    if (!resolveResult.ok)
    {
        MESSAGE("resolve failure: " << resolveResult.message);
    }
    REQUIRE(assetSlot != nullptr);
    CHECK(assetSlot->GetClass() == DataAsset::StaticClass());
}
