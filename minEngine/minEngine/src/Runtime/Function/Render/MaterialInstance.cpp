#include "MaterialInstance.h"

#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/MaterialRenderProxy.h"
#include "Runtime/Function/Render/Texture.h"

namespace minEngine
{
    MaterialInstance::~MaterialInstance()
    {
        // See Material::~Material: a render object that outlives this instance must stop reading
        // the override tables and the parent link that are about to be freed.
        if (m_RenderProxy)
        {
            m_RenderProxy->DetachFromOwner();
        }
    }

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
        if (InstanceMaterialRenderProxy* proxy =
                static_cast<InstanceMaterialRenderProxy*>(m_RenderProxy.get()))
        {
            proxy->SetParentProxy(m_Parent ? m_Parent->GetRenderProxy() : nullptr);
            proxy->MarkGameDataDirty();
        }
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
            m_RenderProxy = MakeMaterialRenderProxy<InstanceMaterialRenderProxy>(*this);
            // The proxy draws from a snapshot of the parent's slots with these overrides applied, so
            // it needs the parent *proxy* rather than the parent material.
            InstanceMaterialRenderProxy* proxy =
                static_cast<InstanceMaterialRenderProxy*>(m_RenderProxy.get());
            proxy->SetParentProxy(m_Parent ? m_Parent->GetRenderProxy() : nullptr);
            proxy->PublishGameData();
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
                m_RenderProxy->MarkGameDataDirty();
                return;
            }
        }

        MaterialScalarOverrideEntry entry;
        entry.ParameterName = name;
        entry.Value = value;
        m_ScalarOverrides.push_back(std::move(entry));
        EnsureRenderProxy();
        m_RenderProxy->MarkGameDataDirty();
    }

    void MaterialInstance::SetTextureParameter(const std::string& name, std::shared_ptr<Texture2D> texture)
    {
        for (MaterialTextureOverrideEntry& entry : m_TextureOverrides)
        {
            if (entry.ParameterName == name)
            {
                entry.Value = std::move(texture);
                EnsureRenderProxy();
                m_RenderProxy->MarkGameDataDirty();
                return;
            }
        }

        MaterialTextureOverrideEntry entry;
        entry.ParameterName = name;
        entry.Value = std::move(texture);
        m_TextureOverrides.push_back(std::move(entry));
        EnsureRenderProxy();
        m_RenderProxy->MarkGameDataDirty();
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
        if (InstanceMaterialRenderProxy* proxy =
                static_cast<InstanceMaterialRenderProxy*>(m_RenderProxy.get()))
        {
            // Deserialization assigns m_Parent directly, so the parent proxy link is bound here too.
            proxy->SetParentProxy(m_Parent ? m_Parent->GetRenderProxy() : nullptr);
            proxy->MarkGameDataDirty();
        }
        return true;
    }
}