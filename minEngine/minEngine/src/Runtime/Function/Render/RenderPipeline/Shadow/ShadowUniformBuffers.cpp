#include "ShadowUniformBuffers.h"

#include "Runtime/Core/Log/LogSystem.h"

namespace minEngine
{
    uint32_t ShadowUniformBuffers::AlignUniformSize(uint32_t size, uint32_t alignment)
    {
        if (alignment == 0)
        {
            return size;
        }
        return ((size + alignment - 1u) / alignment) * alignment;
    }

    void ShadowUniformBuffers::Initialize(RHICommandList& cmdList, RHI& rhi)
    {
        const uint32_t uboAlignment = rhi.RHIGetMinUniformBufferOffsetAlignment();
        m_Mat4SlotStride = AlignUniformSize(static_cast<uint32_t>(sizeof(Matrix4)), uboAlignment);
        m_ParamsSlotStride = AlignUniformSize(static_cast<uint32_t>(sizeof(ShadowPassParamsUBO)), uboAlignment);
        m_CascadeFarPlaneStride = AlignUniformSize(static_cast<uint32_t>(sizeof(Vector4)), uboAlignment);

        m_DirLightViewProjBuffer = cmdList.CreateBuffer(MakeUniformBufferDesc(m_Mat4SlotStride * MAX_CASCADES));
        m_SpotLightViewProjBuffer =
            cmdList.CreateBuffer(MakeUniformBufferDesc(m_Mat4SlotStride * MAX_SPOT_LIGHTS));
        m_PointLightViewProjRingBuffer =
            cmdList.CreateBuffer(MakeUniformBufferDesc(m_Mat4SlotStride * kPointViewProjRingSlots));
        m_ParamsRingBuffer = cmdList.CreateBuffer(MakeUniformBufferDesc(m_ParamsSlotStride * kParamsRingSlots));
        m_CascadeFarPlaneBuffer = cmdList.CreateBuffer(MakeUniformBufferDesc(m_CascadeFarPlaneStride * MAX_CASCADES));

        BeginShadowFrame();
    }

    void ShadowUniformBuffers::Shutdown()
    {
        m_DirLightViewProjBuffer.reset();
        m_SpotLightViewProjBuffer.reset();
        m_PointLightViewProjRingBuffer.reset();
        m_ParamsRingBuffer.reset();
        m_CascadeFarPlaneBuffer.reset();
        BeginShadowFrame();
    }

    void ShadowUniformBuffers::BeginShadowFrame()
    {
        m_PointLightViewProjWriteIndex = 0;
        m_ParamsWriteIndex = 0;
    }

    uint32_t ShadowUniformBuffers::GetDirLightViewProjOffset(uint32_t cascadeIndex) const
    {
        return cascadeIndex * m_Mat4SlotStride;
    }

    uint32_t ShadowUniformBuffers::GetSpotLightViewProjOffset(uint32_t spotSlot) const
    {
        return spotSlot * m_Mat4SlotStride;
    }

    uint32_t ShadowUniformBuffers::GetCascadeFarPlaneOffset(uint32_t cascadeIndex) const
    {
        return cascadeIndex * m_CascadeFarPlaneStride;
    }

    uint32_t ShadowUniformBuffers::WritePointLightViewProj(const Matrix4& viewProj)
    {
        if (m_PointLightViewProjWriteIndex >= kPointViewProjRingSlots)
        {
            ME_CORE_ERROR(
                "ShadowUniformBuffers: point ViewProj ring exhausted ({} slots). "
                "Wrapping — later draws may read stale matrices.",
                kPointViewProjRingSlots);
            m_PointLightViewProjWriteIndex = 0;
        }

        const uint32_t slot = m_PointLightViewProjWriteIndex % kPointViewProjRingSlots;
        const uint32_t offset = slot * m_Mat4SlotStride;
        if (m_PointLightViewProjRingBuffer)
        {
            m_PointLightViewProjRingBuffer->UpdateSubresource(&viewProj, offset, sizeof(Matrix4));
        }
        ++m_PointLightViewProjWriteIndex;
        return offset;
    }

    uint32_t ShadowUniformBuffers::WriteParams(const ShadowPassParamsUBO& params)
    {
        if (m_ParamsWriteIndex >= kParamsRingSlots)
        {
            ME_CORE_ERROR(
                "ShadowUniformBuffers: params ring exhausted ({} slots). "
                "Wrapping — later draws may read stale shadow params.",
                kParamsRingSlots);
            m_ParamsWriteIndex = 0;
        }

        const uint32_t slot = m_ParamsWriteIndex % kParamsRingSlots;
        const uint32_t offset = slot * m_ParamsSlotStride;
        if (m_ParamsRingBuffer)
        {
            m_ParamsRingBuffer->UpdateSubresource(&params, offset, sizeof(ShadowPassParamsUBO));
        }
        ++m_ParamsWriteIndex;
        return offset;
    }
}
