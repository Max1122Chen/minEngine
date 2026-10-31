#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"
#include "EngineTestFixture.h"
#include "Runtime/Core/GUID/GUID.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/EngineConfig.h"
#include "Runtime/Function/Framework/Components/SpriteComponent.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Render/OpenGL/OpenGLRHI.h"
#include "Runtime/Function/Render/Sprite/Sprite.h"
#include "Runtime/Function/Render/Sprite/SpriteAtlas.h"
#include "Runtime/Function/Render/Sprite/SpriteMaterialFactory.h"
#include "Runtime/Resource/AssetSerializeHelper.h"
#include "Runtime/Resource/Loaders/ImageLoader.h"
#include "Runtime/Resource/Loaders/Texture2DLoader.h"

#include "doctest.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>

namespace
{
    class ScopedObjectManager
    {
    public:
        ScopedObjectManager()
        {
            minEngine::Testing::TestAccess<minEngine::ObjectManager>::SetInstance(&m_Manager);
            minEngine::ObjectManager::Initialize();
        }

        ~ScopedObjectManager()
        {
            minEngine::ObjectManager::Shutdown();
            minEngine::Testing::TestAccess<minEngine::ObjectManager>::SetInstance(nullptr);
        }

    private:
        minEngine::ObjectManager m_Manager;
    };

    class ScopedSceneManager
    {
    public:
        ScopedSceneManager()
        {
            minEngine::Testing::TestAccess<minEngine::SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();
        }

        ~ScopedSceneManager()
        {
            m_SceneManager.Shutdown();
            minEngine::Testing::TestAccess<minEngine::SceneManager>::SetInstance(nullptr);
        }

    private:
        minEngine::SceneManager m_SceneManager;
    };

    class ScopedHeadlessGlContext
    {
    public:
        ScopedHeadlessGlContext()
        {
            if (glfwGetCurrentContext() != nullptr)
            {
                return;
            }

            if (!glfwInit())
            {
                return;
            }

            m_OwnsGlfw = true;
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            m_Window = glfwCreateWindow(1, 1, "SpriteRuntimeConsumeTest", nullptr, nullptr);
            if (m_Window == nullptr)
            {
                return;
            }

            glfwMakeContextCurrent(m_Window);
            if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
            {
                glfwDestroyWindow(m_Window);
                m_Window = nullptr;
            }
        }

        ~ScopedHeadlessGlContext()
        {
            if (m_Window != nullptr)
            {
                glfwDestroyWindow(m_Window);
                m_Window = nullptr;
            }
            if (m_OwnsGlfw)
            {
                glfwTerminate();
            }
        }

        bool IsReady() const { return glfwGetCurrentContext() != nullptr; }

    private:
        GLFWwindow* m_Window = nullptr;
        bool m_OwnsGlfw = false;
    };

    minEngine::ImagePixels MakeSolidPixels(int width, int height)
    {
        minEngine::ImagePixels pixels;
        pixels.Storage = minEngine::ImageStorage::UInt8;
        pixels.Width = width;
        pixels.Height = height;
        pixels.Channels = 4;
        const size_t byteCount = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
        pixels.U8 = static_cast<unsigned char*>(std::malloc(byteCount));
        std::memset(pixels.U8, 255, byteCount);
        return pixels;
    }
}

TEST_CASE("sprite-runtime-consume: degenerate UV detection [smoke]")
{
    using namespace minEngine;

    CHECK(SpriteComponent::IsUVRectDegenerate(Vector4(0.0f, 0.0f, 0.0f, 1.0f)));
    CHECK(SpriteComponent::IsUVRectDegenerate(Vector4(0.0f, 0.5f, 1.0f, 0.5f)));
    CHECK_FALSE(SpriteComponent::IsUVRectDegenerate(Vector4(0.0f, 0.0f, 1.0f, 1.0f)));
    CHECK_FALSE(SpriteComponent::IsUVRectDegenerate(Vector4(0.25f, 0.25f, 0.75f, 0.75f)));
}

