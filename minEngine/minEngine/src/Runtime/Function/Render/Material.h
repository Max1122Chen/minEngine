#pragma once
#include "Core.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Function/Render/Texture.h"
#include "Runtime/Function/Render/MaterialInterface.h"
#include "Runtime/Function/Render/Material/MaterialCompiler/MaterialCompileTypes.h"
#include "Runtime/Function/Render/Material/MaterialEdGraph.h"
#include "Runtime/Function/Render/Material/MaterialGraphNodeDefs/MaterialGraphNodeDef.h"
#include "Runtime/Function/Render/MaterialRenderProxy.h"

#include "Runtime/Function/Render/RHI/RHIShaderBinding.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"

#include <memory>
#include <string>
#include <vector>

namespace minEngine
{
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
    class Material : public MaterialInterface
    {
        ME_GENERATED_BODY()
        friend class MaterialCompiler;
        friend class DefaultMaterialRenderProxy;

    public:
        Material();
        virtual ~Material() = default;

        bool Compile();
        /** Compiles a skinned VS variant; requires a successful rigid Compile() first (reuses material bindings). */
        bool EnsureSkinnedCompiled();

        void SetTextureParameter(const std::string& parameterName, std::shared_ptr<Texture2D> texture);
        void SetScalarParameter(const std::string& parameterName, float value);

        Material* GetBaseMaterial() const override { return const_cast<Material*>(this); }
        MaterialRenderProxy* GetRenderProxy() const override { return m_RenderProxy.get(); }
        std::shared_ptr<Material> ResolveParentMaterial(
            const std::shared_ptr<MaterialInterface>& self) const override;

        bool IsCompiledForDraw() const override
        {
            return m_GPUShader != nullptr && m_GPUShader->IsValid() && !m_ParameterLayout.Parameters.empty();
        }

        bool IsCompiledForSkinnedDraw() const
        {
            return IsCompiledForDraw() && m_GPUShaderSkinned != nullptr && m_GPUShaderSkinned->IsValid();
        }

        RHIShader* GetGPUShader() const { return m_GPUShader.get(); }
        RHIShader* GetGPUShaderSkinned() const { return m_GPUShaderSkinned.get(); }
        uint32_t GetCompileGeneration() const { return m_CompileGeneration; }
        RHIShaderBindingSetLayout* GetMaterialShaderBindingSetLayout() const
        {
            return m_MaterialShaderBindingSetLayout.get();
        }
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

        bool IsTranslucent() const override { return m_BlendMode == MaterialBlendMode::Translucent; }
        bool IsMasked() const override { return m_BlendMode == MaterialBlendMode::Masked; }

    private:
        void EnsureRenderProxy();

        RHIShaderRef m_GPUShader;
        RHIShaderRef m_GPUShaderSkinned;
        RHIShaderBindingSetLayoutRef m_MaterialShaderBindingSetLayout;
        uint32_t m_CompileGeneration = 0;
        std::string m_ShaderCompileLog;
        std::unique_ptr<MaterialRenderProxy> m_RenderProxy;

        bool CommitCompileResult(const MaterialCompileResult& result, const MaterialCompileContext& ctx);

        const MaterialGraphNodeDef_TextureObject* FindTextureNodeBySlot(int slotIndex) const;
        const MaterialGraphNodeDef_ScalarParameter* FindScalarNodeBySlot(int slotIndex) const;

        MaterialTextureParameter* FindTextureParameter(const std::string& parameterName);
        MaterialTextureParameter* FindTextureParameterBySlot(int slotIndex);
        MaterialScalarParameter* FindScalarParameter(const std::string& parameterName);
        MaterialScalarParameter* FindScalarParameterBySlot(int slotIndex);
    };
}

#include "Generated/Reflection/Material.gen.h"
