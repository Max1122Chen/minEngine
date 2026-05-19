#include "EditorDefaultScene.h"

#include "Runtime/Function/Framework/Transform/Transform.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Components/CameraComponent.h"
#include "Runtime/Function/Framework/Components/DirectionalLightComponent.h"
#include "Runtime/Function/Framework/Components/StaticMeshComponent.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Render/Material/MaterialSmokeGraph.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/Shader.h"
#include "Runtime/Function/Render/StaticMesh.h"
#include "Runtime/Function/Render/Texture.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Core/Math/Geometry/AABB.h"

#include <array>
#include <memory>
#include <string>

namespace minEngine
{
    namespace
    {
        void SyncSceneToRenderPipeline(Scene& scene)
        {
            SceneManager& sceneManager = SceneManager::Get();
            for (const std::shared_ptr<GameObject>& gameObject : scene.GetAllGameObjects())
            {
                if (!gameObject)
                {
                    continue;
                }

                for (const std::shared_ptr<Component>& component : gameObject->GetAllComponents())
                {
                    if (!component)
                    {
                        continue;
                    }

                    sceneManager.MarkComponentForNeededEndOfFrameUpdate(component.get());
                    if (SceneComponent* sceneComponent = dynamic_cast<SceneComponent*>(component.get()))
                    {
                        sceneComponent->MarkRenderStateDirty();
                    }
                }
            }
        }

        std::shared_ptr<StaticMesh> CreateTexturedGroundQuadMesh()
        {
            // layout: position (3) + texcoord (2) + normal (3), same as Assimp-imported meshes
            constexpr float kHalfExtent = 1.5f;
            const float vertices[] = {
                -kHalfExtent, 0.0f, -kHalfExtent,   0.0f, 0.0f,   0.0f, 1.0f, 0.0f,
                 kHalfExtent, 0.0f, -kHalfExtent,   1.0f, 0.0f,   0.0f, 1.0f, 0.0f,
                 kHalfExtent, 0.0f,  kHalfExtent,   1.0f, 1.0f,   0.0f, 1.0f, 0.0f,
                -kHalfExtent, 0.0f,  kHalfExtent,   0.0f, 1.0f,   0.0f, 1.0f, 0.0f,
            };
            const uint32_t indices[] = { 0, 1, 2, 0, 2, 3 };

            std::shared_ptr<StaticMesh> mesh(new StaticMesh(
                const_cast<float*>(vertices),
                static_cast<uint32_t>(sizeof(vertices)),
                4,
                {
                    VertexElement("a_Position", VertexElementType::Float3),
                    VertexElement("a_TexCoord", VertexElementType::Float2),
                    VertexElement("a_Normal", VertexElementType::Float3),
                },
                const_cast<uint32_t*>(indices),
                6));

            // Local-space AABB for shadow cascades / culling (default StaticMesh AABB is invalid).
            mesh->m_BoundingBox = Math::Geometry::AABB(
                Vector3(-kHalfExtent, 0.0f, -kHalfExtent),
                Vector3(kHalfExtent, 0.0f, kHalfExtent));
            return mesh;
        }

        std::shared_ptr<Material> CreateMaterialIRSmokeMaterial(RHI& rhi, std::string& outError)
        {
            const MaterialSmokeGraph smokeGraph;
            const MaterialCompiledShader compiled = smokeGraph.CompileUnlit();
            if (!compiled.Succeeded)
            {
                outError = "Material IR smoke graph compile failed.";
                for (const MaterialCompileDiagnostic& diagnostic : compiled.Diagnostics)
                {
                    outError += "\n";
                    outError += diagnostic.Message;
                }
                return nullptr;
            }

            std::shared_ptr<Shader> shader = Shader::CreateFromSource(
                rhi,
                compiled.FullVertexShader,
                compiled.FullFragmentShader,
                &outError);
            if (!shader)
            {
                return nullptr;
            }

            std::shared_ptr<Material> material = std::make_shared<Material>();
            material->m_Shader = shader;
            material->m_bUsesCompiledGraph = true;
            std::shared_ptr<Texture2D> whiteTexture = Texture2D::CreateSolidRGBA(rhi, 255, 255, 255, 255);
            material->m_GraphTextureSlots.push_back(whiteTexture);
            material->m_GraphScalarParams.push_back(0.3f);
            return material;
        }
    }

    void PopulateEditorDefaultScene(Scene& scene)
    {
        RHI* rhi = RenderSystem::Get().GetRHI();
        if (rhi == nullptr)
        {
            ME_CORE_ERROR("PopulateEditorDefaultScene: RenderSystem RHI is not available.");
            return;
        }

        std::string materialError;
        std::shared_ptr<Material> smokeMaterial = CreateMaterialIRSmokeMaterial(*rhi, materialError);
        if (!smokeMaterial)
        {
            ME_CORE_ERROR("PopulateEditorDefaultScene: failed to build material IR smoke material.\n{}", materialError);
            return;
        }

        std::shared_ptr<StaticMesh> groundMesh = CreateTexturedGroundQuadMesh();

        {
            std::shared_ptr<GameObject> meshObject = scene.CreateGameObject();
            std::shared_ptr<Component> meshComponentBase = meshObject->AddComponent<StaticMeshComponent>();
            StaticMeshComponent* meshComponent = static_cast<StaticMeshComponent*>(meshComponentBase.get());
            meshObject->SetRootComponent(meshComponent);
            meshComponent->SetMesh(groundMesh);
            meshComponent->SetMaterial(smokeMaterial);
        }

        {
            std::shared_ptr<GameObject> cameraObject = scene.CreateGameObject();
            std::shared_ptr<Component> cameraComponentBase = cameraObject->AddComponent<CameraComponent>();
            CameraComponent* cameraComponent = static_cast<CameraComponent*>(cameraComponentBase.get());
            cameraObject->SetRootComponent(cameraComponent);
            cameraObject->SetPosition(Vector3(0.0f, 2.5f, 5.0f));
            cameraComponent->SetSelfAsMainCamera();
        }

        {
            std::shared_ptr<GameObject> lightObject = scene.CreateGameObject();
            std::shared_ptr<Component> lightComponentBase = lightObject->AddComponent<DirectionalLightComponent>();
            DirectionalLightComponent* lightComponent = static_cast<DirectionalLightComponent*>(lightComponentBase.get());
            lightObject->SetRootComponent(lightComponent);
            lightObject->SetRotation(Vector3(-45.0f, 45.0f, 0.0f));
        }

        ME_CORE_INFO(
            "Editor material IR smoke scene populated (quad + camera + light).\n"
            "Expected: Albedo = whiteTexture * tint -> rgb ~ (0.2, 0.8, 0.2).");
    }

    bool SetEditorMaterialIRSmokeActiveScene()
    {
        SceneManager& sceneManager = SceneManager::Get();
        std::shared_ptr<Scene> scene = sceneManager.CreateNewScene("MaterialIRSmoke");
        if (!scene)
        {
            ME_CORE_ERROR("SetEditorMaterialIRSmokeActiveScene: CreateNewScene failed.");
            return false;
        }

        PopulateEditorDefaultScene(*scene);
        SyncSceneToRenderPipeline(*scene);
        ME_CORE_INFO("Active scene set to '{}'.", scene->GetSceneName());
        return true;
    }
}