TEST_CASE("sprite-runtime-consume: resolve prefers sprite runtime over legacy [smoke]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedObjectManager objectManagerScope;
    ScopedSceneManager sceneManagerScope;
    ScopedHeadlessGlContext gl;
    REQUIRE(gl.IsReady());
    OpenGLRHI rhi;

    ImagePixels pixels = MakeSolidPixels(8, 8);
    std::shared_ptr<Texture2D> sourceTex =
        Texture2DLoader::CreateFromPixels(rhi, pixels, "SpriteConsume_Src", GenerateGUID());
    ImageLoader::Free(pixels);
    const bool hasSource = sourceTex.get() != nullptr;
    REQUIRE(hasSource);

    std::shared_ptr<Sprite> sprite = NewObject<Sprite>("ConsumeSprite");
    sprite->ConfigureFromTexture(sourceTex, Vector4(0.0f, 0.0f, 4.0f, 4.0f));

    std::shared_ptr<SpriteComponent> component = NewObject<SpriteComponent>("ConsumeComp");
    component->SetTexture(sourceTex);
    component->SetUVRect(Vector4(0.0f, 0.0f, 1.0f, 1.0f));

    const Vector4 legacyUv = component->ResolveDrawUVRect();
    CHECK(legacyUv.z == doctest::Approx(1.0f));

    component->SetSprite(sprite);
    const Vector4 spriteUv = component->ResolveDrawUVRect();
    // 4x4 on 8x8 top-left → u0=0, u1=0.5, v0=0.5, v1=1 (V flip)
    CHECK(spriteUv.x == doctest::Approx(0.0f));
    CHECK(spriteUv.y == doctest::Approx(0.5f));
    CHECK(spriteUv.z == doctest::Approx(0.5f));
    CHECK(spriteUv.w == doctest::Approx(1.0f));
    const bool textureMatches = component->ResolveDrawTexture() == sourceTex.get();
    CHECK(textureMatches);
}

TEST_CASE("sprite-runtime-consume: sprite material with UVRect compiles [full]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());

    EngineConfig engineConfig;
    const bool configLoaded = PathRegistry::Get().LoadEngineConfiguration(
        reflectionFixture.GetArgc(),
        reflectionFixture.GetArgv(),
        engineConfig);
    if (!configLoaded || PathRegistry::Get().GetEngineDefaultAssetsRoot().empty())
    {
        MESSAGE("Skipping UV material compile: EngineDefaultAssetsRoot unavailable");
        return;
    }

    ScopedObjectManager objectManagerScope;
    ScopedHeadlessGlContext gl;
    REQUIRE(gl.IsReady());
    OpenGLRHI rhi;

    std::shared_ptr<Material> opaque = SpriteMaterialFactory::CreateInstance(rhi, false);
    std::shared_ptr<Material> translucent = SpriteMaterialFactory::CreateInstance(rhi, true);
    const bool opaqueOk = opaque && opaque->IsCompiledForDraw();
    const bool translucentOk = translucent && translucent->IsCompiledForDraw();
    REQUIRE(opaqueOk);
    REQUIRE(translucentOk);

    ImagePixels pixels = MakeSolidPixels(4, 4);
    std::shared_ptr<Texture2D> texture =
        Texture2DLoader::CreateFromPixels(rhi, pixels, "SpriteConsume_Mat", GenerateGUID());
    ImageLoader::Free(pixels);

    SpriteMaterialFactory::ApplyColorTextureAndUVRect(
        *opaque,
        LinearColor(1.0f, 1.0f, 1.0f, 1.0f),
        texture,
        Vector4(0.25f, 0.25f, 0.75f, 0.75f));
    CHECK(opaque->IsCompiledForDraw());
}


namespace
{
    /** Committed sprite fixtures sit in the source tree; the binary runs from the build's bin dir. */
    std::filesystem::path LocateSpriteFixtureDirectory()
    {
        std::filesystem::path dir = std::filesystem::current_path();
        for (int depth = 0; depth < 6; ++depth)
        {
            const std::filesystem::path direct = dir / "Tests" / "Fixtures" / "Sprites";
            if (std::filesystem::is_directory(direct))
            {
                return direct;
            }

            const std::filesystem::path nested = dir / "minEngine" / "Tests" / "Fixtures" / "Sprites";
            if (std::filesystem::is_directory(nested))
            {
                return nested;
            }

            if (!dir.has_parent_path() || dir.parent_path() == dir)
            {
                break;
            }

            dir = dir.parent_path();
        }

        return std::filesystem::path();
    }
}

