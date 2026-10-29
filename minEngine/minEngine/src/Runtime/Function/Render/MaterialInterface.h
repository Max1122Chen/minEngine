#pragma once

#include "Core.h"
#include "Runtime/Resource/Asset.h"

#include <memory>

namespace minEngine
{
    class Material;
    class MaterialRenderProxy;

    /**
     * Reference-counted render-side material object; see MaterialRenderProxy.
     *
     * A render-side owner holds this instead of the material asset, so it keeps drawing from the
     * published state after the asset that produced it is destroyed, and releasing it touches no
     * game-role registry.
     */
    using MaterialRenderProxyRef = std::shared_ptr<MaterialRenderProxy>;

    /**
     * Polymorphic material handle (UE UMaterialInterface analogue).
     * Draw code must not bind through this type; call GetRenderProxy().
     *
     * Inherits Asset so Material can stay a single-inheritance Asset. Reflection
     * rejects a second super class, so Material : Asset + MaterialInterface is not viable.
     */
    ME_CLASS()
    class MINENGINE_API MaterialInterface : public Asset
    {
        ME_GENERATED_BODY()

    public:
        ~MaterialInterface() override = default;

        virtual Material* GetBaseMaterial() const = 0;

        /** The proxy to draw with; valid while the material that produced it is alive. */
        virtual MaterialRenderProxy* GetRenderProxy() const = 0;

        /**
         * The same proxy, held.
         *
         * This is the reference a render-side owner takes: it keeps the proxy - and the snapshot
         * it published - alive after the material asset is gone, and releasing it is a plain
         * reference count drop rather than an object-system operation.
         */
        virtual MaterialRenderProxyRef GetRenderProxyRef() const = 0;

        /**
         * Resolve the Material used as Instance/MID parent (roadmap D1).
         * Material returns self; MaterialInstance returns its Parent shared_ptr.
         */
        virtual std::shared_ptr<Material> ResolveParentMaterial(
            const std::shared_ptr<MaterialInterface>& self) const = 0;

        virtual bool IsTranslucent() const;
        virtual bool IsMasked() const;
        virtual bool IsCompiledForDraw() const;
    };
}

#include "Generated/Reflection/MaterialInterface.gen.h"
