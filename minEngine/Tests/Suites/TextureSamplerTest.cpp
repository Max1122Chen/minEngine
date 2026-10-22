#include "Runtime/Function/Render/Texture.h"
#include "Runtime/Resource/Loaders/ImageLoader.h"
#include "Runtime/Resource/Loaders/Texture2DLoader.h"
#include "Runtime/Core/Reflection/MEClass.h"
#include "Runtime/Core/Reflection/MEProperties.h"
#include "Runtime/Core/Reflection/Reflection.h"

#include "EngineTestFixture.h"

#include "doctest.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

using namespace minEngine;

namespace
{
    RHISamplerDesc MakeAxisDesc(RHISamplerFilter filter, RHISamplerAddress address)
    {
        RHISamplerDesc desc;
        desc.MinFilter = filter;
        desc.MagFilter = filter;
        desc.AddressU = address;
        desc.AddressV = address;
        desc.AddressW = address;
        return desc;
    }

    ImagePixels MakeOnePixelGray(unsigned char value)
    {
        ImagePixels pixels;
        pixels.Storage = ImageStorage::UInt8;
        pixels.Width = 1;
        pixels.Height = 1;
        pixels.Channels = 1;
        pixels.U8 = static_cast<unsigned char*>(std::malloc(1));
        pixels.U8[0] = value;
        return pixels;
    }

    /** Hand-written .metex so a header shape older than this build can be read back. */
    bool WriteRawMetex(
        const std::filesystem::path& path,
        const std::string& headerText,
        unsigned char pixelValue)
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            return false;
        }
        out.write("METEX1\n", 7);
        out.write(headerText.data(), static_cast<std::streamsize>(headerText.size()));
        out.put('\n');
        out.put(static_cast<char>(pixelValue));
        return static_cast<bool>(out);
    }

    std::filesystem::path MakeTempDir(const char* name)
    {
        const std::filesystem::path dir = std::filesystem::temp_directory_path() / name;
        std::filesystem::create_directories(dir);
        return dir;
    }

    const Reflection::MEProperty* FindProperty(const char* name)
    {
        const Reflection::MEClass* textureClass =
            Reflection::ReflectionSystem::Get().FindClass("minEngine::Texture2D");
        if (textureClass == nullptr)
        {
            return nullptr;
        }
        for (const Reflection::MEProperty* property : textureClass->GetProperties())
        {
            if (property != nullptr && property->GetName() == name)
            {
                return property;
            }
        }
        return nullptr;
    }
}

TEST_CASE("texture-sampler: a field change moves the sampler cache key [smoke]")
{
    const std::shared_ptr<Texture2D> texture = std::make_shared<Texture2D>();
    const RHISamplerDesc bound = texture->GetPreferredSamplerDesc();
    const size_t boundKey = RHISamplerDescHash{}(bound);

    texture->SetSamplerFilter(RHISamplerFilter::Nearest);

    // The sampler cache is keyed by the desc, so an unchanged desc would hand back the
    // sampler a previous bind already created.
    CHECK(texture->GetPreferredSamplerDesc() != bound);
    CHECK(RHISamplerDescHash{}(texture->GetPreferredSamplerDesc()) != boundKey);
}

TEST_CASE("texture-sampler: either setter leaves the other axis alone [smoke]")
{
    const std::shared_ptr<Texture2D> texture = std::make_shared<Texture2D>();
    CHECK(texture->GetPreferredSamplerDesc() == MakeAxisDesc(RHISamplerFilter::Linear, RHISamplerAddress::Repeat));

    texture->SetSamplerFilter(RHISamplerFilter::Nearest);
    CHECK(texture->GetSamplerAddress() == RHISamplerAddress::Repeat);
    CHECK(texture->GetPreferredSamplerDesc() == MakeAxisDesc(RHISamplerFilter::Nearest, RHISamplerAddress::Repeat));

    texture->SetSamplerAddress(RHISamplerAddress::ClampToBorder);
    CHECK(texture->GetSamplerFilter() == RHISamplerFilter::Nearest);
    CHECK(
        texture->GetPreferredSamplerDesc()
        == MakeAxisDesc(RHISamplerFilter::Nearest, RHISamplerAddress::ClampToBorder));

    const std::shared_ptr<Texture2D> reversed = std::make_shared<Texture2D>();
    reversed->SetSamplerAddress(RHISamplerAddress::MirroredRepeat);
    reversed->SetSamplerFilter(RHISamplerFilter::Nearest);
    CHECK(reversed->GetSamplerAddress() == RHISamplerAddress::MirroredRepeat);
    CHECK(
        reversed->GetPreferredSamplerDesc()
        == MakeAxisDesc(RHISamplerFilter::Nearest, RHISamplerAddress::MirroredRepeat));
}

