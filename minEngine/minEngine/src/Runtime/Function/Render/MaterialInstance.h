#pragma once

#include "Core.h"
#include "Runtime/Function/Render/MaterialInterface.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace minEngine
{
    class Material;
    class Texture2D;

    /**
     * Runtime material with a parent Material and local scalar/texture overrides.
     * Draw code must use GetRenderProxy(), not this override table.
     * Inherits Asset through MaterialInterface; not registered or serialized until MIC (MAT-F06).
     */
    class MaterialInstance : public MaterialInterface
    {
        friend class InstanceMaterialRenderProxy;

    public:
        ~MaterialInstance() override;

        static std::shared_ptr<MaterialInstance> Create(std::shared_ptr<Material> parent);

        void SetScalarParameter(const std::string& name, float value);
        void SetTextureParameter(const std::string& name, std::shared_ptr<Texture2D> texture);

        Material* GetBaseMaterial() const override;
        MaterialRenderProxy* GetRenderProxy() const override;

        Material* GetParent() const { return m_Parent.get(); }
        const std::shared_ptr<Material>& GetParentShared() const { return m_Parent; }

    protected:
        MaterialInstance() = default;

        bool InitializeParent(std::shared_ptr<Material> parent);
        void EnsureRenderProxy();

    private:
        std::shared_ptr<Material> m_Parent;
        std::unordered_map<std::string, float> m_ScalarOverrides;
        std::unordered_map<std::string, std::shared_ptr<Texture2D>> m_TextureOverrides;
        std::unique_ptr<MaterialRenderProxy> m_RenderProxy;
    };
}
