#pragma once

#include "Core.h"
#include "Runtime/Function/Render/Texture.h"
#include "Runtime/Function/Render/RHI/RHIShaderBinding.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"
#include "Runtime/Function/Render/RHI/RHITextureViewCache.h"

#include <memory>
#include <string>
#include <vector>

namespace minEngine
{
    class Material;
    class MaterialInstance;
    class RHICommandList;
    class RHIShaderResourceView;

    /**
     * Render-side material binding view (UE FMaterialRenderProxy analogue).
     * Owned by Material / MaterialInstance; SceneProxy and MeshDrawCommand hold raw pointers only.
     * GPU BindingSet / scalar UBO / texture SRV cache live on this object. Parameter values come from virtual Get*.
     */
    class MaterialRenderProxy
    {
    public:
        virtual ~MaterialRenderProxy() = default;

        virtual Material* GetBaseMaterial() const = 0;

        virtual bool IsCompiledForDraw() const = 0;
        virtual bool IsCompiledForSkinnedDraw() const = 0;
        virtual bool IsTranslucent() const = 0;
        virtual bool IsMasked() const = 0;

        virtual RHIShader* GetGPUShader() const = 0;
        virtual RHIShader* GetGPUShaderSkinned() const = 0;
        virtual RHIShaderBindingSetLayout* GetMaterialShaderBindingSetLayout() const = 0;
        virtual RHIShaderBindingSet* GetMaterialShaderBindingSet() const;

        virtual bool GetScalarParameterValue(const std::string& name, float& outValue) const = 0;
        virtual bool GetTextureParameterValue(
            const std::string& name, std::shared_ptr<Texture2D>& outTexture) const = 0;
        virtual bool GetScalarParameterValueBySlot(int slotIndex, float& outValue) const = 0;
        virtual bool GetTextureParameterValueBySlot(
            int slotIndex, std::shared_ptr<Texture2D>& outTexture) const = 0;

        virtual void BindForDraw(RHICommandList& cmdList);
        virtual void InvalidateParameterBindings();

        /** Full GPU-cache rebuild after Material compile changes layout. */
        virtual void NotifyCompiled(RHICommandList& cmdList);

    protected:
        void EnsureScalarUniformBuffer(RHICommandList& cmdList);
        void RebuildShaderBindingSet(RHICommandList& cmdList);

        bool m_bParameterBindingsDirty = true;
        uint32_t m_SeenCompileGeneration = 0;

        RHIShaderBindingSetRef m_MaterialShaderBindingSet;
        RHITextureViewCache m_TextureViewCache;
        std::vector<std::shared_ptr<RHIShaderResourceView>> m_TextureSRVs;
        RHIBufferRef m_ScalarParamsUBO;
        uint32_t m_ScalarParamsUBOSize = 0;
    };

    /** Default proxy owned by Material; reads Material default parameter tables. */
    class DefaultMaterialRenderProxy : public MaterialRenderProxy
    {
    public:
        explicit DefaultMaterialRenderProxy(Material& material);
        ~DefaultMaterialRenderProxy() override = default;

        DefaultMaterialRenderProxy(const DefaultMaterialRenderProxy&) = delete;
        DefaultMaterialRenderProxy& operator=(const DefaultMaterialRenderProxy&) = delete;

        Material* GetBaseMaterial() const override;

        bool IsCompiledForDraw() const override;
        bool IsCompiledForSkinnedDraw() const override;
        bool IsTranslucent() const override;
        bool IsMasked() const override;

        RHIShader* GetGPUShader() const override;
        RHIShader* GetGPUShaderSkinned() const override;
        RHIShaderBindingSetLayout* GetMaterialShaderBindingSetLayout() const override;

        bool GetScalarParameterValue(const std::string& name, float& outValue) const override;
        bool GetTextureParameterValue(
            const std::string& name, std::shared_ptr<Texture2D>& outTexture) const override;
        bool GetScalarParameterValueBySlot(int slotIndex, float& outValue) const override;
        bool GetTextureParameterValueBySlot(
            int slotIndex, std::shared_ptr<Texture2D>& outTexture) const override;

    private:
        Material* m_Material = nullptr;
    };

    /**
     * Instance proxy (UE FMaterialInstanceResource analogue).
     * Overrides win; misses forward to the parent Material proxy. Does not compile.
     */
    class InstanceMaterialRenderProxy : public MaterialRenderProxy
    {
    public:
        explicit InstanceMaterialRenderProxy(MaterialInstance& instance);
        ~InstanceMaterialRenderProxy() override = default;

        InstanceMaterialRenderProxy(const InstanceMaterialRenderProxy&) = delete;
        InstanceMaterialRenderProxy& operator=(const InstanceMaterialRenderProxy&) = delete;

        Material* GetBaseMaterial() const override;

        bool IsCompiledForDraw() const override;
        bool IsCompiledForSkinnedDraw() const override;
        bool IsTranslucent() const override;
        bool IsMasked() const override;

        RHIShader* GetGPUShader() const override;
        RHIShader* GetGPUShaderSkinned() const override;
        RHIShaderBindingSetLayout* GetMaterialShaderBindingSetLayout() const override;

        bool GetScalarParameterValue(const std::string& name, float& outValue) const override;
        bool GetTextureParameterValue(
            const std::string& name, std::shared_ptr<Texture2D>& outTexture) const override;
        bool GetScalarParameterValueBySlot(int slotIndex, float& outValue) const override;
        bool GetTextureParameterValueBySlot(
            int slotIndex, std::shared_ptr<Texture2D>& outTexture) const override;

    private:
        MaterialRenderProxy* GetParentProxy() const;

        MaterialInstance* m_Instance = nullptr;
    };
}