TEST_CASE("texture-sampler: the two author fields derive the preferred desc [smoke]")
{
    struct Combo
    {
        RHISamplerFilter Filter;
        RHISamplerAddress Address;
    };
    const Combo combos[] = {
        {RHISamplerFilter::Linear, RHISamplerAddress::Repeat},
        {RHISamplerFilter::Nearest, RHISamplerAddress::Repeat},
        {RHISamplerFilter::Linear, RHISamplerAddress::ClampToEdge},
        {RHISamplerFilter::Nearest, RHISamplerAddress::ClampToBorder},
    };

    for (const Combo& combo : combos)
    {
        const std::shared_ptr<Texture2D> texture = std::make_shared<Texture2D>();
        texture->SetSamplerFilter(combo.Filter);
        texture->SetSamplerAddress(combo.Address);
        CHECK(texture->GetPreferredSamplerDesc() == MakeAxisDesc(combo.Filter, combo.Address));
    }
}

TEST_CASE("texture-sampler: reflected sampler fields are editable and act through the setters [smoke]")
{
    EngineReflectionFixture reflectionFixture;

    const Reflection::MEProperty* filterProperty = FindProperty("m_SamplerFilter");
    const Reflection::MEProperty* addressProperty = FindProperty("m_SamplerAddress");
    REQUIRE(filterProperty != nullptr);
    REQUIRE(addressProperty != nullptr);

    CHECK(filterProperty->HasSpecifier(Reflection::PropertySpecifier::EditAnywhere));
    CHECK(addressProperty->HasSpecifier(Reflection::PropertySpecifier::EditAnywhere));
    REQUIRE(filterProperty->HasPropertySetter());
    REQUIRE(addressProperty->HasPropertySetter());

    const auto* filterPrimitive =
        dynamic_cast<const Reflection::MEPrimitiveProperty*>(filterProperty);
    const auto* addressPrimitive =
        dynamic_cast<const Reflection::MEPrimitiveProperty*>(addressProperty);
    REQUIRE(filterPrimitive != nullptr);
    REQUIRE(addressPrimitive != nullptr);
    CHECK(filterPrimitive->IsEnum());
    CHECK(addressPrimitive->IsEnum());
    REQUIRE(filterPrimitive->GetEnum() != nullptr);
    REQUIRE(addressPrimitive->GetEnum() != nullptr);
    CHECK(filterPrimitive->GetEnum()->GetName() == "minEngine::RHISamplerFilter");
    CHECK(addressPrimitive->GetEnum()->GetName() == "minEngine::RHISamplerAddress");

    const std::shared_ptr<Texture2D> texture = std::make_shared<Texture2D>();
    const RHISamplerFilter nearest = RHISamplerFilter::Nearest;
    const RHISamplerAddress border = RHISamplerAddress::ClampToBorder;
    filterProperty->GetPropertySetter()(texture.get(), &nearest);
    CHECK(
        texture->GetPreferredSamplerDesc()
        == MakeAxisDesc(RHISamplerFilter::Nearest, RHISamplerAddress::Repeat));
    addressProperty->GetPropertySetter()(texture.get(), &border);
    CHECK(
        texture->GetPreferredSamplerDesc()
        == MakeAxisDesc(RHISamplerFilter::Nearest, RHISamplerAddress::ClampToBorder));
}

