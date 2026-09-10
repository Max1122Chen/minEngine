#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Color.h"
#include "Runtime/Function/Render/Material.h"

#include <memory>

namespace minEngine
{
    class RHI;
    class Texture2D;

    /** Unlit translucent materials for ScreenUI widgets (reuses sprite graph). */
    class ScreenUIMaterialFactory
    {
    public:
        static std::shared_ptr<Material> CreateInstance(RHI& rhi);
        static void ApplyColorAndTexture(
            Material& material,
            const LinearColor& color,
            const std::shared_ptr<Texture2D>& texture,
            RHI& rhi);

        static std::shared_ptr<Texture2D> GetOrCreateWhiteTexture(RHI& rhi);
    };
}
