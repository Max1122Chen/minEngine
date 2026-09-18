#include "MaterialRenderProxy.h"

#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/EngineShaderBindings.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"

#include <algorithm>
#include <vector>

namespace minEngine
{
    void MaterialRenderProxy::NotifyCompiled(RHICommandList& cmdList)
    {
        InvalidateParameterBindings();
        BindForDraw(cmdList);
    }

    DefaultMaterialRenderProxy::DefaultMaterialRenderProxy(Material& material)
        : m_Material(&material)
    {
    }

    Material* DefaultMaterialRenderProxy::GetBaseMaterial() const
    {
        return m_Material;
    }

    bool DefaultMaterialRenderProxy::IsCompiledForDraw() const
    {
        return m_Material && m_Material->IsCompiledForDraw();
    }

    bool DefaultMaterialRenderProxy::IsCompiledForSkinnedDraw() const
    {
        return m_Material && m_Material->IsCompiledForSkinnedDraw();
    }

    bool DefaultMaterialRenderProxy::IsTranslucent() const
    {
        return m_Material && m_Material->IsTranslucent();
    }

    bool DefaultMaterialRenderProxy::IsMasked() const
    {
        return m_Material && m_Material->IsMasked();
    }

    RHIShader* DefaultMaterialRenderProxy::GetGPUShader() const
    {
        return m_Material ? m_Material->GetGPUShader() : nullptr;
    }

    RHIShader* DefaultMaterialRenderProxy::GetGPUShaderSkinned() const
    {
        return m_Material ? m_Material->GetGPUShaderSkinned() : nullptr;
    }

    RHIShaderBindingSetLayout* DefaultMaterialRenderProxy::GetMaterialShaderBindingSetLayout() const
    {
        return m_Material ? m_Material->GetMaterialShaderBindingSetLayout() : nullptr;
    }

    RHIShaderBindingSet* DefaultMaterialRenderProxy::GetMaterialShaderBindingSet() const
    {
        return m_MaterialShaderBindingSet.get();
    }

    bool DefaultMaterialRenderProxy::GetScalarParameterValue(const std::string& name, float& outValue) const
    {
        if (!m_Material)
        {
            return false;
        }
        for (const MaterialScalarParameter& parameter : m_Material->m_ScalarParameters)
        {
            if (parameter.ParameterName == name)
            {
                outValue = parameter.Value;
                return true;
            }
        }
        return false;
    }

    bool DefaultMaterialRenderProxy::GetTextureParameterValue(
        const std::string& name, std::shared_ptr<Texture2D>& outTexture) const
    {
        if (!m_Material)
        {
            return false;
        }
        for (const MaterialTextureParameter& parameter : m_Material->m_TextureParameters)
        {
            if (parameter.ParameterName == name)
            {
                outTexture = parameter.Value;
                return true;
            }
        }
        return false;
    }

    bool DefaultMaterialRenderProxy::GetScalarParameterValueBySlot(int slotIndex, float& outValue) const
    {
        if (!m_Material)
        {
            return false;
        }
        for (const MaterialScalarParameter& parameter : m_Material->m_ScalarParameters)
        {
            if (parameter.SlotIndex == slotIndex)
            {
                outValue = parameter.Value;
                return true;
            }
        }
        return false;
    }

    bool DefaultMaterialRenderProxy::GetTextureParameterValueBySlot(
        int slotIndex, std::shared_ptr<Texture2D>& outTexture) const
    {
        if (!m_Material)
        {
            return false;
        }
        for (const MaterialTextureParameter& parameter : m_Material->m_TextureParameters)
        {
            if (parameter.SlotIndex == slotIndex)
            {
                outTexture = parameter.Value;
                return true;
            }
        }
        return false;
    }

    void DefaultMaterialRenderProxy::InvalidateParameterBindings()
    {
        m_bParameterBindingsDirty = true;
    }

    void DefaultMaterialRenderProxy::NotifyCompiled(RHICommandList& cmdList)
    {
        m_MaterialShaderBindingSet.reset();
        m_TextureSRVs.clear();
        m_ScalarParamsUBO.reset();
        m_ScalarParamsUBOSize = 0;
        m_bParameterBindingsDirty = true;
        EnsureScalarUniformBuffer(cmdList);
        RebuildShaderBindingSet(cmdList);
        m_bParameterBindingsDirty = false;
    }

