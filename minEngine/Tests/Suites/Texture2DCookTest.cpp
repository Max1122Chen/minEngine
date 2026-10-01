#include "Runtime/Resource/Loaders/ImageLoader.h"
#include "Runtime/Resource/Loaders/Texture2DLoader.h"
#include "Runtime/Resource/Importers/NativeCopyImporter.h"
#include "Runtime/Resource/Importers/Texture2DImporter.h"

#include "doctest.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

using namespace minEngine;

namespace
{
    ImagePixels MakeGrayU8(int width, int height, unsigned char value)
    {
        ImagePixels pixels;
        pixels.Storage = ImageStorage::UInt8;
        pixels.Width = width;
        pixels.Height = height;
        pixels.Channels = 1;
        const size_t count = static_cast<size_t>(width) * static_cast<size_t>(height);
        pixels.U8 = static_cast<unsigned char*>(std::malloc(count));
        std::memset(pixels.U8, value, count);
        return pixels;
    }
}

TEST_CASE("texture2d-cook: Color expands 1ch to RGB8 [smoke]")
{
    ImagePixels pixels = MakeGrayU8(2, 2, 128);
    Texture2DCookSettings settings;
    settings.ChannelSemantics = "Color";

    std::string error;
    REQUIRE(Texture2DLoader::ApplyChannelSemantics(pixels, settings, &error));
    CHECK(pixels.Channels == 3);
    CHECK(pixels.U8[0] == 128);
    CHECK(pixels.U8[1] == 128);
    CHECK(pixels.U8[2] == 128);
    ImageLoader::Free(pixels);
}

TEST_CASE("texture2d-cook: SingleChannel keeps R8 [smoke]")
{
    ImagePixels pixels = MakeGrayU8(2, 2, 200);
    Texture2DCookSettings settings;
    settings.ChannelSemantics = "SingleChannel";

    std::string error;
    REQUIRE(Texture2DLoader::ApplyChannelSemantics(pixels, settings, &error));
    CHECK(pixels.Channels == 1);
    CHECK(pixels.U8[0] == 200);
    ImageLoader::Free(pixels);
}

TEST_CASE("texture2d-cook: .metex round-trip preserves pixels [smoke]")
{
    ImagePixels pixels = MakeGrayU8(2, 1, 42);
    Texture2DCookSettings settings;
    settings.ChannelSemantics = "Color";
    REQUIRE(Texture2DLoader::ApplyChannelSemantics(pixels, settings, nullptr));

    const std::filesystem::path tempDir =
        std::filesystem::temp_directory_path() / "minEngine_Texture2DCook";
    std::filesystem::create_directories(tempDir);
    const std::filesystem::path metexPath = tempDir / "roundtrip.metex";

    std::string writeError;
    const bool wrote = Texture2DLoader::WriteCookedFile(metexPath.string(), settings, pixels, &writeError);
    INFO(writeError);
    REQUIRE(wrote);

    Texture2DCookSettings loadedSettings;
    ImagePixels loaded;
    std::string readError;
    REQUIRE(Texture2DLoader::ReadCookedFile(metexPath.string(), loadedSettings, loaded, &readError));
    CHECK(loaded.Width == 2);
    CHECK(loaded.Height == 1);
    CHECK(loaded.Channels == 3);
    CHECK(loaded.U8[0] == 42);
    CHECK(loaded.U8[1] == 42);
    CHECK(loaded.U8[2] == 42);
    CHECK(loadedSettings.ChannelSemantics == "Color");

    ImageLoader::Free(pixels);
    ImageLoader::Free(loaded);
    std::error_code removeError;
    std::filesystem::remove_all(tempDir, removeError);
}

TEST_CASE("texture2d-cook: NativeCopy rejects image; Texture2D accepts [smoke]")
{
    CHECK_FALSE(NativeCopyImporter::AcceptsSourceExtension(".png"));
    CHECK_FALSE(NativeCopyImporter::AcceptsSourceExtension(".JPG"));
    CHECK(Texture2DImporter::AcceptsSourceExtension(".png"));
    CHECK(Texture2DImporter::AcceptsSourceExtension(".jpeg"));
}
