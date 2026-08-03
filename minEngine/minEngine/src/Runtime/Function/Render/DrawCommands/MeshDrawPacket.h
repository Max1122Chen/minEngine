#pragma once

#include "Core.h"
#include "Render/RHI/RHIShaderBinding.h"
#include "Render/RHI/RHIBuffers.h"
#include "Render/RHI/RHIGraphicsPipelineState.h"
#include "Render/RHI/RHIPipelineLayout.h"

#include <array>

namespace minEngine
{
    /** Complete GPU state for a single draw submission (RND-F04). */
    struct MeshDrawPacket
    {
        RHIGraphicsPipelineStateRef PipelineState;

        std::array<RHIShaderBindingSet*, kMaxShaderBindingSets> ShaderBindingSets{};

        RHIBuffer* VertexBuffer = nullptr;
        RHIBuffer* IndexBuffer = nullptr;

        uint32_t IndexCount = 0;
        uint32_t FirstIndex = 0;
        int32_t VertexOffset = 0;
        uint32_t VertexCount = 0;
        uint32_t FirstVertex = 0;
    };
}
