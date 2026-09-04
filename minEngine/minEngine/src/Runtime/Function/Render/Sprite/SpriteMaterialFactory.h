#pragma once

#include "Core.h"
#include "Runtime/Function/Render/Material.h"

#include <memory>

namespace minEngine
{
    class RHI;

    /** Builds per-sprite Unlit textured Material instances (opaque / translucent). */
    class SpriteMaterialFactory
    {
    public:
        static std::shared_ptr<Material> CreateInstance(RHI& rhi, bool translucent);
        static void ApplyColorAndTexture(
            Material& material,
            const Vector4& color,
            const std::shared_ptr<Texture2D>& texture);
    };
}
