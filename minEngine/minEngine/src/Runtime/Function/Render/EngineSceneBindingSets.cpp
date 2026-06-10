#include "EngineSceneBindingSets.h"

#include "EngineShaderBindings.h"
#include "Runtime/Function/Render/OpenGL/OpenGLRHIResources.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"

namespace minEngine
{
    namespace
    {
        using namespace EngineShaderBindings;

        std::shared_ptr<RHIShaderResourceView> MakeTextureSRV(RHITexture* texture, int32_t arraySlice = -1)
        {
            if (!texture)
            {
                return nullptr;
            }

            RHITextureSRVDesc srvDesc;
            srvDesc.Texture = texture;
            srvDesc.ArraySlice = arraySlice;
            return std::make_shared<OpenGLRHIShaderResourceView>(srvDesc);
        }

    }

    void EngineSceneBindingSets::Initialize(RHICommandList& cmdList)
    {
        m_SceneSet0Layout = cmdList.CreateBindingLayout({
            {kSet0_PerFrame, RHIBindingType::UniformBuffer, kGL_PerFrameUBO, RHIGraphicsShaderStage::Vertex},
            {kSet0_Lights, RHIBindingType::UniformBuffer, kGL_LightsUBO, RHIGraphicsShaderStage::Pixel},
            {kSet0_PerObject, RHIBindingType::UniformBuffer, kGL_PerObjectUBO, RHIGraphicsShaderStage::Vertex},
        });

        m_SceneSet1Layout = cmdList.CreateBindingLayout({
            {kSet1_DirShadowSRV, RHIBindingType::TextureSRV, kGL_DirShadowTextureUnit, RHIGraphicsShaderStage::Pixel},
            {kSet1_DirLightViewProjs, RHIBindingType::UniformBuffer, kGL_DirLightViewProjsUBO, RHIGraphicsShaderStage::Pixel},
            {kSet1_CascadeFarPlanes, RHIBindingType::UniformBuffer, kGL_CascadeFarPlanesUBO, RHIGraphicsShaderStage::Pixel},
            {kSet1_SpotLightViewProjs, RHIBindingType::UniformBuffer, kGL_SpotLightViewProjsUBO, RHIGraphicsShaderStage::Pixel},
            {kSet1_SpotShadow0, RHIBindingType::TextureSRV, SPOT_SHADOW_MAP_BASE_UNIT, RHIGraphicsShaderStage::Pixel},
            {kSet1_SpotShadow1, RHIBindingType::TextureSRV, SPOT_SHADOW_MAP_BASE_UNIT + 1, RHIGraphicsShaderStage::Pixel},
            {kSet1_PointShadow0, RHIBindingType::TextureSRV, POINT_SHADOW_MAP_BASE_UNIT, RHIGraphicsShaderStage::Pixel},
            {kSet1_PointShadow1, RHIBindingType::TextureSRV, POINT_SHADOW_MAP_BASE_UNIT + 1, RHIGraphicsShaderStage::Pixel},
            {kSet1_IBLIrradiance, RHIBindingType::TextureSRV, kGL_IBLIrradianceUnit, RHIGraphicsShaderStage::Pixel},
            {kSet1_IBLPrefilter, RHIBindingType::TextureSRV, kGL_IBLPrefilterUnit, RHIGraphicsShaderStage::Pixel},
            {kSet1_IBLBrdfLut, RHIBindingType::TextureSRV, kGL_IBLBrdfLutUnit, RHIGraphicsShaderStage::Pixel},
        });
    }

    void EngineSceneBindingSets::Shutdown()
    {
        m_SceneSet0.reset();
        m_SceneSet1.reset();
        m_SceneSet0Layout.reset();
        m_SceneSet1Layout.reset();
        m_DirShadowSRV.reset();
        m_SpotShadowSRVs = {};
        m_PointShadowSRVs = {};
        m_IblSRVs = {};
    }

