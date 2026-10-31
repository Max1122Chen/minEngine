#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/GUID/GUID.h"
#include "Runtime/Function/Render/OpenGL/OpenGLRHI.h"
#include "Runtime/Function/Render/Sprite/Sprite.h"
#include "Runtime/Function/Render/Sprite/SpriteAtlas.h"
#include "Runtime/Function/Render/Sprite/SpriteAtlasPacker.h"
#include "Runtime/Resource/Loaders/ImageLoader.h"
#include "Runtime/Resource/Loaders/Texture2DLoader.h"
#include "Runtime/Resource/AssetSerializeHelper.h"
#include "Access/ObjectManagerTestAccess.h"
#include "EngineTestFixture.h"

#include "doctest.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

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

TEST_CASE("sprite-atlas: rebuild applies authored sampler modes [full]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedObjectManager objectManagerScope;
    ScopedHeadlessGlContext gl;
    REQUIRE(gl.IsReady());
    OpenGLRHI rhi;

    ImagePixels pixels = MakeSolidPixels(4, 4, 255, 0, 0, 255);
    std::shared_ptr<Texture2D> source =
        Texture2DLoader::CreateFromPixels(rhi, pixels, "SpriteAtlasSampler_Src", GenerateGUID());
    std::shared_ptr<Sprite> sprite = NewObject<Sprite>("SamplerSprite");
    sprite->ConfigureFromTexture(source, Vector4(0, 0, 4, 4));
    REQUIRE(sprite->SetAuthoringPixels(pixels));
    ImageLoader::Free(pixels);

    std::shared_ptr<SpriteAtlas> atlas = NewObject<SpriteAtlas>("SamplerAtlas");
    REQUIRE(atlas->AddMember(sprite));

    // Defaults favor crisp pixel pages.
    CHECK(atlas->GetSamplerFilter() == RHISamplerFilter::Nearest);
    CHECK(atlas->GetSamplerAddress() == RHISamplerAddress::ClampToEdge);
    REQUIRE(atlas->Rebuild(rhi).Ok);
    REQUIRE(atlas->GetGeneratedTextures().size() == 1);
    {
        const Texture2D& page = *atlas->GetGeneratedTextures()[0];
        CHECK(page.GetSamplerFilter() == RHISamplerFilter::Nearest);
        CHECK(page.GetSamplerAddress() == RHISamplerAddress::ClampToEdge);
        CHECK(page.GetPreferredSamplerDesc().MagFilter == RHISamplerFilter::Nearest);
        CHECK(page.GetPreferredSamplerDesc().AddressU == RHISamplerAddress::ClampToEdge);
    }

    atlas->SetSamplerFilter(RHISamplerFilter::Linear);
    atlas->SetSamplerAddress(RHISamplerAddress::Repeat);
    REQUIRE(atlas->Rebuild(rhi).Ok);
    {
        const Texture2D& page = *atlas->GetGeneratedTextures()[0];
        CHECK(page.GetSamplerFilter() == RHISamplerFilter::Linear);
        CHECK(page.GetSamplerAddress() == RHISamplerAddress::Repeat);
    }

    // Live setters update existing pages without requiring another Rebuild.
    atlas->SetSamplerFilter(RHISamplerFilter::Nearest);
    atlas->SetSamplerAddress(RHISamplerAddress::ClampToBorder);
    {
        const Texture2D& page = *atlas->GetGeneratedTextures()[0];
        CHECK(page.GetSamplerFilter() == RHISamplerFilter::Nearest);
        CHECK(page.GetSamplerAddress() == RHISamplerAddress::ClampToBorder);
    }
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


namespace
{
    std::string ReadTextFile(const std::filesystem::path& path)
    {
        std::ifstream in(path);
        if (!in)
        {
            return std::string();
        }
        return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }

    void WriteTextFile(const std::filesystem::path& path, const std::string& text)
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << text;
    }

    std::shared_ptr<minEngine::Sprite> MakeAtlasMemberSprite(
        minEngine::RHI& rhi,
        const std::string& name,
        int32_t size,
        const Vector4& sourceRectPx)
    {
        minEngine::ImagePixels pixels = MakeSolidPixels(size, size, 200, 40, 40, 255);
        std::shared_ptr<minEngine::Texture2D> source =
            minEngine::Texture2DLoader::CreateFromPixels(rhi, pixels, name + "_Src", minEngine::GenerateGUID());
        std::shared_ptr<minEngine::Sprite> sprite = minEngine::NewObject<minEngine::Sprite>(name);
        sprite->ConfigureFromTexture(source, sourceRectPx);
        sprite->SetAuthoringPixels(pixels);
        minEngine::ImageLoader::Free(pixels);
        return sprite;
    }
}

