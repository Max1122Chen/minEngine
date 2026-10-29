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
#include "Runtime/Function/Render/RHI/RHISampler.h"

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
        /** Compile-time sampler intent from TextureSample Override. */
        bool bOverrideSampler = false;
        RHISamplerDesc SamplerDesc{};
    };

    struct MaterialScalarParameter
    {
        std::string ParameterName;
        int SlotIndex = 0;
        std::string ShaderSymbolName;
        float Value = 0.0f;
    };

    ME_CLASS()
    class MINENGINE_API Material : public MaterialInterface
    {
        ME_GENERATED_BODY()
        friend class MaterialCompiler;
        friend class DefaultMaterialRenderProxy;

    public:
        Material();
        ~Material() override
        {
            // A scene proxy can still hold the render object, so it has to be told that the tables
            // it publishes from are about to be freed: it keeps drawing from its last snapshot
            // instead of publishing from a freed owner.
            if (m_RenderProxy)
            {
                m_RenderProxy->DetachFromOwner();
            }
        }

        bool Compile();
        /** Compiles a skinned VS variant; requires a successful rigid Compile() first (reuses material bindings). */
        bool EnsureSkinnedCompiled();

        /**
         * Creates a runtime sibling that reuses this material's compiled program, parameter layout,
         * graph and GPU binding-set layout, WITHOUT recompiling.
         *
         * Parameter tables are copied, so each sibling keeps its own tint/opacity/UV values — sharing
         * the program never shares state. Requires IsCompiledForDraw(); returns nullptr otherwise.
         * Siblings are runtime-only: not meant for serialization or graph editing.
         */
        std::shared_ptr<Material> CreateRuntimeInstance(const std::string& instanceName = {}) const;

        void SetTextureParameter(const std::string& parameterName, std::shared_ptr<Texture2D> texture);
        void SetScalarParameter(const std::string& parameterName, float value);

        Material* GetBaseMaterial() const override { return const_cast<Material*>(this); }
        MaterialRenderProxy* GetRenderProxy() const override { return m_RenderProxy.get(); }
        MaterialRenderProxyRef GetRenderProxyRef() const override { return m_RenderProxy; }
        std::shared_ptr<Material> ResolveParentMaterial(
            const std::shared_ptr<MaterialInterface>& self) const override;

        /**
         * True only when the program exists and can be bound right now.
         *
         * A compile that ran on this thread while the program itself is created by the graphics
         * role is finished but not drawable yet, and answers false here: the shader handle says
         * so through IsReady(), so nothing has to guess from a zero program name.
         */
        bool IsCompiledForDraw() const override
        {
            return m_GPUShader != nullptr && m_GPUShader->IsValid() && !m_ParameterLayout.Parameters.empty();
        }

        bool IsCompiledForSkinnedDraw() const
        {
            return IsCompiledForDraw() && m_GPUShaderSkinned != nullptr && m_GPUShaderSkinned->IsValid();
        }

        /**
         * The compile finished: a program handle and a parameter layout exist.
         *
         * Different from IsCompiledForDraw() by exactly the deferred window - a program queued for
         * the graphics role is compiled, but naming it in a draw would name nothing. Callers that
         * hand the program on rather than draw with it (deriving a runtime sibling, creating the
         * factory instance) ask this one.
         */
        bool HasCompiledProgram() const
        {
            return m_GPUShader != nullptr && !m_ParameterLayout.Parameters.empty();
        }

        /** The program is queued for the graphics role: compiled, not created, not failed. */
        bool IsProgramPendingMaterialization() const
        {
            return m_GPUShader != nullptr && !m_GPUShader->IsReady()
                && !m_GPUShader->HasNativeCreateFailed();
        }

        /** The program was handed to the graphics role and its creation failed there. */
        bool HasProgramCreateFailed() const
        {
            return m_GPUShader != nullptr && m_GPUShader->HasNativeCreateFailed();
        }

        RHIShader* GetGPUShader() const { return m_GPUShader.get(); }
        RHIShader* GetGPUShaderSkinned() const { return m_GPUShaderSkinned.get(); }
        uint32_t GetCompileGeneration() const { return m_CompileGeneration; }
        RHIShaderBindingSetLayout* GetMaterialShaderBindingSetLayout() const
        {
            return m_MaterialShaderBindingSetLayout.get();
        }
        /**
         * Why the program is not there, as far as the program itself knows.
         *
         * Read through the handle rather than the copy taken at compile time: a queued program has
         * no message yet, and the one the driver produced lands on the handle when the graphics
         * role creates it. A caller that asks later therefore sees the failure instead of the
         * "queued" placeholder it was given during the compile.
         */
        const std::string& GetShaderCompileLog() const
        {
            if (m_GPUShader != nullptr && !m_GPUShader->GetCompileLog().empty())
            {
                m_ShaderCompileLog = m_GPUShader->GetCompileLog();
            }
            return m_ShaderCompileLog;
        }

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
        /** Written by the compile and refreshed from the program handle whenever it is read later. */
        mutable std::string m_ShaderCompileLog;

        /**
         * Built once and kept for this material's whole lifetime.
         *
         * Compiles and parameter changes rebuild this proxy's GPU binding state in place, never the
         * proxy object, so a reference held across a recompile still draws the current state. The
         * reference is shared: this material keeps one, and every scene proxy that draws with it
         * keeps another, so the proxy - and the snapshot it published - outlives this material.
         */
        MaterialRenderProxyRef m_RenderProxy;

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