#pragma once

#include "Core.h"
#include "Runtime/Function/Render/MaterialInterface.h"
#include "Runtime/Function/Render/MaterialRenderProxy.h"
#include "Runtime/Function/Render/Texture.h"

#include <memory>
#include <string>
#include <vector>

namespace minEngine
{
    class Material;

    ME_STRUCT()
    struct MaterialScalarOverrideEntry
    {
        ME_GENERATED_BODY()

        ME_PROPERTY()
        std::string ParameterName;

        ME_PROPERTY()
        float Value = 0.0f;
    };

    ME_STRUCT()
    struct MaterialTextureOverrideEntry
    {
        ME_GENERATED_BODY()

        ME_PROPERTY()
        std::string ParameterName;

        ME_PROPERTY()
        std::shared_ptr<Texture2D> Value;
    };

    /**
     * Runtime / MIC material with a parent Material and local overrides.
     * Draw code must use GetRenderProxy. Disk form is.memi.
     */
    ME_CLASS()
    class MINENGINE_API MaterialInstance : public MaterialInterface
    {
        ME_GENERATED_BODY()
        friend class InstanceMaterialRenderProxy;

    public:
        MaterialInstance() = default;
        ~MaterialInstance() override;

        static std::shared_ptr<MaterialInstance> Create(std::shared_ptr<Material> parent);

        void SetScalarParameter(const std::string& name, float value);
        void SetTextureParameter(const std::string& name, std::shared_ptr<Texture2D> texture);

        Material* GetBaseMaterial() const override;
        MaterialRenderProxy* GetRenderProxy() const override;
        std::shared_ptr<Material> ResolveParentMaterial(
            const std::shared_ptr<MaterialInterface>& self) const override;

        Material* GetParent() const { return m_Parent.get(); }
        const std::shared_ptr<Material>& GetParentShared() const { return m_Parent; }
        const std::vector<MaterialScalarOverrideEntry>& GetScalarOverrides() const { return m_ScalarOverrides; }

        /** After deserialize: ensure proxy exists; returns false if Parent is null. */
        bool FinalizeAfterLoad(std::string* outError = nullptr);

        bool InitializeParent(std::shared_ptr<Material> parent);

    protected:
        void EnsureRenderProxy();
        void ApplyClassIdentity();

        bool TryGetScalarOverride(const std::string& name, float& outValue) const;
        bool TryGetTextureOverride(
            const std::string& name, std::shared_ptr<Texture2D>& outTexture) const;

        ME_PROPERTY()
        std::shared_ptr<Material> m_Parent;

        ME_PROPERTY()
        std::vector<MaterialScalarOverrideEntry> m_ScalarOverrides;

        ME_PROPERTY()
        std::vector<MaterialTextureOverrideEntry> m_TextureOverrides;

        std::unique_ptr<MaterialRenderProxy> m_RenderProxy;
    };
}

#include "Generated/Reflection/MaterialInstance.gen.h"
