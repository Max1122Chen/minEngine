#pragma once

#include "Core.h"
#include "Runtime/Resource/Asset.h"

#include <memory>

namespace minEngine
{
    class Material;
    class MaterialRenderProxy;

    /**
     * Polymorphic material handle (UE UMaterialInterface analogue).
     * Draw code must not bind through this type; call GetRenderProxy().
     *
     * Inherits Asset so Material can stay a single-inheritance Asset. Reflection
     * rejects a second super class, so Material : Asset + MaterialInterface is not viable.
     */
    ME_CLASS()
    class MaterialInterface : public Asset
    {
        ME_GENERATED_BODY()

    public:
        ~MaterialInterface() override = default;

        virtual Material* GetBaseMaterial() const = 0;
        virtual MaterialRenderProxy* GetRenderProxy() const = 0;

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
