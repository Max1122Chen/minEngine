#include "Material.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Object/ObjectCast.h"
#include "Runtime/Function/Render/Material/MaterialCompiler/MaterialCompiler.h"
#include "Runtime/Function/Render/Material/MaterialGraphNodeDefs/MaterialGraphNodeDef.h"
#include "Runtime/Function/Render/Material/MaterialCapability.h"
#include "Runtime/Function/Render/Material/MaterialValueType.h"
#include "Runtime/Function/Render/MaterialRenderProxy.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/EngineShaderBindings.h"
#include "Runtime/Function/Render/EngineShaderUtils.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "RHI/RHIShader.h"
#include "Texture.h"

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <vector>

namespace minEngine
{
    Material::Material()
    {
        EnsureRenderProxy();
        // Publishes the authored state (blend mode, empty tables) so a proxy created before the
        // first compile answers translucent/masked exactly as the material does.
        m_RenderProxy->PublishGameData();
    }

    std::shared_ptr<Material> Material::ResolveParentMaterial(
        const std::shared_ptr<MaterialInterface>& self) const
    {
        return std::static_pointer_cast<Material>(self);
    }

    void Material::EnsureRenderProxy()
    {
        if (!m_RenderProxy)
        {
            m_RenderProxy = MakeMaterialRenderProxy<DefaultMaterialRenderProxy>(*this);
        }
    }

    std::shared_ptr<Material> Material::CreateRuntimeInstance(const std::string& instanceName) const
    {
        // A sibling reuses this material's program instead of compiling one, so a program that is
        // still queued for the graphics role is enough: both materials name the same handle, and
        // both become drawable when the graphics role creates it.
        if (!HasCompiledProgram() || !m_Graph)
        {
            return nullptr;
        }

        const std::string name = instanceName.empty() ? GetName() : instanceName;
        std::shared_ptr<Material> instance = NewObject<Material>(name, nullptr, GenerateGUID());
        if (!instance)
        {
            return nullptr;
        }

        instance->m_ShadingModel = m_ShadingModel;
        instance->m_BlendMode = m_BlendMode;
        instance->m_Graph = m_Graph;
        instance->m_ParameterLayout = m_ParameterLayout;
        instance->m_TextureParameters = m_TextureParameters;
        instance->m_ScalarParameters = m_ScalarParameters;
        instance->m_GPUShader = m_GPUShader;
        instance->m_GPUShaderSkinned = m_GPUShaderSkinned;
        instance->m_MaterialShaderBindingSetLayout = m_MaterialShaderBindingSetLayout;
        instance->m_CompileGeneration = m_CompileGeneration;
        instance->EnsureRenderProxy();
        // Nothing compiles for a sibling, so the publish the compile path would have done has to
        // happen here: the copied program, layout and tables are what the render role draws with.
        instance->m_RenderProxy->PublishGameData();
        return instance;
    }

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