TEST_CASE("texture-sampler: .metex keeps written sampler modes [smoke]")
{
    ImagePixels pixels = MakeOnePixelGray(9);
    Texture2DCookSettings settings;
    settings.SamplerFilter = RHISamplerFilter::Nearest;
    settings.SamplerAddress = RHISamplerAddress::ClampToBorder;

    const std::filesystem::path tempDir = MakeTempDir("minEngine_TextureSampler");
    const std::filesystem::path metexPath = tempDir / "modes.metex";

    std::string error;
    const bool wrote = Texture2DLoader::WriteCookedFile(metexPath.string(), settings, pixels, &error);
    INFO(error);
    REQUIRE(wrote);

    Texture2DCookSettings loaded;
    ImagePixels loadedPixels;
    REQUIRE(Texture2DLoader::ReadCookedFile(metexPath.string(), loaded, loadedPixels, &error));
    CHECK(loaded.SamplerFilter == RHISamplerFilter::Nearest);
    CHECK(loaded.SamplerAddress == RHISamplerAddress::ClampToBorder);
    CHECK(loadedPixels.U8[0] == 9);

    ImageLoader::Free(pixels);
    ImageLoader::Free(loadedPixels);
    std::error_code removeError;
    std::filesystem::remove_all(tempDir, removeError);
}

TEST_CASE("texture-sampler: a version 1 .metex reads as Linear and Repeat [smoke]")
{
    const std::filesystem::path tempDir = MakeTempDir("minEngine_TextureSamplerLegacy");
    const std::filesystem::path metexPath = tempDir / "legacy.metex";

    // A pre-sampler-modes header, including the legacy string keys a reader must not use.
    const std::string header =
        R"({"version":1,"width":1,"height":1,"channels":1,"cookedFormat":"R8",)"
        R"("channelSemantics":"SingleChannel","sRGB":false,)"
        R"("wrapU":"ClampToEdge","wrapV":"ClampToEdge","filter":"Nearest"})";
    REQUIRE(WriteRawMetex(metexPath, header, 3));

    Texture2DCookSettings loaded;
    loaded.SamplerFilter = RHISamplerFilter::Nearest;
    loaded.SamplerAddress = RHISamplerAddress::ClampToBorder;

    ImagePixels loadedPixels;
    std::string error;
    REQUIRE(Texture2DLoader::ReadCookedFile(metexPath.string(), loaded, loadedPixels, &error));
    CHECK(loaded.SamplerFilter == RHISamplerFilter::Linear);
    CHECK(loaded.SamplerAddress == RHISamplerAddress::Repeat);
    CHECK(loadedPixels.U8[0] == 3);

    ImageLoader::Free(loadedPixels);
    std::error_code removeError;
    std::filesystem::remove_all(tempDir, removeError);
}

TEST_CASE("texture-sampler: a header without sampler keys reads as Linear and Repeat [smoke]")
{
    const std::filesystem::path tempDir = MakeTempDir("minEngine_TextureSamplerMissing");
    const std::filesystem::path metexPath = tempDir / "missing.metex";

    const std::string header =
        R"({"version":2,"width":1,"height":1,"channels":1,"cookedFormat":"R8",)"
        R"("channelSemantics":"SingleChannel","sRGB":false})";
    REQUIRE(WriteRawMetex(metexPath, header, 5));

    Texture2DCookSettings loaded;
    loaded.SamplerFilter = RHISamplerFilter::Nearest;
    loaded.SamplerAddress = RHISamplerAddress::ClampToEdge;

    ImagePixels loadedPixels;
    std::string error;
    REQUIRE(Texture2DLoader::ReadCookedFile(metexPath.string(), loaded, loadedPixels, &error));
    CHECK(loaded.SamplerFilter == RHISamplerFilter::Linear);
    CHECK(loaded.SamplerAddress == RHISamplerAddress::Repeat);

    ImageLoader::Free(loadedPixels);
    std::error_code removeError;
    std::filesystem::remove_all(tempDir, removeError);
}