TEST_CASE("sprite-atlas: saved asset carries no runtime pages [full]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedObjectManager objectManagerScope;
    ScopedHeadlessGlContext gl;
    REQUIRE(gl.IsReady());
    OpenGLRHI rhi;

    std::shared_ptr<Sprite> sprite = MakeAtlasMemberSprite(rhi, "RuntimePageSprite", 4, Vector4(0, 0, 4, 4));
    REQUIRE(sprite != nullptr);

    std::shared_ptr<SpriteAtlas> atlas = NewObject<SpriteAtlas>("RuntimePageAtlas");
    atlas->SetMaxSize(64);
    REQUIRE(atlas->AddMember(sprite));
    REQUIRE(atlas->Rebuild(rhi).Ok);
    REQUIRE(atlas->GetGeneratedTextures().size() == 1);

    const std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / "minEngine_sprite_atlas_runtime_page.mespriteatlas";
    std::string writeError;
    REQUIRE(AssetSerializeHelper::WriteObjectToFile(tempPath.string(), *atlas, &writeError));

    const std::string saved = ReadTextFile(tempPath);
    REQUIRE_FALSE(saved.empty());
    // The page set is rebuilt at start-up; persisting it only stores guids of unregistered textures.
    CHECK(saved.find("m_GeneratedTextures") == std::string::npos);
    CHECK(saved.find("m_Members") != std::string::npos);

    std::shared_ptr<SpriteAtlas> loaded = NewObject<SpriteAtlas>("RuntimePageAtlasReloaded");
    std::string readError;
    INFO(readError);
    REQUIRE(AssetSerializeHelper::ReadObjectFromFile(tempPath.string(), *loaded, &readError));
    CHECK(loaded->GetGeneratedTextures().empty());
    CHECK(loaded->GetSlots().empty());
    REQUIRE(loaded->GetMembers().size() == 1);
    CHECK(loaded->GetMembers()[0].get() == sprite.get());

    // Loading leaves the pages absent on purpose: the first Rebuild supplies them.
    const SpriteAtlasRebuildResult rebuilt = loaded->Rebuild(rhi);
    CHECK(rebuilt.Ok);
    CHECK(loaded->GetGeneratedTextures().size() == 1);

    std::error_code removeError;
    std::filesystem::remove(tempPath, removeError);
}

TEST_CASE("sprite-atlas: legacy asset with page guids still loads [full]")
{
    using namespace minEngine;



    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedObjectManager objectManagerScope;
    ScopedHeadlessGlContext gl;
    REQUIRE(gl.IsReady());
    OpenGLRHI rhi;

    std::shared_ptr<Sprite> sprite = MakeAtlasMemberSprite(rhi, "LegacyAtlasSprite", 4, Vector4(0, 0, 4, 4));
    REQUIRE(sprite != nullptr);

    // Shape of an asset saved before the page set became runtime-only: the member reference plus a
    // guid array of page textures that no longer resolve to any object or registered asset.
    const GUID memberGuid = sprite->GetGuid();
    std::ostringstream legacy;
    legacy << "{\n"
           << "  \"$engineVersion\": \"0.0.9\",\n"
           << "  \"$schemaVersion\": 1,\n"
           << "  \"m_GeneratedTextures\": [ { \"$guid\": { \"high\": 424242, \"low\": 848484 } } ],\n"
           << "  \"m_MaxSize\": 64,\n"
           << "  \"m_Members\": [ { \"$guid\": { \"high\": " << memberGuid.High
           << ", \"low\": " << memberGuid.Low << " } } ],\n"
           << "  \"m_Padding\": 1,\n"
           << "  \"m_PaddingMode\": 0,\n"
           << "  \"m_SamplerAddress\": 2,\n"
           << "  \"m_SamplerFilter\": 0\n"
           << "}\n";

    const std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / "minEngine_sprite_atlas_legacy_pages.mespriteatlas";
    WriteTextFile(tempPath, legacy.str());
    REQUIRE_FALSE(ReadTextFile(tempPath).empty());

    std::shared_ptr<SpriteAtlas> loaded = NewObject<SpriteAtlas>("LegacyPageAtlas");
    std::string readError;
    INFO(readError);
    REQUIRE(AssetSerializeHelper::ReadObjectFromFile(tempPath.string(), *loaded, &readError));
    CHECK(loaded->GetGeneratedTextures().empty());
    CHECK(loaded->GetSlots().empty());
    REQUIRE(loaded->GetMembers().size() == 1);
    REQUIRE(loaded->GetMembers()[0] != nullptr);
    CHECK(loaded->GetMembers()[0].get() == sprite.get());
    CHECK(loaded->GetMaxSize() == 64);
    CHECK(loaded->GetPadding() == 1);
    CHECK(loaded->GetSamplerFilter() == RHISamplerFilter::Nearest);

    const SpriteAtlasRebuildResult rebuilt = loaded->Rebuild(rhi);
    CHECK(rebuilt.Ok);
    CHECK(loaded->GetGeneratedTextures().size() == 1);

    std::error_code removeError;
    std::filesystem::remove(tempPath, removeError);
}