TEST_CASE("sprite-runtime-consume: a sprite file that stored baked fields still loads [full]")
{
    using namespace minEngine;

    const std::filesystem::path fixturePath =
        LocateSpriteFixtureDirectory() / "legacy_baked_sprite.mesprite";
    INFO("fixture: " << fixturePath.string());
    REQUIRE(std::filesystem::is_regular_file(fixturePath));

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedObjectManager objectManagerScope;
    ScopedHeadlessGlContext gl;
    REQUIRE(gl.IsReady());
    OpenGLRHI rhi;

    Json root;
    REQUIRE(AssetSerializeHelper::ReadJsonFromFile(fixturePath.string(), root));
    // The fixture proves nothing once it stops carrying the baked fields; it is captured data,
    // not something this test may rewrite.
    REQUIRE(root.contains("m_BakedTexture"));
    REQUIRE(root["m_BakedTexture"].contains("$guid"));
    REQUIRE(root.contains("m_SourceTexture"));

    // Recreate the authored source texture under the guid the file names, so the baked page is the
    // only reference the load cannot resolve.
    const GUID sourceGuid(
        root["m_SourceTexture"]["$guid"]["high"].get<uint64_t>(),
        root["m_SourceTexture"]["$guid"]["low"].get<uint64_t>());
    REQUIRE_FALSE(sourceGuid.IsZero());

    ImagePixels pixels = MakeSolidPixels(8, 8);
    std::shared_ptr<Texture2D> source =
        Texture2DLoader::CreateFromPixels(rhi, pixels, "LegacyBakedSource", GenerateGUID());
    REQUIRE(source != nullptr);
    // The loader registers under a fresh guid; give the texture the identity the file names, so the
    // authored reference resolves and the baked page stays the only unresolvable one.
    REQUIRE(ObjectManager::Get().RemapObjectGuid(source, sourceGuid));

    std::shared_ptr<Sprite> loaded = NewObject<Sprite>("LegacyBakedSpriteReloaded");
    std::string readError;
    INFO(readError);
    REQUIRE(AssetSerializeHelper::ReadObjectFromFile(fixturePath.string(), *loaded, &readError));
    CHECK(readError.empty());

    // The baked half is gone, the authored half is intact and points at a live texture.
    CHECK(loaded->GetBakedTexture() == nullptr);
    CHECK(loaded->GetBakedRectPx() == Vector4(0.0f, 0.0f, 0.0f, 0.0f));
    CHECK(loaded->GetSourceRectPx() == Vector4(0.0f, 0.0f, 4.0f, 4.0f));
    REQUIRE(loaded->GetSourceTexture() == source.get());
    CHECK(loaded->GetRuntimeTexture() == source.get());

    // 4x4 at the top left of an 8x8 texture, no bake: V flips to 0.5..1.
    const Vector4 unBakedUv = loaded->GetUVRectNormalized();
    CHECK(unBakedUv.x == doctest::Approx(0.0f));
    CHECK(unBakedUv.y == doctest::Approx(0.5f));
    CHECK(unBakedUv.z == doctest::Approx(0.5f));
    CHECK(unBakedUv.w == doctest::Approx(1.0f));

    // Baking works again from scratch: the atlas rebuild owns the baked pair.
    REQUIRE(loaded->SetAuthoringPixels(pixels));
    ImageLoader::Free(pixels);
    std::shared_ptr<SpriteAtlas> atlas = NewObject<SpriteAtlas>("LegacyBakedAtlasReloaded");
    atlas->SetMaxSize(64);
    REQUIRE(atlas->AddMember(loaded));
    const SpriteAtlasRebuildResult rebuilt = atlas->Rebuild(rhi);
    INFO(rebuilt.Message);
    REQUIRE(rebuilt.Ok);
    REQUIRE(atlas->GetGeneratedTextures().size() == 1);
    REQUIRE(loaded->GetBakedTexture() != nullptr);
    CHECK(loaded->GetRuntimeTexture() == atlas->GetGeneratedTextures()[0].get());

    const std::shared_ptr<Texture2D> page = atlas->GetGeneratedTextures()[0];
    const Vector4 expectedUv = Sprite::PixelRectToUvRectTopLeft(
        loaded->GetBakedRectPx(),
        page->GetWidth(),
        page->GetHeight());
    const Vector4 bakedUv = loaded->GetUVRectNormalized();
    CHECK(bakedUv == expectedUv);
    CHECK(loaded->GetBakedRectPx().z == doctest::Approx(4.0f));
    CHECK(loaded->GetBakedRectPx().w == doctest::Approx(4.0f));
}
