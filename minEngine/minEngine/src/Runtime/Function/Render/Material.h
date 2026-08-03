#pragma once
#include "Core.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Function/Render/Texture.h"
#include "Runtime/Resource/Asset.h"
#include "Runtime/Function/Render/Material/MaterialCompiler/MaterialCompileTypes.h"
#include "Runtime/Function/Render/Material/MaterialEdGraph.h"
#include "Runtime/Function/Render/Material/MaterialGraphNodeDefs/MaterialGraphNodeDef.h"

#include "Runtime/Function/Render/RHI/RHIShaderBinding.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"
#include "Runtime/Function/Render/RHI/RHITextureViewCache.h"
#include "Runtime/Function/Render/RHI/RHITextureViewCache.h"

#include <string>
#include <vector>

namespace minEngine
{
    class RHIShaderResourceView;
    class RHICommandList;
    class MaterialCompiler;
    class MaterialGraphNodeDef_TextureObject;
    class MaterialGraphNodeDef_ScalarParameter;

    struct MaterialTextureParameter
    {
        std::string ParameterName;
        int SlotIndex = 0;
        std::string ShaderSymbolName;
        std::shared_ptr<Texture2D> Value;
    };

    struct MaterialScalarParameter
    {
        std::string ParameterName;
        int SlotIndex = 0;
        std::string ShaderSymbolName;
        float Value = 0.0f;
    };

    ME_CLASS()
    class Material : public Asset
    {
        ME_GENERATED_BODY(Material)
        friend class MaterialCompiler;

    public:
        Material() = default;
        virtual ~Material() = default;

        bool Compile();
        void BindForDraw(RHICommandList& cmdList) const;

        void SetTextureParameter(const std::string& parameterName, std::shared_ptr<Texture2D> texture);
        void SetScalarParameter(const std::string& parameterName, float value);

        bool IsCompiledForDraw() const
        {
            return m_GPUShader != nullptr && m_GPUShader->IsValid() && !m_ParameterLayout.Parameters.empty();
        }

        RHIShader* GetGPUShader() const { return m_GPUShader.get(); }
        RHIShaderBindingSetLayout* GetMaterialShaderBindingSetLayout() const { return m_MaterialShaderBindingSetLayout.get(); }
        RHIShaderBindingSet* GetMaterialShaderBindingSet() const { return m_MaterialShaderBindingSet.get(); }
        const std::string& GetShaderCompileLog() const { return m_ShaderCompileLog; }

        MaterialEdGraph& GetGraph();
        const MaterialEdGraph& GetGraph() const;

        bool LinkNodeDefGraph();
        bool RebuildPinLinks();
        bool ValidateMaterialAsset(std::string* outError = nullptr) const;
        bool FinalizeGraphAfterLoad(std::string* outError = nullptr);

        ME_PROPERTY()
        MaterialShadingModel m_ShadingModel = MaterialShadingModel::Unlit;

        ME_PROPERTY()
        MaterialBlendMode m_BlendMode = MaterialBlendMode::Opaque;

        ME_PROPERTY(Instanced)
        std::shared_ptr<MaterialEdGraph> m_Graph;

        std::vector<MaterialCompileDiagnostic> m_LastCompileDiagnostics;
        MaterialShaderParameterLayout m_ParameterLayout;
        std::vector<MaterialTextureParameter> m_TextureParameters;
        std::vector<MaterialScalarParameter> m_ScalarParameters;

        bool IsTranslucent() const { return m_BlendMode == MaterialBlendMode::Translucent; }
        bool IsMasked() const { return m_BlendMode == MaterialBlendMode::Masked; }

    private:
        RHIShaderRef m_GPUShader;
        RHIShaderBindingSetLayoutRef m_MaterialShaderBindingSetLayout;
        RHIShaderBindingSetRef m_MaterialShaderBindingSet;
        RHITextureViewCache m_TextureViewCache;
        std::vector<std::shared_ptr<RHIShaderResourceView>> m_TextureSRVs;
        RHIBufferRef m_ScalarParamsUBO;
        uint32_t m_ScalarParamsUBOSize = 0;
        std::string m_ShaderCompileLog;

        bool CommitCompileResult(const MaterialCompileResult& result, const MaterialCompileContext& ctx);
        void RebuildMaterialShaderBindingSet(RHICommandList& cmdList);

        const MaterialGraphNodeDef_TextureObject* FindTextureNodeBySlot(int slotIndex) const;
        const MaterialGraphNodeDef_ScalarParameter* FindScalarNodeBySlot(int slotIndex) const;

        MaterialTextureParameter* FindTextureParameter(const std::string& parameterName);
        MaterialTextureParameter* FindTextureParameterBySlot(int slotIndex);
        MaterialScalarParameter* FindScalarParameter(const std::string& parameterName);
        MaterialScalarParameter* FindScalarParameterBySlot(int slotIndex);
    };
}

#include "Generated/Reflection/Material.gen.h"
