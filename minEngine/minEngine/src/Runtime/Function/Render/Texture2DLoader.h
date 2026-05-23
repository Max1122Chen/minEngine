#pragma once

#include "Core.h"
#include "Runtime/Resource/AssetMeta.h"
#include "Runtime/Resource/ImageLoader.h"

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
    };
}
