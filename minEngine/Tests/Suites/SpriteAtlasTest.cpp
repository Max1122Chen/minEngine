#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/GUID/GUID.h"
#include "Runtime/Function/Render/OpenGL/OpenGLRHI.h"
#include "Runtime/Function/Render/Sprite/Sprite.h"
#include "Runtime/Function/Render/Sprite/SpriteAtlas.h"
#include "Runtime/Function/Render/Sprite/SpriteAtlasPacker.h"
#include "Runtime/Resource/Loaders/ImageLoader.h"
#include "Runtime/Resource/Loaders/Texture2DLoader.h"
#include "Access/ObjectManagerTestAccess.h"
#include "EngineTestFixture.h"

#include "doctest.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <cstdlib>
#include <cstring>
#include <vector>

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
            m_Window = glfwCreateWindow(1, 1, "SpriteAtlasTest", nullptr, nullptr);
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

    minEngine::ImagePixels MakeSolidPixels(int width, int height, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        minEngine::ImagePixels pixels;
        pixels.Storage = minEngine::ImageStorage::UInt8;
        pixels.Width = width;
        pixels.Height = height;
        pixels.Channels = 4;
        const size_t byteCount = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
        pixels.U8 = static_cast<unsigned char*>(std::malloc(byteCount));
        for (size_t i = 0; i < byteCount; i += 4)
        {
            pixels.U8[i + 0] = r;
            pixels.U8[i + 1] = g;
            pixels.U8[i + 2] = b;
            pixels.U8[i + 3] = a;
        }
        return pixels;
    }
}

TEST_CASE("sprite-atlas: uv from top-left rect flips V for OpenGL [smoke]")
{
    using namespace minEngine;

    // Full texture
    const Vector4 full = Sprite::PixelRectToUvRectTopLeft(Vector4(0, 0, 64, 32), 64, 32);
    CHECK(full.x == doctest::Approx(0.0f));
    CHECK(full.z == doctest::Approx(1.0f));
    CHECK(full.y == doctest::Approx(0.0f));
    CHECK(full.w == doctest::Approx(1.0f));

    // Top-left 32x16 of 64x32 → v near top of GL texture
    const Vector4 top = Sprite::PixelRectToUvRectTopLeft(Vector4(0, 0, 32, 16), 64, 32);
    CHECK(top.x == doctest::Approx(0.0f));
    CHECK(top.z == doctest::Approx(0.5f));
    CHECK(top.y == doctest::Approx(0.5f));
    CHECK(top.w == doctest::Approx(1.0f));
}

TEST_CASE("sprite-atlas: runtime falls back to source before rebuild [smoke]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedObjectManager objectManagerScope;
    ScopedHeadlessGlContext gl;
    REQUIRE(gl.IsReady());
    OpenGLRHI rhi;
    ImagePixels pixels = MakeSolidPixels(8, 8, 255, 0, 0, 255);
    std::shared_ptr<Texture2D> texture =
        Texture2DLoader::CreateFromPixels(rhi, pixels, "SpriteAtlasTest_Src", GenerateGUID());
    ImageLoader::Free(pixels);
    const bool hasTexture = texture.get() != nullptr;
    REQUIRE(hasTexture);

    std::shared_ptr<Sprite> sprite = NewObject<Sprite>("TestSprite");
    sprite->ConfigureFromTexture(texture, Vector4(0, 0, 0, 0));
    const bool runtimeIsSource = sprite->GetRuntimeTexture() == texture.get();
    const bool bakedIsNull = sprite->GetBakedTexture() == nullptr;
    CHECK(runtimeIsSource);
    CHECK(bakedIsNull);

    const Vector4 uv = sprite->GetUVRectNormalized();
    CHECK(uv.x == doctest::Approx(0.0f));
    CHECK(uv.y == doctest::Approx(0.0f));
    CHECK(uv.z == doctest::Approx(1.0f));
    CHECK(uv.w == doctest::Approx(1.0f));
}

