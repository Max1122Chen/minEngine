#pragma once

#include "Core.h"
#include "Render/RHI/RHIShaderBinding.h"
#include "Render/RHI/RHIBuffers.h"
#include "Render/RHI/RHIGraphicsPipelineState.h"
#include "Render/RHI/RHIPipelineLayout.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"

#include <array>

namespace minEngine
{
    /** Complete GPU state for a single draw submission. */
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

        /**
         * Takes the geometry and the draw ranges from a pass draw command.
         * One conversion for every packet producer, so all of them carry the range the mesh
         * metadata recorded instead of leaving submission to guess.
         */
        void BindDrawCommand(const MeshDrawCommand& command)
        {
            VertexBuffer = command.m_VertexBuffer;
            IndexBuffer = command.m_IndexBuffer;
            IndexCount = command.m_IndexCount;
            VertexCount = command.m_VertexCount;
        }
    };
}
