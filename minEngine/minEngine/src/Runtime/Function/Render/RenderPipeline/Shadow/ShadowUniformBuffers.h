#pragma once

#include "Render/EngineRHITextureUtils.h"
#include "Runtime/Function/Render/EnginePassUniforms.h"
#include "Runtime/Function/Render/RenderPipeline/Shadow/ShadowTypes.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"

namespace minEngine
{
    struct ShadowPassUniformBinding
    {
        RHIBuffer* ViewProjBuffer = nullptr;
        uint32_t ViewProjByteOffset = 0;
        RHIBuffer* ParamsBuffer = nullptr;
        uint32_t ParamsByteOffset = 0;

        bool IsValid() const { return ViewProjBuffer != nullptr && ParamsBuffer != nullptr; }
    };

    class ShadowUniformBuffers
    {
    public:
        static constexpr uint32_t kPointViewProjRingSlots = kMaxShadowGraphPasses;
        static constexpr uint32_t kParamsRingSlots = kMaxShadowGraphPasses;

        void Initialize(RHICommandList& cmdList, RHI& rhi);
        void Shutdown();
        void BeginShadowFrame();

        RHIBuffer* GetDirLightViewProjBuffer() const { return m_DirLightViewProjBuffer.get(); }
        RHIBuffer* GetSpotLightViewProjBuffer() const { return m_SpotLightViewProjBuffer.get(); }
        RHIBuffer* GetPointLightViewProjRingBuffer() const { return m_PointLightViewProjRingBuffer.get(); }
        RHIBuffer* GetParamsRingBuffer() const { return m_ParamsRingBuffer.get(); }
        RHIBuffer* GetCascadeFarPlaneBuffer() const { return m_CascadeFarPlaneBuffer.get(); }

        uint32_t GetMat4SlotStride() const { return m_Mat4SlotStride; }
        uint32_t GetParamsSlotStride() const { return m_ParamsSlotStride; }
        uint32_t GetCascadeFarPlaneStride() const { return m_CascadeFarPlaneStride; }

        uint32_t GetDirLightViewProjOffset(uint32_t cascadeIndex) const;
        uint32_t GetSpotLightViewProjOffset(uint32_t spotSlot) const;
        uint32_t GetCascadeFarPlaneOffset(uint32_t cascadeIndex) const;

        uint32_t WritePointLightViewProj(const Matrix4& viewProj);
        uint32_t WriteParams(const ShadowPassParamsUBO& params);

    private:
        static uint32_t AlignUniformSize(uint32_t size, uint32_t alignment);

        RHIBufferRef m_DirLightViewProjBuffer;
        RHIBufferRef m_SpotLightViewProjBuffer;
        RHIBufferRef m_PointLightViewProjRingBuffer;
        RHIBufferRef m_ParamsRingBuffer;
        RHIBufferRef m_CascadeFarPlaneBuffer;

        uint32_t m_Mat4SlotStride = 256;
        uint32_t m_ParamsSlotStride = 256;
        uint32_t m_CascadeFarPlaneStride = 16;
        uint32_t m_PointLightViewProjWriteIndex = 0;
        uint32_t m_ParamsWriteIndex = 0;
    };
}