    bool Material::EnsureSkinnedCompiled()
    {
        if (IsCompiledForSkinnedDraw())
        {
            return true;
        }
        if (!HasCompiledProgram() && !Compile())
        {
            return false;
        }
        if (m_GPUShaderSkinned != nullptr && !m_GPUShaderSkinned->HasNativeCreateFailed())
        {
            // The variant was already built for this generation: its program is queued for the
            // graphics role or already exists. Requesting it again would make a second program
            // for the same material and publish a snapshot of the same state.
            return true;
        }
        MaterialCompileContext ctx;
        ctx.RHI = RenderSystem::Get().GetRHI();
        const bool bCompiled = MaterialCompiler::CompileSkinnedVariant(*this, ctx);
        if (bCompiled && m_RenderProxy)
        {
            // A skinned variant is a second GPU program; without publishing it the render role has
            // no snapshot that names it and the mesh keeps drawing unskinned.
            m_RenderProxy->PublishGameData();
        }
        return bCompiled;
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
                    Cast<MaterialGraphNodeDef_TextureObject>(graphNode->GetDefinition()))
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
                    Cast<MaterialGraphNodeDef_ScalarParameter>(graphNode->GetDefinition()))
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
        m_GPUShader = EngineShaderUtils::CreateShaderFromSpirvSources(
            *ctx.RHI,
            result.FullVertexShader,
            result.FullFragmentShader,
            GetName().empty() ? "Material" : GetName(),
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

        if (!m_GPUShader->IsValid() && !m_GPUShader->HasNativeCreateFailed())
        {
            // The program is queued for the graphics role. The compile itself succeeded, so this
            // is not a failure - but it is the one moment someone might look for a drawable
            // material and find none, so the state is named once per compile.
            ME_LOG(
                LogRender,
                Info,
                "Material '{}': program queued for the graphics role; it becomes drawable at the "
                "start of the next render frame.",
                GetName().empty() ? "Material" : GetName());
        }

        m_ParameterLayout = result.ParameterLayout;
        m_TextureParameters.clear();
        m_ScalarParameters.clear();

        // compile-time slot → SamplerDesc from TextureSample Override (first wins).
        std::unordered_map<int, RHISamplerDesc> slotOverrideSamplers;
        if (m_Graph)
        {
            for (const std::shared_ptr<MaterialEdGraphNode>& graphNode : m_Graph->m_Nodes)
            {
                if (!graphNode)
                {
                    continue;
                }

                const MaterialGraphNodeDef_TextureSample* sampleNode =
                    Cast<MaterialGraphNodeDef_TextureSample>(graphNode->GetDefinition());
                if (sampleNode == nullptr || !sampleNode->bOverrideSampler)
                {
                    continue;
                }

                const MaterialGraphNodeDefInput* textureInput = sampleNode->GetInput(0);
                if (textureInput == nullptr || !textureInput->IsConnected())
                {
                    continue;
                }

                const MaterialGraphNodeDef_TextureObject* textureObject =
                    Cast<MaterialGraphNodeDef_TextureObject>(textureInput->NodeDef);
                if (textureObject == nullptr)
                {
                    continue;
                }

                const int slotIndex = textureObject->TextureSlotIndex;
                const RHISamplerDesc overrideDesc = sampleNode->MakeOverrideSamplerDesc();
                const auto inserted = slotOverrideSamplers.emplace(slotIndex, overrideDesc);
                if (!inserted.second && inserted.first->second != overrideDesc)
                {
                    ME_LOG(
                        LogRender,
                        Warn,
                        "Material '{}': TextureSample Override conflict on slot {} — first Override wins.",
                        GetName(),
                        slotIndex);
                }
            }
        }

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

                const auto overrideIt = slotOverrideSamplers.find(parameterDesc.SlotIndex);
                if (overrideIt != slotOverrideSamplers.end())
                {
                    textureParameter.bOverrideSampler = true;
                    textureParameter.SamplerDesc = overrideIt->second;
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
        std::vector<RHIShaderBindingSetLayoutEntry> layoutEntries;
        layoutEntries.reserve(m_TextureParameters.size() + (m_ScalarParameters.empty() ? 0u : 1u));
        for (const MaterialTextureParameter& textureParameter : m_TextureParameters)
        {
            layoutEntries.push_back({
                static_cast<uint32_t>(textureParameter.SlotIndex),
                RHIShaderBindingType::TextureSRV,
                static_cast<uint32_t>(textureParameter.SlotIndex),
                RHIGraphicsShaderStage::Pixel,
            });
        }

        if (!m_ScalarParameters.empty())
        {
            layoutEntries.push_back({
                EngineShaderBindings::kSet2_MaterialParamsUBO,
                RHIShaderBindingType::UniformBuffer,
                EngineShaderBindings::kSet2_MaterialParamsUBO,
                RHIGraphicsShaderStage::Pixel,
            });
        }

        m_MaterialShaderBindingSetLayout = layoutEntries.empty()
            ? nullptr
            : cmdList.CreateShaderBindingSetLayout(layoutEntries);

        ++m_CompileGeneration;
        EnsureRenderProxy();
        // Game role: publish the new generation, shaders and layout. The GPU objects built from them
        // (binding set, scalar UBO) are created on the render role the first time it draws this
        // generation, which is where a graphics handle may legally be created.
        m_RenderProxy->PublishGameData();
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
            EnsureRenderProxy();
            if (m_RenderProxy)
            {
                // One publish per tick, not one per write: a sprite syncing ten parameters must
                // still hand the render role a single snapshot.
                m_RenderProxy->MarkGameDataDirty();
            }
        }
    }

    void Material::SetScalarParameter(const std::string& parameterName, float value)
    {
        if (MaterialScalarParameter* parameter = FindScalarParameter(parameterName))
        {
            parameter->Value = value;
            EnsureRenderProxy();
            if (m_RenderProxy)
            {
                m_RenderProxy->MarkGameDataDirty();
            }
        }
    }
}
