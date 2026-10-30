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

        /**
         * The binding sets this draw names, owned for as long as the packet exists.
         *
         * A packet is built in a pass preparation step and submitted later, and a recorded stream
         * that names a set can be replayed later still. In that gap the producer of a set may
         * rebuild it - a material whose compile generation changed drops the set it built before -
         * so a packet that only borrowed the address would hand a destroyed object to the device.
         * Holding the reference is what makes the gap harmless: the old set stays alive until the
         * last consumer of it is gone, and the draw it was recorded for reads the bindings that
         * were current when the packet was filled.
         */
        std::array<RHIShaderBindingSetRef, kMaxShaderBindingSets> ShaderBindingSets{};

        /**
         * The geometry this packet names, owned for as long as the packet exists.
         *
         * Same gap as the sets above, one layer lower: a packet is prepared in one step and
         * submitted in another, and a recorded stream that names a buffer can be replayed later
         * still. A packet that only borrowed the address let the producer drop the last handle in
         * between - the frame-start creation then decided nobody wanted the native object, and the
         * submission either named nothing or dereferenced what was already gone. Holding the
         * handles is what makes the packet the owner of the geometry it draws.
         */
        RHIBufferRef VertexBufferRef;
        RHIBufferRef IndexBufferRef;

        /** Frame views of the handles above; the packet owns them through the refs. */
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
            VertexBufferRef = command.m_VertexBufferRef;
            IndexBufferRef = command.m_IndexBufferRef;
            // A command without handles still names what it was built from; the packet then only
            // borrows that, which is the state the readiness gate at submission has to cover.
            VertexBuffer = VertexBufferRef ? VertexBufferRef.get() : command.m_VertexBuffer;
            IndexBuffer = IndexBufferRef ? IndexBufferRef.get() : command.m_IndexBuffer;
            IndexCount = command.m_IndexCount;
            VertexCount = command.m_VertexCount;
        }
    };
}
