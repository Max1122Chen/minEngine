#include "MaterialInstance.h"

#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/MaterialRenderProxy.h"
#include "Runtime/Function/Render/Texture.h"

namespace minEngine
{
    MaterialInstance::~MaterialInstance() = default;

    bool MaterialInstance::InitializeParent(std::shared_ptr<Material> parent)
    {
        if (!parent)
        {
            return false;
        }

        m_Parent = std::move(parent);
        EnsureRenderProxy();
        return true;
    }

    std::shared_ptr<MaterialInstance> MaterialInstance::Create(std::shared_ptr<Material> parent)
    {
        std::shared_ptr<MaterialInstance> instance(new MaterialInstance());
        if (!instance->InitializeParent(std::move(parent)))
        {
            return nullptr;
        }
        return instance;
    }

    void MaterialInstance::EnsureRenderProxy()
    {
        if (!m_RenderProxy)
        {
            m_RenderProxy = std::make_unique<InstanceMaterialRenderProxy>(*this);
        }
    }

    void MaterialInstance::SetScalarParameter(const std::string& name, float value)
    {
        m_ScalarOverrides[name] = value;
        EnsureRenderProxy();
        m_RenderProxy->InvalidateParameterBindings();
    }

    void MaterialInstance::SetTextureParameter(const std::string& name, std::shared_ptr<Texture2D> texture)
    {
        m_TextureOverrides[name] = std::move(texture);
        EnsureRenderProxy();
        m_RenderProxy->InvalidateParameterBindings();
    }

    Material* MaterialInstance::GetBaseMaterial() const
    {
        return m_Parent ? m_Parent->GetBaseMaterial() : nullptr;
    }

    MaterialRenderProxy* MaterialInstance::GetRenderProxy() const
    {
        return m_RenderProxy.get();
    }
}