TEST_CASE("sprite-atlas: rebuild mapping digest is stable [full]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedObjectManager objectManagerScope;
    ScopedHeadlessGlContext gl;
    REQUIRE(gl.IsReady());
    OpenGLRHI rhi;

    std::shared_ptr<Sprite> small = MakeAtlasMemberSprite(rhi, "DigestSmall", 4, Vector4(0, 0, 4, 4));
    std::shared_ptr<Sprite> large = MakeAtlasMemberSprite(rhi, "DigestLarge", 8, Vector4(0, 0, 8, 8));
    REQUIRE(small != nullptr);
    REQUIRE(large != nullptr);

    std::shared_ptr<SpriteAtlas> atlas = NewObject<SpriteAtlas>("DigestAtlas");
    atlas->SetMaxSize(64);
    atlas->SetPadding(1);
    REQUIRE(atlas->AddMember(small));
    REQUIRE(atlas->AddMember(large));
    const SpriteAtlasRebuildResult rebuilt = atlas->Rebuild(rhi);
    REQUIRE(rebuilt.Ok);
    REQUIRE(atlas->GetGeneratedTextures().size() == 1);
    REQUIRE(atlas->GetSlots().size() == 2);

    std::ostringstream digest;
    digest << std::fixed << std::setprecision(4);
    digest << atlas->GetGeneratedTextures()[0]->GetWidth() << "x"
           << atlas->GetGeneratedTextures()[0]->GetHeight() << ";";
    for (const SpriteAtlasSlot& slot : atlas->GetSlots())
    {
        digest << "slot(" << slot.X << "," << slot.Y << "," << slot.Width << "," << slot.Height
               << "," << slot.Page << ");";
    }
    for (const std::shared_ptr<Sprite>& member : atlas->GetMembers())
    {
        const Vector4 uv = member->GetUVRectNormalized();
        const Vector4 baked = member->GetBakedRectPx();
        digest << member->GetName() << " uv(" << uv.x << "," << uv.y << "," << uv.z << "," << uv.w
               << ") baked(" << baked.x << "," << baked.y << "," << baked.z << "," << baked.w << ");";
    }

    CHECK(digest.str()
        == "16x10;slot(11,1,4,4,0);slot(1,1,8,8,0);"
           "DigestSmall uv(0.6875,0.1000,0.9375,0.5000) baked(11.0000,5.0000,4.0000,4.0000);"
           "DigestLarge uv(0.0625,0.1000,0.5625,0.9000) baked(1.0000,1.0000,8.0000,8.0000);");
}


namespace
{
    /**
     * One line that carries everything a bake decides: page size, slot placement, and the per-member
     * baked rect / UV pair the renderer reads. A change to packing or to the Source-to-Baked handoff
     * moves this string.
     */
    std::string BuildBakeSemanticsDigest(minEngine::RHI& rhi)
    {
        using namespace minEngine;

        std::shared_ptr<Sprite> small = MakeAtlasMemberSprite(rhi, "BakeDigestSmall", 4, Vector4(0, 0, 4, 4));
        std::shared_ptr<Sprite> large = MakeAtlasMemberSprite(rhi, "BakeDigestLarge", 8, Vector4(0, 0, 8, 8));
        if (!small || !large)
        {
            return "member-create-failed";
        }

        std::shared_ptr<SpriteAtlas> atlas = NewObject<SpriteAtlas>("BakeDigestAtlas");
        atlas->SetMaxSize(64);
        atlas->SetPadding(1);
        atlas->AddMember(small);
        atlas->AddMember(large);
        const SpriteAtlasRebuildResult rebuilt = atlas->Rebuild(rhi);
        if (!rebuilt.Ok)
        {
            return "rebuild-failed: " + rebuilt.Message;
        }

        std::ostringstream digest;
        digest << std::fixed << std::setprecision(4);
        const std::shared_ptr<Texture2D> page = atlas->GetGeneratedTextures()[0];
        digest << "page(" << page->GetWidth() << "x" << page->GetHeight() << ");";
        for (const SpriteAtlasSlot& slot : atlas->GetSlots())
        {
            digest << "slot(" << slot.X << "," << slot.Y << "," << slot.Width << "," << slot.Height
                   << "," << slot.Page << ");";
        }
        for (const std::shared_ptr<Sprite>& member : atlas->GetMembers())
        {
            const Vector4 source = member->ResolveSourceRectPx();
            const Vector4 baked = member->GetBakedRectPx();
            const Vector4 uv = member->GetUVRectNormalized();
            digest << member->GetName()
                   << " src(" << source.x << "," << source.y << "," << source.z << "," << source.w << ")"
                   << " baked(" << baked.x << "," << baked.y << "," << baked.z << "," << baked.w << ")"
                   << " uv(" << uv.x << "," << uv.y << "," << uv.z << "," << uv.w << ")"
                   << " page(" << (member->GetRuntimeTexture() == page.get() ? 1 : 0) << ");";
        }
        return digest.str();
    }
}

