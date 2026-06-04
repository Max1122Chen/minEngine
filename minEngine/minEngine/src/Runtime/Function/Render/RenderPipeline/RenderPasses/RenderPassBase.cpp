#include "RenderPassBase.h"

#include "Runtime/Function/Render/Environment/EngineIBLEnvironment.h"
#include "Runtime/Function/Render/OpenGL/OpenGLBuffers.h"
#include "Runtime/Function/Render/OpenGL/OpenGLVertexArrayObject.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"

#include <glad/glad.h>
#include <string>

namespace minEngine
{
    void RenderPassBase::DrawMeshCommand(const MeshDrawCommand& drawCommand)
    {
        static_cast<OpenGLVertexArrayObject*>(drawCommand.m_VertexDefinition)->Bind();

        if (drawCommand.m_IndexBuffer)
        {
            static_cast<OpenGLIndexBuffer*>(drawCommand.m_IndexBuffer)->Bind();
            glDrawElements(GL_TRIANGLES, drawCommand.m_IndexBuffer->GetNumIndices(), GL_UNSIGNED_INT, nullptr);
            static_cast<OpenGLIndexBuffer*>(drawCommand.m_IndexBuffer)->Unbind();
        }
        else
        {
            glDrawArrays(GL_TRIANGLES, 0, drawCommand.m_VertexBuffer->GetNumVertices());
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
