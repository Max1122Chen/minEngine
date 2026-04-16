#include "Runtime/Function/Framework/Transform/Transform.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Components/DirectionalLightComponent.h"
#include "Runtime/Function/Framework/Components/StaticMeshComponent.h"
#include "Runtime/Function/Render/OpenGL/OpenGLShader.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/StaticMesh.h"
#include "Runtime/Function/Render/Texture.h"
#include "Runtime/Resource/AssetManager.h"

#include <array>
#include <filesystem>
#include <initializer_list>

namespace
{
    std::filesystem::path ResolveFirstExistingPath(const std::initializer_list<std::string>& candidates)
    {
        for (const std::string& candidate : candidates)
        {
            const std::filesystem::path absolutePath = std::filesystem::absolute(candidate).lexically_normal();
            if (std::filesystem::exists(absolutePath))
            {
                return absolutePath;
            }
        }

        return std::filesystem::path();
    }

    const minEngine::AssetMeta* FindAssetMetaByRelativePath(minEngine::AssetManager& assetManager,
                                                             const std::string& relativePath)
    {
        const std::array<std::string, 4> candidates = {
            relativePath,
            "../" + relativePath,
            "../../" + relativePath,
            "minEngine/" + relativePath
        };

        for (const std::string& candidate : candidates)
        {
            const std::filesystem::path absolutePath = std::filesystem::absolute(candidate).lexically_normal();
            if (!std::filesystem::exists(absolutePath))
            {
                continue;
            }

            const minEngine::AssetMeta* meta = assetManager.FindAssetMetaByPath(absolutePath.generic_string());
            if (meta != nullptr)
            {
                return meta;
            }
        }

        return nullptr;
    }

    std::shared_ptr<minEngine::Material> BuildDefaultMaterial(const std::shared_ptr<minEngine::Texture2D>& diffuseTexture)
    {
        auto material = std::make_shared<minEngine::Material>();

        const std::filesystem::path vertShaderPath = ResolveFirstExistingPath({
            "Shaders/Phong.vert",
            "../Shaders/Phong.vert",
            "../../Shaders/Phong.vert",
            "minEngine/Shaders/Phong.vert"
        });
        const std::filesystem::path fragShaderPath = ResolveFirstExistingPath({
            "Shaders/Phong.frag",
            "../Shaders/Phong.frag",
            "../../Shaders/Phong.frag",
            "minEngine/Shaders/Phong.frag"
        });

        if (!vertShaderPath.empty() && !fragShaderPath.empty())
        {
            material->m_Shader = std::make_shared<minEngine::OpenGLShader>(
                vertShaderPath.string().c_str(),
                fragShaderPath.string().c_str());
        }
        else
        {
            ME_CORE_WARN("Default scene material shader path was not found. Meshes may not render.");
        }

        material->m_Diffuse.Value = minEngine::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        material->m_Diffuse.Texture = diffuseTexture;
        material->m_Specular.Value = minEngine::Vector4(0.2f, 0.2f, 0.2f, 1.0f);

        return material;
    }

    std::shared_ptr<minEngine::StaticMesh> LoadMeshByRelativePath(minEngine::AssetManager& assetManager,
                                                                   const std::string& relativePath,
                                                                   const std::string& debugName)
    {
        const minEngine::AssetMeta* meshMeta = FindAssetMetaByRelativePath(assetManager, relativePath);
        if (meshMeta == nullptr)
        {
            ME_CORE_WARN("Default scene mesh meta not found: {}", relativePath);
            return nullptr;
        }

        std::shared_ptr<minEngine::StaticMesh> mesh = assetManager.LoadStaticMeshByMeta(*meshMeta);
        if (mesh == nullptr)
        {
            ME_CORE_WARN("Failed to load default scene mesh '{}' from meta: {}", debugName, meshMeta->AssetPath);
            return nullptr;
        }

        ME_CORE_INFO("Loaded default scene mesh '{}' from meta: {}", debugName, meshMeta->AssetPath);
        return mesh;
    }

    void CreateRenderableObject(minEngine::Scene& scene,
                                const std::string& name,
                                const minEngine::Transform& transform,
                                const std::shared_ptr<minEngine::StaticMesh>& mesh,
                                const std::shared_ptr<minEngine::Material>& material)
    {
        if (mesh == nullptr)
        {
            ME_CORE_WARN("Skip creating default scene object '{}' because mesh is null.", name);
            return;
        }

        std::shared_ptr<minEngine::GameObject> gameObject = scene.CreateGameObject();
        gameObject->SetName(name);

        std::shared_ptr<minEngine::StaticMeshComponent> meshComponent = gameObject->AddComponent<minEngine::StaticMeshComponent>();
        meshComponent->SetOwner(gameObject.get());
        gameObject->SetRootComponent(meshComponent);

        meshComponent->SetMesh(mesh);
        meshComponent->SetMaterial(material);
        gameObject->SetTransform(transform);
    }