    void DefaultMaterialRenderProxy::EnsureScalarUniformBuffer(RHICommandList& cmdList)
    {
        if (!m_Material || m_Material->m_ScalarParameters.empty())
        {
            m_ScalarParamsUBO.reset();
            m_ScalarParamsUBOSize = 0;
            return;
        }

        int maxSlot = 0;
        for (const MaterialScalarParameter& scalarParameter : m_Material->m_ScalarParameters)
        {
            maxSlot = std::max(maxSlot, scalarParameter.SlotIndex);
        }
        const uint32_t uboSize = static_cast<uint32_t>((maxSlot + 1) * 16u);
        if (m_ScalarParamsUBO && m_ScalarParamsUBOSize == uboSize)
        {
            return;
        }

        m_ScalarParamsUBOSize = uboSize;
        RHIBufferCreateDesc uboDesc;
        uboDesc.Usage = RHIBufferUsage::Uniform;
        uboDesc.ByteSize = m_ScalarParamsUBOSize;
        m_ScalarParamsUBO = cmdList.CreateBuffer(uboDesc, nullptr);
    }

    void DefaultMaterialRenderProxy::RebuildShaderBindingSet(RHICommandList& cmdList)
    {
        m_MaterialShaderBindingSet.reset();
        m_TextureSRVs.clear();

        RHIShaderBindingSetLayout* layout = GetMaterialShaderBindingSetLayout();
        if (!layout)
        {
            return;
        }

        const std::vector<RHIShaderBindingSetLayoutEntry>& entries = layout->GetEntries();
        std::vector<RHIShaderBinding> resources(entries.size());

        for (size_t entryIndex = 0; entryIndex < entries.size(); ++entryIndex)
        {
            const RHIShaderBindingSetLayoutEntry& entry = entries[entryIndex];
            if (entry.Type == RHIShaderBindingType::UniformBuffer)
            {
                resources[entryIndex].Type = RHIShaderBindingType::UniformBuffer;
                resources[entryIndex].Buffer = m_ScalarParamsUBO.get();
                continue;
            }

            std::shared_ptr<Texture2D> texture;
            if (!GetTextureParameterValueBySlot(static_cast<int>(entry.ShaderBinding), texture) || !texture)
            {
                continue;
            }

            RHITexture* modernTexture = texture->GetRHITexture();
            if (!modernTexture)
            {
                continue;
            }

            RHIShaderResourceViewRef srv = m_TextureViewCache.GetOrCreate(cmdList, modernTexture);
            m_TextureSRVs.push_back(srv);
            resources[entryIndex].Type = RHIShaderBindingType::TextureSRV;
            resources[entryIndex].TextureSRV = srv.get();
        }

        m_MaterialShaderBindingSet = cmdList.CreateShaderBindingSet(layout, resources);
    }

    void DefaultMaterialRenderProxy::BindForDraw(RHICommandList& cmdList)
    {
        if (m_bParameterBindingsDirty)
        {
            EnsureScalarUniformBuffer(cmdList);
            RebuildShaderBindingSet(cmdList);
            m_bParameterBindingsDirty = false;
        }

        if (!m_ScalarParamsUBO || m_ScalarParamsUBOSize == 0)
        {
            return;
        }

        const uint32_t floatCount = m_ScalarParamsUBOSize / 16u;
        std::vector<float> scalarData(floatCount * 4u, 0.0f);
        if (m_Material)
        {
            for (const MaterialScalarParameter& scalarParameter : m_Material->m_ScalarParameters)
            {
                if (scalarParameter.SlotIndex >= 0 &&
                    static_cast<uint32_t>(scalarParameter.SlotIndex) < floatCount)
                {
                    float value = 0.0f;
                    if (GetScalarParameterValueBySlot(scalarParameter.SlotIndex, value))
                    {
                        scalarData[static_cast<size_t>(scalarParameter.SlotIndex) * 4u] = value;
                    }
                }
            }
        }
        m_ScalarParamsUBO->UpdateSubresource(scalarData.data(), 0, m_ScalarParamsUBOSize);
    }
}
