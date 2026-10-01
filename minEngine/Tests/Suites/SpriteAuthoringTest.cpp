#include "Access/AssetManagerTestAccess.h"
#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"
#include "EngineTestFixture.h"

#include "Services/Asset/AssetEditorServices.h"
#include "Services/Sprite/SpriteEditorServices.h"
#include "Shell/EditorCommandStack.h"

#include "Runtime/Core/GUID/GUID.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/EngineConfig.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Render/OpenGL/OpenGLRHI.h"
#include "Runtime/Function/Render/Sprite/Sprite.h"
#include "Runtime/Function/Render/Sprite/SpriteAtlas.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/Loaders/ImageLoader.h"
#include "Runtime/Resource/Loaders/Texture2DLoader.h"

#include "doctest.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <vector>

namespace
{
    class ScopedManagers
    {
    public:
        ScopedManagers()
        {
            minEngine::Testing::TestAccess<minEngine::ObjectManager>::SetInstance(&m_ObjectManager);
            m_ObjectManager.Initialize();
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
            m_ObjectManager.Shutdown();
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
                std::filesystem::temp_directory_path() / "minEngine_SpriteAuthoringTest";
            std::error_code removeError;
            std::filesystem::remove_all(tempRoot, removeError);
            m_ProjectRoot = tempRoot / "Project";
            std::filesystem::create_directories(m_ProjectRoot / "Assets" / "Sprites");
            minEngine::PathRegistry::Get().SetProjectRoots(m_ProjectRoot);
        }

        ~ScopedTempProject()
        {
            minEngine::PathRegistry::Get().ClearProjectRoots();
            std::error_code removeError;
            std::filesystem::remove_all(
                std::filesystem::temp_directory_path() / "minEngine_SpriteAuthoringTest",
                removeError);
        }

        std::string GetSpritesDir() const { return "Assets/Sprites"; }

    private:
        std::filesystem::path m_ProjectRoot;
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
            m_Window = glfwCreateWindow(1, 1, "SpriteAuthoringTest", nullptr, nullptr);
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

TEST_CASE("sprite-authoring: set source rect undo [smoke]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedManagers managers;
    ScopedHeadlessGlContext gl;
    REQUIRE(gl.IsReady());
    OpenGLRHI rhi;

    ImagePixels pixels = MakeSolidPixels(8, 8);
    std::shared_ptr<Texture2D> texture =
        Texture2DLoader::CreateFromPixels(rhi, pixels, "Authoring_UndoTex", GenerateGUID());
    ImageLoader::Free(pixels);
    REQUIRE(texture.get() != nullptr);

    std::shared_ptr<Sprite> sprite = NewObject<Sprite>("AuthoringUndoSprite");
    sprite->ConfigureFromTexture(texture, Vector4(0.0f, 0.0f, 8.0f, 8.0f));

    EditorCommandStack stack;
    SpriteEditorServices services;
    const GraphEditorResult setResult =
        services.SetSpriteSourceRect(stack, *sprite, Vector4(1.0f, 2.0f, 4.0f, 4.0f));
    REQUIRE(setResult.Ok);
    CHECK(sprite->GetSourceRectPx().x == doctest::Approx(1.0f));
    CHECK(sprite->GetSourceRectPx().y == doctest::Approx(2.0f));

    REQUIRE(stack.CanUndo());
    REQUIRE(stack.Undo());
    CHECK(sprite->GetSourceRectPx().x == doctest::Approx(0.0f));
    CHECK(sprite->GetSourceRectPx().z == doctest::Approx(8.0f));
}

TEST_CASE("sprite-authoring: create atlas add rebuild pipeline [full]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());

    EngineConfig engineConfig;
    const bool configLoaded = PathRegistry::Get().LoadEngineConfiguration(
        reflectionFixture.GetArgc(),
        reflectionFixture.GetArgv(),
        engineConfig);
    if (!configLoaded)
    {
        MESSAGE("Skipping authoring pipeline: EngineConfig unavailable");
        return;
    }

    ScopedManagers managers;
    ScopedTempProject project;
    ScopedHeadlessGlContext gl;
    REQUIRE(gl.IsReady());
    OpenGLRHI rhi;

    ImagePixels pixels = MakeSolidPixels(4, 4);
    std::shared_ptr<Texture2D> texture =
        Texture2DLoader::CreateFromPixels(rhi, pixels, "Authoring_PipeTex", GenerateGUID());
    REQUIRE(texture.get() != nullptr);

    AssetEditorServices assetServices;
    SpriteEditorServices services;
    std::shared_ptr<Sprite> sprite;
    std::string spritePath;
    const GraphEditorResult createSprite = services.CreateSpriteFromTexture(
        assetServices,
        texture,
        Vector4(0.0f, 0.0f, 4.0f, 4.0f),
        project.GetSpritesDir(),
        "PipeSprite",
        &sprite,
        &spritePath,
        nullptr);
    REQUIRE(createSprite.Ok);
    REQUIRE(sprite);
    REQUIRE_FALSE(spritePath.empty());
    REQUIRE(sprite->SetAuthoringPixels(pixels));
    ImageLoader::Free(pixels);

    std::shared_ptr<SpriteAtlas> atlas;
    std::string atlasPath;
    const GraphEditorResult createAtlas =
        services.CreateAtlas(assetServices, project.GetSpritesDir(), "PipeAtlas", &atlas, &atlasPath, nullptr);
    REQUIRE(createAtlas.Ok);
    REQUIRE(atlas);
    atlas->SetMaxSize(64);
    atlas->SetPadding(1);

    EditorCommandStack stack;
    const GraphEditorResult addMembers =
        services.AddAtlasMembers(stack, *atlas, { sprite });
    REQUIRE(addMembers.Ok);
    CHECK(atlas->GetMembers().size() == 1);

    const GraphEditorResult rebuild = services.RebuildAtlas(*atlas, rhi);
    REQUIRE(rebuild.Ok);
    REQUIRE(atlas->GetGeneratedTextures().size() == 1);
    CHECK(sprite->GetRuntimeTexture() == atlas->GetGeneratedTextures()[0].get());

    const Vector4 uv = sprite->GetUVRectNormalized();
    CHECK(uv.z > uv.x);
    CHECK(uv.w > uv.y);

    REQUIRE(stack.CanUndo());
    REQUIRE(stack.Undo());
    CHECK(atlas->GetMembers().empty());
}

TEST_CASE("sprite-authoring: reject member owned by other atlas [smoke]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedManagers managers;

    std::shared_ptr<Sprite> sprite = NewObject<Sprite>("OwnedSprite");
    std::shared_ptr<SpriteAtlas> atlasA = NewObject<SpriteAtlas>("AtlasA");
    std::shared_ptr<SpriteAtlas> atlasB = NewObject<SpriteAtlas>("AtlasB");
    REQUIRE(atlasA->AddMember(sprite));

    EditorCommandStack stack;
    SpriteEditorServices services;
    const GraphEditorResult addOther = services.AddAtlasMembers(stack, *atlasB, { sprite });
    CHECK_FALSE(addOther.Ok);
    CHECK(atlasB->GetMembers().empty());
}
