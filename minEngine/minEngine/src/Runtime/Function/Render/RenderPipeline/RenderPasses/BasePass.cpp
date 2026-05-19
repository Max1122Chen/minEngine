#include "BasePass.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RenderScene.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/RenderCamera.h"
#include "Runtime/Function/Render/OpenGL/OpenGLRHI.h"
#include "Runtime/Function/Render/OpenGL/OpenGLVertexArrayObject.h"
#include "Runtime/Function/Render/OpenGL/OpenGLBuffers.h"
#include "Runtime/Function/Render/PrimitiveSceneProxies/StaticMeshSceneProxy.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"
#include "Runtime/Function/Render/LightSceneProxies/PointLightSceneProxy.h"
#include "Runtime/Function/Render/LightSceneProxies/DirectionalLightSceneProxy.h"
#include "Runtime/Function/Render/LightSceneProxies/SpotLightSceneProxy.h"
#include "Render/RHI/RHITexture.h"
#include "Render/Shader.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <string>

namespace minEngine
{
    namespace
    {
        void DrawMeshCommand(const MeshDrawCommand& drawCommand)
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

        void BindLegacyPhongMaterial(
            Material& material,
            RHIShader& shader,
            const MeshDrawCommand& drawCommand,
            const ShadowResourceHandle& directionalShadowHandle,
            const std::vector<ShadowResourceHandle>& spotShadowHandles,
            const std::vector<ShadowResourceHandle>& pointShadowHandles)
        {
            material.BindTextures();
            shader.UploadUniformInt("u_Material.DiffuseMap", 0);

            shader.BindUniformBlock("PerFrameData", 0);
            shader.BindUniformBlock("LightsData", 1);

            if (directionalShadowHandle.IsValid())
            {
                directionalShadowHandle.GetAs2DArray()->Bind(8);
                shader.UploadUniformInt("u_DirLightShadowMap", 8);
                shader.BindUniformBlock("DirLightViewProjs", 9);
                shader.BindUniformBlock("CascadeFarPlanes", 10);
            }

            if (!spotShadowHandles.empty())
            {
                shader.BindUniformBlock("SpotLightViewProjs", 11);
                for (const ShadowResourceHandle& handle : spotShadowHandles)
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

            if (!pointShadowHandles.empty())
            {
                for (const ShadowResourceHandle& handle : pointShadowHandles)
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

            shader.UploadUniformMat4("u_Model", drawCommand.m_ModelMatrix);
        }

        void BindCompiledGraphMaterial(Material& material, RHIShader& shader, const MeshDrawCommand& drawCommand)
        {
            material.BindCompiledGraph(shader);
            shader.BindUniformBlock("PerFrameData", 0);
            shader.UploadUniformMat4("u_Model", drawCommand.m_ModelMatrix);
        }
    }

    void BasePass::Execute()
    {
        Render();
    }

    void BasePass::Render()
    {
        RHI* rhi = RenderSystem::Get().GetRHI();
        if (!rhi)
        {
            return;
        }
        rhi->EnableDepthTest();

        for (MeshDrawCommand& drawCommand : m_DrawCommands)
        {
            Material* material = drawCommand.m_Material;
            if (!material || !drawCommand.m_VertexDefinition || !drawCommand.m_VertexBuffer)
            {
                continue;
            }

            const std::shared_ptr<Shader>& shaderAsset = material->m_Shader;
            if (!shaderAsset || !shaderAsset->GetRHIShader())
            {
                continue;
            }

            RHIShader* shader = shaderAsset->GetRHIShader().get();
            shader->Use();

            if (material->UsesCompiledGraphMaterial())
            {
                BindCompiledGraphMaterial(*material, *shader, drawCommand);
            }
            else
            {
                BindLegacyPhongMaterial(
                    *material,
                    *shader,
                    drawCommand,
                    m_DirectionalShadowHandle,
                    m_SpotShadowHandles,
                    m_PointShadowHandles);
            }

            DrawMeshCommand(drawCommand);
        }
    }
}
