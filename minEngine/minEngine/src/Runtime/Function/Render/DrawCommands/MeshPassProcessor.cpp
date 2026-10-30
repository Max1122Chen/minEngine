#include "MeshPassProcessor.h"
#include "Runtime/Function/Render/MaterialRenderProxy.h"

namespace minEngine
{
    bool MeshPassProcessor::IsDrawable(const MeshBatch& batch)
    {
        return batch.m_VertexBuffer != nullptr && batch.m_VertexInputLayout != nullptr && batch.m_MaterialProxy != nullptr;
    }

    bool MeshPassProcessor::UsesTranslucentPath(const MeshBatch& batch)
    {
        if (batch.m_bForceTranslucentPass)
        {
            return true;
        }

        return batch.m_MaterialProxy != nullptr && batch.m_MaterialProxy->IsTranslucent();
    }

    MeshDrawCommand MeshPassProcessor::MakeDrawCommand(const MeshBatch& batch)
    {
        MeshDrawCommand command;
        command.m_VertexBufferRef = batch.m_VertexBufferRef;
        command.m_IndexBufferRef = batch.m_IndexBufferRef;
        command.m_VertexBuffer = batch.m_VertexBuffer;
        command.m_VertexInputLayout = batch.m_VertexInputLayout;
        command.m_IndexBuffer = batch.m_IndexBuffer;
        command.m_IndexCount = batch.m_IndexCount;
        command.m_VertexCount = batch.m_VertexCount;
        command.m_MaterialProxy = batch.m_MaterialProxy;
        command.m_ModelMatrix = batch.m_LocalToWorld;
        command.m_BoundingBox = batch.m_BoundingBox;
        command.m_CastShadow = batch.m_CastShadow;
        command.m_BonePalette = batch.m_BonePalette;
        command.m_BoneCount = batch.m_BoneCount;
        command.m_bSkinned = batch.m_bSkinned;
        return command;
    }

    OpaqueMeshPassProcessor::OpaqueMeshPassProcessor(std::vector<MeshDrawCommand>& outCommands)
        : m_OutCommands(outCommands)
    {
    }

    void OpaqueMeshPassProcessor::AddMeshBatch(const MeshBatch& batch)
    {
        if (!IsDrawable(batch) || UsesTranslucentPath(batch))
        {
            return;
        }

        m_OutCommands.push_back(MakeDrawCommand(batch));
    }

    TranslucentMeshPassProcessor::TranslucentMeshPassProcessor(std::vector<MeshDrawCommand>& outCommands)
        : m_OutCommands(outCommands)
    {
    }

    void TranslucentMeshPassProcessor::AddMeshBatch(const MeshBatch& batch)
    {
        if (!IsDrawable(batch) || !UsesTranslucentPath(batch))
        {
            return;
        }

        m_OutCommands.push_back(MakeDrawCommand(batch));
    }

    ShadowDepthMeshPassProcessor::ShadowDepthMeshPassProcessor(std::vector<MeshDrawCommand>& outCommands)
        : m_OutCommands(outCommands)
    {
    }

    void ShadowDepthMeshPassProcessor::AddMeshBatch(const MeshBatch& batch)
    {
        if (!IsDrawable(batch) || !batch.m_CastShadow || UsesTranslucentPath(batch))
        {
            return;
        }

        m_OutCommands.push_back(MakeDrawCommand(batch));
    }
}
