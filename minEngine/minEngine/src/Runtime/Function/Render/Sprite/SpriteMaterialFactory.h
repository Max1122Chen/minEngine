#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Color.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Render/Material.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace minEngine
{
    class RHI;
    class Texture2D;

    /** Counters for the compiled-template cache; the measurable oracle for it. */
    struct SpriteMaterialCacheStats
    {
        /** Full graph + shader compiles of a template Material (opaque/translucent). */
        uint64_t TemplateCompiles = 0;
        /** CreateInstance calls served by an already-compiled template without compiling. */
        uint64_t InstanceDerivations = 0;
    };

    /**
     * Builds per-sprite Unlit textured Material instances (opaque / translucent).
     *
     * Compilation is per-material-variant, not per-instance: one template Material per blend mode is
     * compiled once per RHI instance, and every CreateInstance() derives a runtime sibling that
     * shares its GPU program while keeping private parameter tables (tint/opacity/UV per sprite).
     */
    class MINENGINE_API SpriteMaterialFactory
    {
    public:
        static std::shared_ptr<Material> CreateInstance(RHI& rhi, bool translucent);

        /** Compiled template for this blend mode, compiling it on first use (shared program owner). */
        static std::shared_ptr<Material> GetOrCreateTemplate(RHI& rhi, bool translucent);

        static SpriteMaterialCacheStats GetCacheStats();
        static void ResetCacheStats();
        /** Compiled templates currently held (0..2); used by the shutdown-ownership oracle. */
        static size_t GetTemplateCacheEntryCount();
        /** Drops templates (asset reload / shader hot-reload / test isolation); next call recompiles. */
        static void ClearTemplateCache();

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