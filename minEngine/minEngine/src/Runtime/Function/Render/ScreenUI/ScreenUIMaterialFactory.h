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

    /** Unlit translucent materials for ScreenUI widgets (reuses sprite graph). */
    class MINENGINE_API ScreenUIMaterialFactory
    {
    public:
        static std::shared_ptr<Material> CreateInstance(RHI& rhi);
        static void ApplyColorAndTexture(
            Material& material,
            const LinearColor& color,
            const std::shared_ptr<Texture2D>& texture,
            RHI& rhi);

        static void ApplyColorTextureAndUVRect(
            Material& material,
            const LinearColor& color,
            const std::shared_ptr<Texture2D>& texture,
            const Vector4& uvRect,
            RHI& rhi);

        static std::shared_ptr<Texture2D> GetOrCreateWhiteTexture(RHI& rhi);

        /**
         * Drops the process-lifetime white texture. It owns a GPU texture, so it must be released
         * while the graphics context is alive (RenderSystem::Shutdown does this).
         */
        static void ClearCachedResources();
        static bool HasCachedWhiteTexture();
    };
}
