#include "MaterialInstance.h"

#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/MaterialRenderProxy.h"
#include "Runtime/Function/Render/Texture.h"

namespace minEngine
{
    MaterialInstance::~MaterialInstance() = default;

    void MaterialInstance::ApplyClassIdentity()
    {
        SetClass(MaterialInstance::StaticClass());
    }

    bool MaterialInstance::InitializeParent(std::shared_ptr<Material> parent)
    {
        if (!parent)
        {
            return false;
        }

        m_Parent = std::move(parent);
        ApplyClassIdentity();
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

    bool MaterialInstance::TryGetScalarOverride(const std::string& name, float& outValue) const
    {
        for (const MaterialScalarOverrideEntry& entry : m_ScalarOverrides)
        {
            if (entry.ParameterName == name)
            {
                outValue = entry.Value;
                return true;
            }
        }
        return false;
    }

    bool MaterialInstance::TryGetTextureOverride(
        const std::string& name, std::shared_ptr<Texture2D>& outTexture) const
    {
        for (const MaterialTextureOverrideEntry& entry : m_TextureOverrides)
        {
            if (entry.ParameterName == name)
            {
                outTexture = entry.Value;
                return true;
            }
        }
        return false;
    }

    void MaterialInstance::SetScalarParameter(const std::string& name, float value)
    {
        for (MaterialScalarOverrideEntry& entry : m_ScalarOverrides)
        {
            if (entry.ParameterName == name)
            {
                entry.Value = value;
                EnsureRenderProxy();
                m_RenderProxy->InvalidateParameterBindings();
                return;
            }
        }

        MaterialScalarOverrideEntry entry;
        entry.ParameterName = name;
        entry.Value = value;
        m_ScalarOverrides.push_back(std::move(entry));
        EnsureRenderProxy();
        m_RenderProxy->InvalidateParameterBindings();
    }

    void MaterialInstance::SetTextureParameter(const std::string& name, std::shared_ptr<Texture2D> texture)
    {
        for (MaterialTextureOverrideEntry& entry : m_TextureOverrides)
        {
            if (entry.ParameterName == name)
            {
                entry.Value = std::move(texture);
                EnsureRenderProxy();
                m_RenderProxy->InvalidateParameterBindings();
                return;
            }
        }

        MaterialTextureOverrideEntry entry;
        entry.ParameterName = name;
        entry.Value = std::move(texture);
        m_TextureOverrides.push_back(std::move(entry));
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

    std::shared_ptr<Material> MaterialInstance::ResolveParentMaterial(
        const std::shared_ptr<MaterialInterface>& /*self*/) const
    {
        return m_Parent;
    }

    bool MaterialInstance::FinalizeAfterLoad(std::string* outError)
    {
        ApplyClassIdentity();
        if (!m_Parent)
        {
            if (outError != nullptr)
            {
                *outError = "MaterialInstance has no Parent Material.";
            }
            return false;
        }

        EnsureRenderProxy();
        if (m_RenderProxy)
        {
            m_RenderProxy->InvalidateParameterBindings();
        }
        return true;
    }
}
