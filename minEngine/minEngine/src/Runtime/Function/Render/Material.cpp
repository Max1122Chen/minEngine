#include "Material.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Render/Material/MaterialCompiler/MaterialCompiler.h"
#include "Runtime/Function/Render/Material/MaterialGraphNodeDefs/MaterialGraphNodeDef.h"
#include "Runtime/Function/Render/Material/MaterialCapability.h"
#include "Runtime/Function/Render/Material/MaterialValueType.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/EngineShaderBindings.h"
#include "Runtime/Function/Render/OpenGL/OpenGLRHIResources.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "RHI/RHIShader.h"
#include "RHI/RHITexture.h"
#include "Texture.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace minEngine
{
    MaterialEdGraph& Material::GetGraph()
    {
        if (!m_Graph)
        {
            m_Graph = NewObject<MaterialEdGraph>("", this);
        }

        return *m_Graph;
    }

    const MaterialEdGraph& Material::GetGraph() const
    {
        static MaterialEdGraph emptyGraph;
        return m_Graph ? *m_Graph : emptyGraph;
    }

    bool Material::LinkNodeDefGraph()
    {
        if (!m_Graph)
        {
            return false;
        }

        std::unordered_map<GUID, MaterialGraphNodeDef*, GUID::Hash> nodeDefByGuid;
        nodeDefByGuid.reserve(m_Graph->m_Nodes.size());

        for (const std::shared_ptr<MaterialEdGraphNode>& edNode : m_Graph->m_Nodes)
        {
            if (!edNode)
            {
                continue;
            }

            MaterialGraphNodeDef* nodeDef = edNode->GetNodeDef();
            if (nodeDef == nullptr || nodeDef->GetGuid().IsZero())
            {
                continue;
            }

            nodeDefByGuid[nodeDef->GetGuid()] = nodeDef;
        }

        for (const std::shared_ptr<MaterialEdGraphNode>& edNode : m_Graph->m_Nodes)
        {
            if (!edNode || edNode->GetNodeDef() == nullptr)
            {
                continue;
            }

            for (int32_t inputIndex = 0; MaterialGraphNodeDefInput* input = edNode->GetNodeDef()->GetInput(inputIndex); ++inputIndex)
            {
                if (input->ConnectedNodeDefGuid.IsZero())
                {
                    input->NodeDef = nullptr;
                    input->OutputIndex = 0;
                    continue;
                }

                const auto linkedNodeDef = nodeDefByGuid.find(input->ConnectedNodeDefGuid);
                if (linkedNodeDef == nodeDefByGuid.end())
                {
                    return false;
                }

                input->NodeDef = linkedNodeDef->second;
            }
        }

        return true;
    }

    bool Material::RebuildPinLinks()
    {
        if (!m_Graph)
        {
            return false;
        }

        for (const std::shared_ptr<MaterialEdGraphNode>& edNode : m_Graph->m_Nodes)
        {
            if (!edNode)
            {
                continue;
            }

            edNode->RebuildPins();
        }

        for (const std::shared_ptr<MaterialEdGraphNode>& toEdNode : m_Graph->m_Nodes)
        {
            if (!toEdNode || toEdNode->GetNodeDef() == nullptr)
            {
                continue;
            }

            for (int32_t inputIndex = 0; MaterialGraphNodeDefInput* input = toEdNode->GetNodeDef()->GetInput(inputIndex); ++inputIndex)
            {
                if (input->NodeDef == nullptr)
                {
                    continue;
                }

                MaterialEdGraphNode* fromEdNode = m_Graph->FindEdNodeByNodeDef(input->NodeDef);
                if (fromEdNode == nullptr)
                {
                    return false;
                }

                if (!m_Graph->ConnectPins(
                        *fromEdNode,
                        input->OutputIndex,
                        *toEdNode,
                        inputIndex,
                        m_ShadingModel,
                        m_BlendMode))
                {
                    return false;
                }
            }
        }

        return true;
    }

    bool Material::ValidateMaterialAsset(std::string* outError) const
    {
        if (!m_Graph || m_Graph->m_Nodes.empty())
        {
            if (outError != nullptr)
            {
                *outError = "Material graph has no nodes.";
            }
            return false;
        }

        bool hasMaterialOutput = false;
        for (const std::shared_ptr<MaterialEdGraphNode>& edNode : m_Graph->m_Nodes)
        {
            if (edNode && edNode->GetNodeDef() && edNode->GetNodeDef()->IsMaterialOutputNode())
            {
                hasMaterialOutput = true;
                break;
            }
        }

        if (!hasMaterialOutput)
        {
            if (outError != nullptr)
            {
                *outError = "Material graph requires at least one MaterialOutput node.";
            }
            return false;
        }

        if (!MaterialValueTypeUtil::ValidateGraphPinConnections(
                *m_Graph,
                m_ShadingModel,
                m_BlendMode,
                outError))
        {
            return false;
        }

        return true;
    }

    bool Material::FinalizeGraphAfterLoad(std::string* outError)
    {
        if (!LinkNodeDefGraph())
        {
            if (outError != nullptr)
            {
                *outError = "Failed to link material graph node definitions by GUID.";
            }
            return false;
        }

        if (!RebuildPinLinks())
        {
            if (outError != nullptr)
            {
                *outError = "Failed to rebuild material graph pin links.";
            }
            return false;
        }

        MaterialCapabilityUtil::PruneInvalidMaterialOutputLinks(*this);

        if (!ValidateMaterialAsset(outError))
        {
            return false;
        }

        return true;
    }

    bool Material::Compile()
    {
        MaterialCompileContext ctx;
        ctx.RHI = RenderSystem::Get().GetRHI();
        return MaterialCompiler::Compile(*this, ctx);
    }

    const MaterialGraphNodeDef_TextureObject* Material::FindTextureNodeBySlot(int slotIndex) const
    {
        if (!m_Graph)
        {
            return nullptr;
        }

        for (const std::shared_ptr<MaterialEdGraphNode>& graphNode : m_Graph->m_Nodes)
        {
            if (!graphNode)
            {
                continue;
            }

            if (const MaterialGraphNodeDef_TextureObject* textureNode =
                    dynamic_cast<const MaterialGraphNodeDef_TextureObject*>(graphNode->GetDefinition()))
            {
                if (textureNode->TextureSlotIndex == slotIndex)
                {
                    return textureNode;
                }
            }
        }

        return nullptr;
    }

    const MaterialGraphNodeDef_ScalarParameter* Material::FindScalarNodeBySlot(int slotIndex) const
    {
        if (!m_Graph)
        {
            return nullptr;
        }

        for (const std::shared_ptr<MaterialEdGraphNode>& graphNode : m_Graph->m_Nodes)
        {
            if (!graphNode)
            {
                continue;
            }

            if (const MaterialGraphNodeDef_ScalarParameter* scalarNode =
                    dynamic_cast<const MaterialGraphNodeDef_ScalarParameter*>(graphNode->GetDefinition()))
            {
                if (scalarNode->UniformSlotIndex == slotIndex)
                {
                    return scalarNode;
                }
            }
        }

        return nullptr;
    }

    bool Material::CommitCompileResult(const MaterialCompileResult& result, const MaterialCompileContext& ctx)
    {
        if (ctx.RHI == nullptr)
        {
            m_LastCompileDiagnostics.push_back({
                MaterialCompileDiagnostic::Error,
                "Material::CommitCompileResult requires a valid RHI in MaterialCompileContext.",
            });
            return false;
        }

        std::string compileError;
        m_GPUShader = ctx.RHI->RHICreateShader(
            result.FullVertexShader,
            result.FullFragmentShader,
            &compileError);
        m_ShaderCompileLog = compileError;
        if (!m_GPUShader)
        {
            if (!compileError.empty())
            {
                m_LastCompileDiagnostics.push_back({
                    MaterialCompileDiagnostic::Error,
                    compileError,
                });
            }
            return false;
        }

        m_ParameterLayout = result.ParameterLayout;
        m_TextureParameters.clear();
        m_ScalarParameters.clear();

        for (const MaterialShaderParameterDesc& parameterDesc : m_ParameterLayout.Parameters)
        {
            if (parameterDesc.Type == MaterialShaderParameterType::Texture2D)
            {
                MaterialTextureParameter textureParameter;
                textureParameter.SlotIndex = parameterDesc.SlotIndex;
                textureParameter.ShaderSymbolName = parameterDesc.ShaderSymbolName;

                if (const MaterialGraphNodeDef_TextureObject* textureNode = FindTextureNodeBySlot(parameterDesc.SlotIndex))
                {
                    textureParameter.ParameterName = textureNode->ParameterName;
                    textureParameter.Value = textureNode->DefaultTexture;
                }

                if (!textureParameter.Value)
                {
                    textureParameter.Value = Texture2D::CreateSolidRGBA(*ctx.RHI, 255, 255, 255, 255);
                }

                m_TextureParameters.push_back(textureParameter);
            }
            else if (parameterDesc.Type == MaterialShaderParameterType::Scalar)
            {
                MaterialScalarParameter scalarParameter;
                scalarParameter.SlotIndex = parameterDesc.SlotIndex;
                scalarParameter.ShaderSymbolName = parameterDesc.ShaderSymbolName;

                if (const MaterialGraphNodeDef_ScalarParameter* scalarNode = FindScalarNodeBySlot(parameterDesc.SlotIndex))
                {
                    scalarParameter.ParameterName = scalarNode->ParameterName;
                    scalarParameter.Value = scalarNode->DefaultValue;
                }

                m_ScalarParameters.push_back(scalarParameter);
            }
        }

        RHICommandList cmdList(ctx.RHI);
        std::vector<RHIBindingLayoutEntry> layoutEntries;
        layoutEntries.reserve(m_TextureParameters.size() + (m_ScalarParameters.empty() ? 0u : 1u));
        for (const MaterialTextureParameter& textureParameter : m_TextureParameters)
        {
            layoutEntries.push_back({
                static_cast<uint32_t>(textureParameter.SlotIndex),
                RHIBindingType::TextureSRV,
                static_cast<uint32_t>(textureParameter.SlotIndex),
                RHIGraphicsShaderStage::Pixel,
            });
        }

        if (!m_ScalarParameters.empty())
        {
            int maxSlot = 0;
            for (const MaterialScalarParameter& scalarParameter : m_ScalarParameters)
            {
                maxSlot = std::max(maxSlot, scalarParameter.SlotIndex);
            }
            m_ScalarParamsUBOSize = static_cast<uint32_t>((maxSlot + 1) * 16u);

            RHIBufferCreateDesc uboDesc;
            uboDesc.Usage = RHIBufferUsage::Uniform;
            uboDesc.ByteSize = m_ScalarParamsUBOSize;
            m_ScalarParamsUBO = cmdList.CreateBuffer(uboDesc, nullptr);

            layoutEntries.push_back({
                EngineShaderBindings::kSet2_MaterialParamsUBO,
                RHIBindingType::UniformBuffer,
                EngineShaderBindings::kSet2_MaterialParamsUBO,
                RHIGraphicsShaderStage::Pixel,
            });
        }
        else
        {
            m_ScalarParamsUBO.reset();
            m_ScalarParamsUBOSize = 0;
        }

        m_MaterialBindingLayout = layoutEntries.empty()
            ? nullptr
            : cmdList.CreateBindingLayout(layoutEntries);

        return true;
    }

    MaterialTextureParameter* Material::FindTextureParameter(const std::string& parameterName)
    {
        for (MaterialTextureParameter& parameter : m_TextureParameters)
        {
            if (parameter.ParameterName == parameterName)
            {
                return &parameter;
            }
        }

        return nullptr;
    }

    MaterialTextureParameter* Material::FindTextureParameterBySlot(int slotIndex)
    {
        for (MaterialTextureParameter& parameter : m_TextureParameters)
        {
            if (parameter.SlotIndex == slotIndex)
            {
                return &parameter;
            }
        }

        return nullptr;
    }

    MaterialScalarParameter* Material::FindScalarParameter(const std::string& parameterName)
    {
        for (MaterialScalarParameter& parameter : m_ScalarParameters)
        {
            if (parameter.ParameterName == parameterName)
            {
                return &parameter;
            }
        }

        return nullptr;
    }

    MaterialScalarParameter* Material::FindScalarParameterBySlot(int slotIndex)
    {
        for (MaterialScalarParameter& parameter : m_ScalarParameters)
        {
            if (parameter.SlotIndex == slotIndex)
            {
                return &parameter;
            }
        }

        return nullptr;
    }

    void Material::SetTextureParameter(const std::string& parameterName, std::shared_ptr<Texture2D> texture)
    {
        if (MaterialTextureParameter* parameter = FindTextureParameter(parameterName))
        {
            parameter->Value = std::move(texture);
        }
    }

    void Material::SetScalarParameter(const std::string& parameterName, float value)
    {
        if (MaterialScalarParameter* parameter = FindScalarParameter(parameterName))
        {
            parameter->Value = value;
        }
    }

    void Material::BindForDraw(RHICommandList& cmdList) const
    {
        if (m_ScalarParamsUBO && m_ScalarParamsUBOSize > 0)
        {
            const uint32_t floatCount = m_ScalarParamsUBOSize / 16u;
            std::vector<float> scalarData(floatCount * 4u, 0.0f);
            for (const MaterialScalarParameter& scalarParameter : m_ScalarParameters)
            {
                if (scalarParameter.SlotIndex >= 0 &&
                    static_cast<uint32_t>(scalarParameter.SlotIndex) < floatCount)
                {
                    scalarData[static_cast<size_t>(scalarParameter.SlotIndex) * 4u] = scalarParameter.Value;
                }
            }
            m_ScalarParamsUBO->UpdateSubresource(scalarData.data(), 0, m_ScalarParamsUBOSize);
        }

        if (!m_MaterialBindingLayout)
        {
            return;
        }

        const std::vector<RHIBindingLayoutEntry>& entries = m_MaterialBindingLayout->GetEntries();
        std::vector<RHIBindingResource> resources(entries.size());
        std::vector<std::shared_ptr<RHIShaderResourceView>> textureSrvs;
        textureSrvs.reserve(m_TextureParameters.size());

        for (size_t entryIndex = 0; entryIndex < entries.size(); ++entryIndex)
        {
            const RHIBindingLayoutEntry& entry = entries[entryIndex];
            if (entry.Type == RHIBindingType::UniformBuffer)
            {
                resources[entryIndex].Type = RHIBindingType::UniformBuffer;
                resources[entryIndex].Buffer = m_ScalarParamsUBO.get();
                continue;
            }

            MaterialTextureParameter const* textureParameter = nullptr;
            for (const MaterialTextureParameter& candidate : m_TextureParameters)
            {
                if (candidate.SlotIndex == static_cast<int>(entry.ShaderBinding))
                {
                    textureParameter = &candidate;
                    break;
                }
            }
            if (!textureParameter || !textureParameter->Value)
            {
                continue;
            }

            RHITexture* modernTexture = textureParameter->Value->GetRHITexture();
            if (!modernTexture)
            {
                continue;
            }

            RHITextureSRVDesc srvDesc;
            srvDesc.Texture = modernTexture;
            textureSrvs.push_back(std::make_shared<OpenGLRHIShaderResourceView>(srvDesc));
            resources[entryIndex].Type = RHIBindingType::TextureSRV;
            resources[entryIndex].TextureSRV = textureSrvs.back().get();
        }

        if (RHIBindingSetRef materialSet = cmdList.CreateBindingSet(m_MaterialBindingLayout.get(), resources))
        {
            cmdList.SetBindingSet(EngineShaderBindings::kSetMaterial, materialSet.get());
        }
    }
}