    void EngineSceneBindingSets::BuildSceneSet0(
        RHICommandList& cmdList,
        RHIBuffer* perFrame,
        RHIBuffer* lights,
        RHIBuffer* perObject)
    {
        if (!m_SceneSet0Layout)
        {
            return;
        }

        std::vector<RHIBindingResource> resources(3);
        resources[kSet0_PerFrame] = {RHIBindingType::UniformBuffer, perFrame, nullptr};
        resources[kSet0_Lights] = {RHIBindingType::UniformBuffer, lights, nullptr};
        resources[kSet0_PerObject] = {RHIBindingType::UniformBuffer, perObject, nullptr};
        m_SceneSet0 = cmdList.CreateBindingSet(m_SceneSet0Layout.get(), resources);
    }

    void EngineSceneBindingSets::BuildSceneSet1(
        RHICommandList& cmdList,
        const SceneRenderContext& ctx,
        RHIBuffer* dirLightViewProjs,
        RHIBuffer* cascadeFarPlanes,
        RHIBuffer* spotLightViewProjs)
    {
        if (!m_SceneSet1Layout)
        {
            return;
        }

        if (ctx.DirectionalShadowHandle.IsValid())
        {
            m_DirShadowSRV = MakeTextureSRV(ctx.DirectionalShadowHandle.Texture.get());
        }
        else
        {
            m_DirShadowSRV.reset();
        }

        m_SpotShadowSRVs = {};
        for (size_t i = 0; i < ctx.SpotShadowHandles.size() && i < m_SpotShadowSRVs.size(); ++i)
        {
            const ShadowResourceHandle& handle = ctx.SpotShadowHandles[i];
            if (handle.IsValid())
            {
                m_SpotShadowSRVs[i] = MakeTextureSRV(handle.Texture.get());
            }
        }

        m_PointShadowSRVs = {};
        for (size_t i = 0; i < ctx.PointShadowHandles.size() && i < m_PointShadowSRVs.size(); ++i)
        {
            const ShadowResourceHandle& handle = ctx.PointShadowHandles[i];
            if (handle.IsValid())
            {
                m_PointShadowSRVs[i] = MakeTextureSRV(handle.Texture.get());
            }
        }

        // F03-M4 P0: IBL textures disabled; layout slots stay null until EnvMap returns.
        m_IblSRVs = {};

        std::vector<RHIBindingResource> resources(11);
        resources[kSet1_DirShadowSRV] = {RHIBindingType::TextureSRV, nullptr, m_DirShadowSRV.get()};
        resources[kSet1_DirLightViewProjs] = {RHIBindingType::UniformBuffer, dirLightViewProjs, nullptr};
        resources[kSet1_CascadeFarPlanes] = {RHIBindingType::UniformBuffer, cascadeFarPlanes, nullptr};
        resources[kSet1_SpotLightViewProjs] = {RHIBindingType::UniformBuffer, spotLightViewProjs, nullptr};
        resources[kSet1_SpotShadow0] = {RHIBindingType::TextureSRV, nullptr, m_SpotShadowSRVs[0].get()};
        resources[kSet1_SpotShadow1] = {RHIBindingType::TextureSRV, nullptr, m_SpotShadowSRVs[1].get()};
        resources[kSet1_PointShadow0] = {RHIBindingType::TextureSRV, nullptr, m_PointShadowSRVs[0].get()};
        resources[kSet1_PointShadow1] = {RHIBindingType::TextureSRV, nullptr, m_PointShadowSRVs[1].get()};
        resources[kSet1_IBLIrradiance] = {RHIBindingType::TextureSRV, nullptr, m_IblSRVs[0].get()};
        resources[kSet1_IBLPrefilter] = {RHIBindingType::TextureSRV, nullptr, m_IblSRVs[1].get()};
        resources[kSet1_IBLBrdfLut] = {RHIBindingType::TextureSRV, nullptr, m_IblSRVs[2].get()};

        m_SceneSet1 = cmdList.CreateBindingSet(m_SceneSet1Layout.get(), resources);
    }

    void EngineSceneBindingSets::UpdatePerObjectModel(RHIBuffer* perObjectBuffer, const Matrix4& model) const
    {
        if (perObjectBuffer)
        {
            perObjectBuffer->UpdateSubresource(&model, 0, sizeof(Matrix4));
        }
    }
}