TEST_CASE("sprite-atlas: rebuild packs two sprites onto one texture [full]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedObjectManager objectManagerScope;
    ScopedHeadlessGlContext gl;
    REQUIRE(gl.IsReady());
    OpenGLRHI rhi;

    ImagePixels redPixels = MakeSolidPixels(4, 4, 255, 0, 0, 255);
    ImagePixels greenPixels = MakeSolidPixels(4, 4, 0, 255, 0, 255);
    std::shared_ptr<Texture2D> redTex =
        Texture2DLoader::CreateFromPixels(rhi, redPixels, "SpriteAtlasTest_Red", GenerateGUID());
    std::shared_ptr<Texture2D> greenTex =
        Texture2DLoader::CreateFromPixels(rhi, greenPixels, "SpriteAtlasTest_Green", GenerateGUID());

    std::shared_ptr<Sprite> red = NewObject<Sprite>("RedSprite");
    std::shared_ptr<Sprite> green = NewObject<Sprite>("GreenSprite");
    red->ConfigureFromTexture(redTex, Vector4(0, 0, 4, 4));
    green->ConfigureFromTexture(greenTex, Vector4(0, 0, 4, 4));
    REQUIRE(red->SetAuthoringPixels(redPixels));
    REQUIRE(green->SetAuthoringPixels(greenPixels));
    ImageLoader::Free(redPixels);
    ImageLoader::Free(greenPixels);

    std::shared_ptr<Texture2D> sourceRedBefore = red->GetSourceTextureShared();
    std::shared_ptr<Texture2D> sourceGreenBefore = green->GetSourceTextureShared();

    std::shared_ptr<SpriteAtlas> atlas = NewObject<SpriteAtlas>("TestAtlas");
    atlas->SetMaxSize(64);
    atlas->SetPadding(1);
    REQUIRE(atlas->AddMember(red));
    REQUIRE(atlas->AddMember(green));

    const SpriteAtlasRebuildResult rebuild = atlas->Rebuild(rhi);
    REQUIRE(rebuild.Ok);
    REQUIRE(atlas->GetGeneratedTextures().size() == 1);

    Texture2D* bakedTex = atlas->GetGeneratedTextures()[0].get();
    const bool redUsesBaked = red->GetRuntimeTexture() == bakedTex;
    const bool greenUsesBaked = green->GetRuntimeTexture() == bakedTex;
    const bool sameRuntime = red->GetRuntimeTexture() == green->GetRuntimeTexture();
    const bool redSourceUnchanged = red->GetSourceTextureShared().get() == sourceRedBefore.get();
    const bool greenSourceUnchanged = green->GetSourceTextureShared().get() == sourceGreenBefore.get();
    CHECK(redUsesBaked);
    CHECK(greenUsesBaked);
    CHECK(sameRuntime);
    CHECK(redSourceUnchanged);
    CHECK(greenSourceUnchanged);

    const Vector4 redUv = red->GetUVRectNormalized();
    const Vector4 greenUv = green->GetUVRectNormalized();
    const bool uvDiffer =
        redUv.x != greenUv.x || redUv.y != greenUv.y || redUv.z != greenUv.z || redUv.w != greenUv.w;
    CHECK(uvDiffer);
}

TEST_CASE("sprite-atlas: remove member clears baked [full]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedObjectManager objectManagerScope;
    ScopedHeadlessGlContext gl;
    REQUIRE(gl.IsReady());
    OpenGLRHI rhi;
    ImagePixels pixels = MakeSolidPixels(4, 4, 0, 0, 255, 255);
    std::shared_ptr<Texture2D> texture =
        Texture2DLoader::CreateFromPixels(rhi, pixels, "SpriteAtlasTest_Blue", GenerateGUID());

    std::shared_ptr<Sprite> sprite = NewObject<Sprite>("BlueSprite");
    sprite->ConfigureFromTexture(texture, Vector4(0, 0, 4, 4));
    REQUIRE(sprite->SetAuthoringPixels(pixels));
    ImageLoader::Free(pixels);

    std::shared_ptr<SpriteAtlas> atlas = NewObject<SpriteAtlas>("RemoveAtlas");
    REQUIRE(atlas->AddMember(sprite));
    REQUIRE(atlas->Rebuild(rhi).Ok);
    const bool bakedAfterRebuild = sprite->GetBakedTexture() != nullptr;
    REQUIRE(bakedAfterRebuild);

    REQUIRE(atlas->RemoveMember(sprite));
    const bool bakedCleared = sprite->GetBakedTexture() == nullptr;
    const bool atlasCleared = sprite->GetAtlas() == nullptr;
    const bool runtimeBackToSource = sprite->GetRuntimeTexture() == texture.get();
    CHECK(bakedCleared);
    CHECK(atlasCleared);
    CHECK(runtimeBackToSource);
}

