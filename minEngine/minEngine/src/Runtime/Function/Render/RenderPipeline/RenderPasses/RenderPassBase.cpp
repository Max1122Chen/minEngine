#include "RenderPassBase.h"

#include "Runtime/Function/Render/Environment/EngineIBLEnvironment.h"
#include "Runtime/Function/Render/OpenGL/OpenGLRHIModern.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"

#include <string>

namespace minEngine
{
    void RenderPassBase::DrawMeshCommand(RHICommandList& cmdList, const MeshDrawCommand& drawCommand)
    {
        auto layout = OpenGLRHIVertexInputLayout::WrapLegacyVertexDefinition(drawCommand.m_VertexDefinition);
        auto vertexBuffer = OpenGLRHIBuffer::WrapLegacyVertexBuffer(drawCommand.m_VertexBuffer);

        if (layout)
        {
            cmdList.SetVertexInputLayout(layout.get());
        }
        if (vertexBuffer)
        {
            cmdList.SetVertexBuffer(vertexBuffer.get());
        }

        if (drawCommand.m_IndexBuffer)
        {
            auto indexBuffer = OpenGLRHIBuffer::WrapLegacyIndexBuffer(drawCommand.m_IndexBuffer);
            if (indexBuffer)
            {
                cmdList.SetIndexBuffer(indexBuffer.get());
                cmdList.DrawIndexed(drawCommand.m_IndexBuffer->GetNumIndices(), 0, 0);
            }
        }
        else if (drawCommand.m_VertexBuffer)
        {
            cmdList.Draw(drawCommand.m_VertexBuffer->GetNumVertices(), 0);
        }
    }

    void RenderPassBase::BindSceneDrawResources(RHIShaderLegacy& shader, const MeshPassSceneBinding& binding)
    {
        shader.BindUniformBlock("PerFrameData", 0);
        shader.UploadUniformMat4("u_Model", binding.DrawCommand.m_ModelMatrix);

        if (!binding.bBindLighting)
        {
            return;
        }

        shader.BindUniformBlock("LightsData", 1);

        if (binding.DirectionalShadowHandle != nullptr && binding.DirectionalShadowHandle->IsValid())
        {
            binding.DirectionalShadowHandle->GetAs2DArray()->Bind(8);
            shader.UploadUniformInt("u_DirLightShadowMap", 8);
            shader.BindUniformBlock("DirLightViewProjs", 9);
            shader.BindUniformBlock("CascadeFarPlanes", 10);
        }

        if (binding.SpotShadowHandles != nullptr && !binding.SpotShadowHandles->empty())
        {
            shader.BindUniformBlock("SpotLightViewProjs", 11);
            for (const ShadowResourceHandle& handle : *binding.SpotShadowHandles)
            {
                if (!handle.IsValid())
                {
                    continue;
                }

                RHITexture2D* texture = handle.GetAs2D().get();
                if (!texture)
                {
                    continue;
                }

                const int slot = handle.TextureUnit - SPOT_SHADOW_MAP_BASE_UNIT;
                if (slot < 0 || slot >= MAX_SPOT_SHADOW_MAPS)
                {
                    continue;
                }

                texture->Bind(handle.TextureUnit);
                shader.UploadUniformInt("u_SpotShadowMaps[" + std::to_string(slot) + "]", handle.TextureUnit);
            }
        }

        if (binding.PointShadowHandles != nullptr && !binding.PointShadowHandles->empty())
        {
            for (const ShadowResourceHandle& handle : *binding.PointShadowHandles)
            {
                if (!handle.IsValid())
                {
                    continue;
                }

                RHITextureCube* texture = handle.GetAsCube().get();
                if (!texture)
                {
                    continue;
                }

                const int slot = handle.TextureUnit - POINT_SHADOW_MAP_BASE_UNIT;
                if (slot < 0 || slot >= MAX_POINT_SHADOW_MAPS)
                {
                    continue;
                }

                texture->Bind(handle.TextureUnit);
                shader.UploadUniformInt("u_PointShadowMaps[" + std::to_string(slot) + "]", handle.TextureUnit);
            }
        }

        // IBL cubemap/BRDF samplers are bound after Material::BindForDraw (see BasePass / TranslucencyPass).
    }
}
