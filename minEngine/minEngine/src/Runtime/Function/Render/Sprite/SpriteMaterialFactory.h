#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Color.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Render/Material.h"

#include <memory>

namespace minEngine
{
    class RHI;
    class Texture2D;

    /** Builds per-sprite Unlit textured Material instances (opaque / translucent). */
    class SpriteMaterialFactory
    {
    public:
        static std::shared_ptr<Material> CreateInstance(RHI& rhi, bool translucent);

        static void ApplyColorAndTexture(
            Material& material,
            const LinearColor& color,
            const std::shared_ptr<Texture2D>& texture);

        static void ApplyColorTextureAndUVRect(
            Material& material,
            const LinearColor& color,
            const std::shared_ptr<Texture2D>& texture,
            const Vector4& uvRect);
    };
}