TEST_CASE("sprite-atlas: a baked sprite asset stores no baked texture [full]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedObjectManager objectManagerScope;
    ScopedHeadlessGlContext gl;
    REQUIRE(gl.IsReady());
    OpenGLRHI rhi;

    std::shared_ptr<Sprite> sprite = MakeAtlasMemberSprite(rhi, "BakedRoundTripSprite", 8, Vector4(0, 0, 4, 4));
    REQUIRE(sprite != nullptr);

    // Texture2DLoader::CreateFromPixels registers the texture under the guid NewObject made and then
    // stamps the caller's guid on it; re-register so the authored reference in the file resolves.
    const std::shared_ptr<Texture2D>& sourceTexture = sprite->GetSourceTextureShared();
    REQUIRE(sourceTexture != nullptr);
    REQUIRE(ObjectManager::Get().RemapObjectGuid(sourceTexture, sourceTexture->GetGuid()));

    std::shared_ptr<SpriteAtlas> atlas = NewObject<SpriteAtlas>("BakedRoundTripAtlas");
    atlas->SetMaxSize(64);
    REQUIRE(atlas->AddMember(sprite));
    REQUIRE(atlas->Rebuild(rhi).Ok);
    REQUIRE(sprite->GetBakedTexture() != nullptr);

    const std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / "minEngine_sprite_baked_roundtrip.mesprite";
    std::string writeError;
    REQUIRE(AssetSerializeHelper::WriteObjectToFile(tempPath.string(), *sprite, &writeError));

    const std::string saved = ReadTextFile(tempPath);
    REQUIRE_FALSE(saved.empty());
    // A bake names a page texture that exists only in memory; persisting it writes a guid no load
    // can resolve, and the rect beside it means nothing without that texture.
    CHECK(saved.find("m_BakedTexture") == std::string::npos);
    CHECK(saved.find("m_BakedRectPx") == std::string::npos);
    CHECK(saved.find("m_SourceTexture") != std::string::npos);
    CHECK(saved.find("m_SourceRectPx") != std::string::npos);

    std::shared_ptr<Sprite> loaded = NewObject<Sprite>("BakedRoundTripSpriteReloaded");
    std::string readError;
    INFO(readError);
    REQUIRE(AssetSerializeHelper::ReadObjectFromFile(tempPath.string(), *loaded, &readError));
    CHECK(readError.empty());
    CHECK(loaded->GetBakedTexture() == nullptr);
    CHECK(loaded->GetBakedRectPx() == Vector4(0.0f, 0.0f, 0.0f, 0.0f));
    // The authored half survives: same source texture, same authored rect.
    CHECK(loaded->GetSourceRectPx() == Vector4(0.0f, 0.0f, 4.0f, 4.0f));
    REQUIRE(loaded->GetSourceTexture() == sprite->GetSourceTexture());

    // No bake after load, so the quad falls back to the authored rect: 4x4 at the top left of 8x8.
    const Vector4 uv = loaded->GetUVRectNormalized();
    CHECK(uv.x == doctest::Approx(0.0f));
    CHECK(uv.y == doctest::Approx(0.5f));
    CHECK(uv.z == doctest::Approx(0.5f));
    CHECK(uv.w == doctest::Approx(1.0f));

    std::error_code removeError;
    std::filesystem::remove(tempPath, removeError);
}

TEST_CASE("sprite-atlas: bake semantics digest is unchanged [full]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedObjectManager objectManagerScope;
    ScopedHeadlessGlContext gl;
    REQUIRE(gl.IsReady());
    OpenGLRHI rhi;

    // Recorded while the baked pair was still serialised: it pins page size, slot placement and the
    // per-member baked rect / UV pair to the values a bake produced then, so dropping the fields
    // cannot quietly move what the renderer reads.
    CHECK(BuildBakeSemanticsDigest(rhi)
        == "page(16x10);slot(11,1,4,4,0);slot(1,1,8,8,0);"
           "BakeDigestSmall src(0.0000,0.0000,4.0000,4.0000) baked(11.0000,5.0000,4.0000,4.0000) "
           "uv(0.6875,0.1000,0.9375,0.5000) page(1);"
           "BakeDigestLarge src(0.0000,0.0000,8.0000,8.0000) baked(1.0000,1.0000,8.0000,8.0000) "
           "uv(0.0625,0.1000,0.5625,0.9000) page(1);");
}

