#pragma once

#include "Core.h"
#include "Runtime/Resource/AssetMeta.h"
#include "Runtime/Resource/Loaders/ImageLoader.h"
#include "Runtime/Resource/Loaders/Texture2DCookTypes.h"

#include <memory>
#include <string>

namespace minEngine
{
    class AssetManager;
    class RHI;
    class Texture2D;
    class Asset;
    struct AssetMeta;

    class MINENGINE_API Texture2DLoader
    {
    public:
        static std::shared_ptr<Texture2D> LoadFromAssetMeta(const AssetMeta& meta);

        static std::shared_ptr<Texture2D> CreateFromPixels(
            RHI& rhi,
            const ImagePixels& pixels,
            const std::string& debugName,
            const GUID& guid);

        /** HDR equirectangular map (float pixels) → RGB16F / RGBA16F RHI texture. */
        static std::shared_ptr<Texture2D> CreateFromHdrPixels(
            RHI& rhi,
            const ImagePixels& pixels,
            const std::string& debugName);

        static bool SaveHandler(
            const AssetManager& manager,
            const AssetMeta& meta,
            const Asset& asset,
            std::string& outError);

        static std::shared_ptr<Asset> CreateHandler(
            AssetManager& manager,
            const std::string& assetName,
            const std::string& directoryRel,
            std::string& outError);

        static bool ApplyChannelSemantics(
            ImagePixels& inOutPixels,
            const Texture2DCookSettings& settings,
            std::string* outError);

        static bool WriteCookedFile(
            const std::string& absolutePath,
            const Texture2DCookSettings& settings,
            const ImagePixels& pixels,
            std::string* outError);

        static bool ReadCookedFile(
            const std::string& absolutePath,
            Texture2DCookSettings& outSettings,
            ImagePixels& outPixels,
            std::string* outError);

        static bool ReadCookSettings(
            const std::string& absolutePath,
            Texture2DCookSettings& outSettings,
            std::string* outError);

        /** Authoring pixels from .metex (or legacy image path during migration). */
        static bool TryGetCookedPixels(
            const AssetMeta& meta,
            ImagePixels& outPixels,
            std::string* outError);
    };
}
