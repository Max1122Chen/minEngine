#pragma once

#include "Core.h"
#include "Runtime/Resource/AssetMeta.h"
#include "Runtime/Resource/Loaders/ImageLoader.h"

#include <memory>
#include <string>

namespace minEngine
{
    class RHI;
    class Texture2D;

    class Texture2DLoader
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
    };
}