    void CreateDirectionalLightObject(minEngine::Scene& scene)
    {
        std::shared_ptr<minEngine::GameObject> lightObject = scene.CreateGameObject();
        lightObject->SetName("DefaultDirectionalLight");

        std::shared_ptr<minEngine::DirectionalLightComponent> lightComponent = lightObject->AddComponent<minEngine::DirectionalLightComponent>();
        lightComponent->SetOwner(lightObject.get());
        lightObject->SetRootComponent(lightComponent);

        lightComponent->SetDirection(minEngine::Vector3(-0.35f, -1.0f, -0.45f));
        lightComponent->SetLightColor(minEngine::Vector4(1.0f, 0.95f, 0.88f, 1.0f));
        lightComponent->SetIntensity(1.2f);
        lightComponent->SetCastShadow(true);
    }
}

namespace minEngine
{
    void PopulateEditorDefaultScene(Scene& scene)
    {
        AssetManager& assetManager = AssetManager::Get();

        const AssetMeta* awesomefaceMeta = FindAssetMetaByRelativePath(assetManager, "Assets/EngineDefault/Textures/awesomeface.png");
        std::shared_ptr<Texture2D> diffuseTexture = nullptr;
        if (awesomefaceMeta != nullptr)
        {
            diffuseTexture = assetManager.LoadTexture2DByMeta(*awesomefaceMeta, 0);
            if (diffuseTexture)
            {
                ME_CORE_INFO("Loaded default scene texture from meta: {}", awesomefaceMeta->AssetPath);
            }
            else
            {
                ME_CORE_WARN("Failed to load default scene texture by meta: {}", awesomefaceMeta->AssetPath);
            }
        }
        else
        {
            ME_CORE_WARN("Default scene texture meta not found in AssetManager registry: Assets/EngineDefault/Textures/awesomeface.png");
        }

        const std::shared_ptr<Material> sharedMaterial = BuildDefaultMaterial(diffuseTexture);
        const std::shared_ptr<StaticMesh> cubeMesh = LoadMeshByRelativePath(assetManager, "Assets/EngineDefault/Meshes/BasicShapes/cube.obj", "cube");
        const std::shared_ptr<StaticMesh> sphereMesh = LoadMeshByRelativePath(assetManager, "Assets/EngineDefault/Meshes/BasicShapes/sphere.obj", "sphere");
        const std::shared_ptr<StaticMesh> cylinderMesh = LoadMeshByRelativePath(assetManager, "Assets/EngineDefault/Meshes/BasicShapes/cylinder.obj", "cylinder");
        const std::shared_ptr<StaticMesh> coneMesh = LoadMeshByRelativePath(assetManager, "Assets/EngineDefault/Meshes/BasicShapes/cone.obj", "cone");

        CreateRenderableObject(
            scene,
            "DefaultGround",
            Transform(Vector3(0.0f, -0.5f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(8.0f, 0.2f, 8.0f)),
            cubeMesh,
            sharedMaterial);

        CreateRenderableObject(
            scene,
            "DefaultCube",
            Transform(Vector3(-2.0f, 0.5f, 0.0f), Vector3(0.0f, 25.0f, 0.0f), Vector3(1.0f, 1.0f, 1.0f)),
            cubeMesh,
            sharedMaterial);

        CreateRenderableObject(
            scene,
            "DefaultSphere",
            Transform(Vector3(0.5f, 0.6f, -1.5f), Vector3(0.0f, 10.0f, 0.0f), Vector3(1.0f, 1.0f, 1.0f)),
            sphereMesh,
            sharedMaterial);

        CreateRenderableObject(
            scene,
            "DefaultCylinder",
            Transform(Vector3(2.5f, 0.8f, 0.5f), Vector3(0.0f, -20.0f, 0.0f), Vector3(0.8f, 1.4f, 0.8f)),
            cylinderMesh,
            sharedMaterial);

        CreateRenderableObject(
            scene,
            "DefaultCone",
            Transform(Vector3(0.0f, 0.75f, 2.0f), Vector3(0.0f, 35.0f, 0.0f), Vector3(1.0f, 1.5f, 1.0f)),
            coneMesh,
            sharedMaterial);

        CreateDirectionalLightObject(scene);
    }
}