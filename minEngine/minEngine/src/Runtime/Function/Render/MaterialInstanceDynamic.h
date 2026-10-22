#pragma once

#include "Runtime/Core/EngineAPI.h"

#include "Runtime/Function/Render/MaterialInstance.h"

#include <memory>
#include <string>

namespace minEngine
{
    /**
     * Runtime-created material instance (UE UMaterialInstanceDynamic analogue).
     * Reuses MaterialInstance storage and InstanceMaterialRenderProxy; usually not serialized.
     * Prefer this for per-frame Set*; use Material for asset defaults; MIC (.memi) is.
     */
    class MINENGINE_API MaterialInstanceDynamic : public MaterialInstance
    {
    public:
        ~MaterialInstanceDynamic() override = default;

        /**
         * Parent may be Material or MaterialInstance.
         * MVP (D1): always parents to the resolved base Material shared_ptr.
         */
        static std::shared_ptr<MaterialInstanceDynamic> Create(
            std::shared_ptr<MaterialInterface> parent,
            const std::string& debugName = {});

    private:
        MaterialInstanceDynamic() = default;
    };
}