TEST_CASE("sprite-atlas: rebuild fails transactionally when MaxSize too small [full]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedObjectManager objectManagerScope;
    ScopedHeadlessGlContext gl;
    REQUIRE(gl.IsReady());
    OpenGLRHI rhi;
    ImagePixels pixels = MakeSolidPixels(16, 16, 128, 128, 128, 255);
    std::shared_ptr<Texture2D> texture =
        Texture2DLoader::CreateFromPixels(rhi, pixels, "SpriteAtlasTest_Big", GenerateGUID());

    std::shared_ptr<Sprite> sprite = NewObject<Sprite>("BigSprite");
    sprite->ConfigureFromTexture(texture, Vector4(0, 0, 16, 16));
    REQUIRE(sprite->SetAuthoringPixels(pixels));
    ImageLoader::Free(pixels);

    std::shared_ptr<SpriteAtlas> atlas = NewObject<SpriteAtlas>("FailAtlas");
    atlas->SetMaxSize(8);
    REQUIRE(atlas->AddMember(sprite));

    const SpriteAtlasRebuildResult rebuild = atlas->Rebuild(rhi);
    CHECK_FALSE(rebuild.Ok);
    const bool bakedStillNull = sprite->GetBakedTexture() == nullptr;
    CHECK(bakedStillNull);
    CHECK(atlas->GetGeneratedTextures().empty());
}

TEST_CASE("sprite-atlas: shelf pack places non-overlapping content rects with inset padding [smoke]")
{
    using namespace minEngine;

    std::vector<SpritePackLayoutEntry> sizes;
    sizes.push_back(SpritePackLayoutEntry{0, 8, 8, 0, 0});
    sizes.push_back(SpritePackLayoutEntry{1, 8, 8, 0, 0});

    const SpritePackLayoutResult layout = SpriteAtlasPacker::PackShelf(sizes, 32, 1);
    REQUIRE(layout.Ok);
    REQUIRE(layout.Entries.size() == 2);
    CHECK(layout.AtlasWidth == 20);
    CHECK(layout.AtlasHeight == 10);

    const SpritePackLayoutEntry& a = layout.Entries[0];
    const SpritePackLayoutEntry& b = layout.Entries[1];
    CHECK(a.X == 1);
    CHECK(a.Y == 1);
    CHECK(b.X == 11);
    CHECK(b.Y == 1);
    const bool overlap =
        a.X < b.X + b.Width && a.X + a.Width > b.X && a.Y < b.Y + b.Height && a.Y + a.Height > b.Y;
    CHECK_FALSE(overlap);
}

TEST_CASE("sprite-atlas: DilateBorderRing copies edge into exclusive pad ring [smoke]")
{
    using namespace minEngine;

    ImagePixels atlas;
    atlas.Storage = ImageStorage::UInt8;
    atlas.Width = 5;
    atlas.Height = 5;
    atlas.Channels = 4;
    const size_t byteCount = 5u * 5u * 4u;
    atlas.U8 = static_cast<unsigned char*>(std::malloc(byteCount));
    REQUIRE(atlas.U8 != nullptr);
    std::memset(atlas.U8, 0, byteCount);

    // Center 3x3 content solid red.
    for (int32_t y = 1; y <= 3; ++y)
    {
        for (int32_t x = 1; x <= 3; ++x)
        {
            const size_t i = (static_cast<size_t>(y) * 5u + static_cast<size_t>(x)) * 4u;
            atlas.U8[i + 0] = 255;
            atlas.U8[i + 1] = 0;
            atlas.U8[i + 2] = 0;
            atlas.U8[i + 3] = 255;
        }
    }

    SpriteAtlasPacker::DilateBorderRing(atlas, 1, 1, 3, 3, 1);

    auto sample = [&](int32_t x, int32_t y) -> unsigned char
    {
        return atlas.U8[(static_cast<size_t>(y) * 5u + static_cast<size_t>(x)) * 4u];
    };

    CHECK(sample(0, 2) == 255); // left ring
    CHECK(sample(4, 2) == 255); // right ring
    CHECK(sample(2, 0) == 255); // bottom (memory -Y) ring
    CHECK(sample(2, 4) == 255); // top (memory +Y) ring
    CHECK(sample(0, 0) == 255); // corner
    CHECK(sample(4, 4) == 255);

    ImageLoader::Free(atlas);
}
