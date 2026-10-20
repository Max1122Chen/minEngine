#include "ScenePropertyRefTypeTest.h"

#include "Access/AssetManagerTestAccess.h"
#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"
#include "EngineTestFixture.h"

#include "Services/Scene/ScenePropertyValueCodec.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Core/Serialization/Json.h"
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

namespace
{
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
                std::filesystem::temp_directory_path() / "minEngine_ScenePropertyRefTypeTest";
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
                std::filesystem::temp_directory_path() / "minEngine_ScenePropertyRefTypeTest",
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
            ME_LOG(minEngine::LogTest, Error, "ScenePropertyRefType: CreateDataAsset failed ({})", createError);
            return false;
        }

        const minEngine::AssetMeta* meta = asset->GetMeta();
        if (meta == nullptr)
        {
            ME_LOG(minEngine::LogTest, Error, "ScenePropertyRefType: created asset has no meta.");
            return false;
        }

        outAsset.Guid = meta->Guid;
        outAsset.Path = meta->AssetPath;

        minEngine::ObjectManager::Get().UnregisterObject(asset.get());
        asset.reset();
        assetManager.EvictLoadedAssetCache(outAsset.Path);
        return true;
    }

    Json MakeGuidNode(const minEngine::GUID& guid)
    {
        return Json{{"$guid", Json{{"high", guid.High}, {"low", guid.Low}}}};
    }
}

TEST_CASE("scene-property-ref-type: guid write of wrong asset class is an invalid payload [smoke]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedManagers managers;
    ScopedTempProject project;

    UnloadedAsset wrongClassAsset;
    REQUIRE(CreateUnloadedDataAsset(AssetManager::Get(), "SceneProp_WrongClass", wrongClassAsset));

    const std::shared_ptr<Scene> scene =
        SceneManager::Get().CreateNewScene("scene-property-ref-type-guid");
    REQUIRE(scene != nullptr);
    const std::shared_ptr<GameObject> gameObject = scene->CreateGameObject();
    REQUIRE(gameObject != nullptr);
    const std::shared_ptr<ImageComponent> image = gameObject->AddComponent<ImageComponent>();
    REQUIRE(image != nullptr);
    image->SetSourceMode(EUIImageSourceMode::Sprite);

    // Stage 1 of scene.set_property: rewrite the reference sugar and verify it resolves.
    Json normalizedValue;
    ScenePropertyValueCodec::RefError normalizeError = ScenePropertyValueCodec::RefError::None;
    std::string normalizeMessage;
    CHECK(ScenePropertyValueCodec::TryNormalizeValueReferences(
        MakeGuidNode(wrongClassAsset.Guid), normalizedValue, normalizeError, normalizeMessage));

    // Stage 2: the live write. A DataAsset must not be able to occupy a shared_ptr<Sprite> slot.
    const Serialization::SerializerOptions options{};
    ScenePropertyValueCodec::RefError writeError = ScenePropertyValueCodec::RefError::None;
    std::string writeMessage;
    const bool written = ScenePropertyValueCodec::TryWriteJsonToLive(
        *image,
        *ImageComponent::StaticClass(),
        "m_Sprite",
        normalizedValue,
        writeMessage,
        options,
        &writeError);

    CHECK_FALSE(written);
    CHECK(writeMessage.find("type mismatch") != std::string::npos);
    CHECK(writeMessage.find("Sprite") != std::string::npos);
    CHECK(writeMessage.find("DataAsset") != std::string::npos);
    // RefError::Invalid is the kind EditorAutomationSurface::MapRefErrorCode reports as InvalidPayload.
    CHECK(writeError == ScenePropertyValueCodec::RefError::Invalid);

    // The slot keeps its previous (empty) value and the draw-side read cannot crash on it: the
    // historical failure was a crash inside ResolveDrawUVRect dereferencing the foreign pointer.
    const Vector4 uv = image->ResolveDrawUVRect();
    CHECK(uv.x == doctest::Approx(0.0f));
    CHECK(uv.y == doctest::Approx(0.0f));
    CHECK(uv.z == doctest::Approx(0.0f));
    CHECK(uv.w == doctest::Approx(0.0f));
    CHECK_FALSE(image->CanDraw());

    Json readBack;
    std::string readMessage;
    CHECK(ScenePropertyValueCodec::TryReadJson(
        *image, *ImageComponent::StaticClass(), "m_Sprite", readBack, readMessage, options));
    CHECK(readBack.is_null());
}

TEST_CASE("scene-property-ref-type: assetRef sugar of a live wrong class asset is rejected too [smoke]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedManagers managers;
    ScopedTempProject project;

    UnloadedAsset wrongClassAsset;
    REQUIRE(CreateUnloadedDataAsset(AssetManager::Get(), "SceneProp_AssetRef", wrongClassAsset));

    const std::shared_ptr<Scene> scene =
        SceneManager::Get().CreateNewScene("scene-property-ref-type-assetref");
    REQUIRE(scene != nullptr);
    const std::shared_ptr<GameObject> gameObject = scene->CreateGameObject();
    REQUIRE(gameObject != nullptr);
    const std::shared_ptr<ImageComponent> image = gameObject->AddComponent<ImageComponent>();
    REQUIRE(image != nullptr);

    Json normalizedValue;
    ScenePropertyValueCodec::RefError normalizeError = ScenePropertyValueCodec::RefError::None;
    std::string normalizeMessage;
    REQUIRE(ScenePropertyValueCodec::TryNormalizeValueReferences(
        Json{{"$assetRef", wrongClassAsset.Path}}, normalizedValue, normalizeError, normalizeMessage));

    const Serialization::SerializerOptions options{};
    ScenePropertyValueCodec::RefError writeError = ScenePropertyValueCodec::RefError::None;
    std::string writeMessage;
    const bool written = ScenePropertyValueCodec::TryWriteJsonToLive(
        *image,
        *ImageComponent::StaticClass(),
        "m_Sprite",
        normalizedValue,
        writeMessage,
        options,
        &writeError);

    CHECK_FALSE(written);
    CHECK(writeMessage.find("type mismatch") != std::string::npos);

    Json readBack;
    std::string readMessage;
    CHECK(ScenePropertyValueCodec::TryReadJson(
        *image, *ImageComponent::StaticClass(), "m_Sprite", readBack, readMessage, options));
    CHECK(readBack.is_null());
}
