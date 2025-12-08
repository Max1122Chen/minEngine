#include "../minEngine.h"

class Playground : public minEngine::Application
{
    public:
    Playground() = default;
    ~Playground() = default;

    minEngine::Engine* engine = nullptr;

    virtual void Initialize() override
    {
        engine = new minEngine::Engine();
        engine->Initialize();

    }

    virtual void Shutdown() override
    {
        engine->Shutdown();
        delete engine;
        engine = nullptr;
    }

    virtual void Run() override
    {
        minEngine::WorldManager* worldManager = minEngine::RuntimeGlobalContext::GetInstance().m_WorldManager.get();

        worldManager->m_CurrentActiveLevel = std::make_shared<minEngine::Level>();

        minEngine::Level& level = *worldManager->m_CurrentActiveLevel;
        
        auto cube = level.CreateGameObject();
        std::shared_ptr<minEngine::StaticMeshComponent> cubeMeshComponent = cube->CreateAndAddComponent<minEngine::StaticMeshComponent>();
        cubeMeshComponent->MarkRenderStateDirty();

        auto light = level.CreateGameObject();
        std::shared_ptr<minEngine::StaticMeshComponent> lightMeshComponent = light->CreateAndAddComponent<minEngine::StaticMeshComponent>();
        lightMeshComponent->MarkRenderStateDirty();

        // cube vertex data
        float modelVertices[] = {
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,  0.0f, 0.0f, -1.0f,
        0.5f, -0.5f, -0.5f,  1.0f, 0.0f,  0.0f, 0.0f, -1.0f,
        0.5f,  0.5f, -0.5f,  1.0f, 1.0f,  0.0f, 0.0f, -1.0f,
        0.5f,  0.5f, -0.5f,  1.0f, 1.0f,  0.0f, 0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,  0.0f, 0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,  0.0f, 0.0f, -1.0f,

        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  0.0f, 0.0f, 1.0f,
        0.5f, -0.5f,  0.5f,  1.0f, 0.0f,  0.0f, 0.0f, 1.0f,
        0.5f,  0.5f,  0.5f,  1.0f, 1.0f,  0.0f, 0.0f, 1.0f,
        0.5f,  0.5f,  0.5f,  1.0f, 1.0f,  0.0f, 0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,  0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  0.0f, 0.0f, 1.0f,

        -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,  -1.0f, 0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  1.0f, 1.0f,  -1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  -1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  -1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  -1.0f, 0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,  -1.0f, 0.0f, 0.0f,

        0.5f,  0.5f,  0.5f,  1.0f, 0.0f,  1.0f, 0.0f, 0.0f,
        0.5f,  0.5f, -0.5f,  1.0f, 1.0f,  1.0f, 0.0f, 0.0f,
        0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  1.0f, 0.0f, 0.0f,
        0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  1.0f, 0.0f, 0.0f,
        0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  1.0f, 0.0f, 0.0f,
        0.5f,  0.5f,  0.5f,  1.0f, 0.0f,  1.0f, 0.0f, 0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  0.0f, -1.0f, 0.0f,
        0.5f, -0.5f, -0.5f,  1.0f, 1.0f,  0.0f, -1.0f, 0.0f,
        0.5f, -0.5f,  0.5f,  1.0f, 0.0f,  0.0f, -1.0f, 0.0f,
        0.5f, -0.5f,  0.5f,  1.0f, 0.0f,  0.0f, -1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  0.0f, -1.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  0.0f, -1.0f, 0.0f,

        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,  0.0f,  1.0f,  0.0f,
        0.5f,  0.5f, -0.5f,  1.0f, 1.0f,  0.0f,  1.0f,  0.0f,
        0.5f,  0.5f,  0.5f,  1.0f, 0.0f,  0.0f,  1.0f,  0.0f,
        0.5f,  0.5f,  0.5f,  1.0f, 0.0f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 0.0f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,  0.0f,  1.0f,  0.0f,
        };

        float lightVertices[] = {
        -0.5f, -0.5f, -0.5f, 
         0.5f, -0.5f, -0.5f,  
         0.5f,  0.5f, -0.5f,  
         0.5f,  0.5f, -0.5f,  
        -0.5f,  0.5f, -0.5f, 
        -0.5f, -0.5f, -0.5f, 

        -0.5f, -0.5f,  0.5f, 
         0.5f, -0.5f,  0.5f,  
         0.5f,  0.5f,  0.5f,  
         0.5f,  0.5f,  0.5f,  
        -0.5f,  0.5f,  0.5f, 
        -0.5f, -0.5f,  0.5f, 

        -0.5f,  0.5f,  0.5f, 
        -0.5f,  0.5f, -0.5f, 
        -0.5f, -0.5f, -0.5f, 
        -0.5f, -0.5f, -0.5f, 
        -0.5f, -0.5f,  0.5f, 
        -0.5f,  0.5f,  0.5f, 

         0.5f,  0.5f,  0.5f,  
         0.5f,  0.5f, -0.5f,  
         0.5f, -0.5f, -0.5f,  
         0.5f, -0.5f, -0.5f,  
         0.5f, -0.5f,  0.5f,  
         0.5f,  0.5f,  0.5f,  

        -0.5f, -0.5f, -0.5f, 
         0.5f, -0.5f, -0.5f,  
         0.5f, -0.5f,  0.5f,  
         0.5f, -0.5f,  0.5f,  
        -0.5f, -0.5f,  0.5f, 
        -0.5f, -0.5f, -0.5f, 

        -0.5f,  0.5f, -0.5f, 
         0.5f,  0.5f, -0.5f,  
         0.5f,  0.5f,  0.5f,  
         0.5f,  0.5f,  0.5f,  
        -0.5f,  0.5f,  0.5f, 
        -0.5f,  0.5f, -0.5f, 
        };

        // create cube
        minEngine::StaticMesh cubeMesh(modelVertices, sizeof(modelVertices), {
            minEngine::VertexElement("a_Position", minEngine::VertexElementType::Float3),
            minEngine::VertexElement("a_TexCoord", minEngine::VertexElementType::Float2),
            minEngine::VertexElement("a_Normal", minEngine::VertexElementType::Float3)
        });

        // create light
        minEngine::StaticMesh lightMesh(lightVertices, sizeof(lightVertices), {
            minEngine::VertexElement("a_Position", minEngine::VertexElementType::Float3)
        });

        minEngine::Material cubeMaterial;
        cubeMaterial.m_Shader = std::make_shared<minEngine::OpenGLShader>("D:/Dev/GitRepo/minEngine/minEngine/Shaders/Phong.vert", "D:/Dev/GitRepo/minEngine/minEngine/Shaders/Phong.frag");
        cubeMaterial.m_Albedo.Texture = std::make_shared<minEngine::OpenGLTexture>("D:/Dev/GitRepo/minEngine/minEngine/Assets/Textures/container.jpg", 0);

        // create light material
        minEngine::Material lightMaterial;
        lightMaterial.m_Shader = std::make_shared<minEngine::OpenGLShader>("D:/Dev/GitRepo/minEngine/minEngine/Shaders/Light.vert", "D:/Dev/GitRepo/minEngine/minEngine/Shaders/Light.frag");
        lightMaterial.m_Albedo.Value = minEngine::Vector4(1.0f, 1.0f, 1.0f, 1.0f);

        cubeMeshComponent->SetMesh(std::make_shared<minEngine::StaticMesh>(cubeMesh));
        cubeMeshComponent->SetMaterial(std::make_shared<minEngine::Material>(cubeMaterial));

        // set light mesh and material
        lightMeshComponent->SetMesh(std::make_shared<minEngine::StaticMesh>(lightMesh));
        lightMeshComponent->SetMaterial(std::make_shared<minEngine::Material>(lightMaterial));

        // set cube transforms
        cube->SetPosition(minEngine::Vector3(0.0f, 0.0f, 0.0f));
        cube->SetScale(minEngine::Vector3(2.0f, 2.0f, 2.0f));
        cube->SetRotation(minEngine::Vector3(45.0f, 0.0f, 0.0f));

        // set light transforms
        light->SetPosition(minEngine::Vector3(0.0f, 0.0f, 0.0f));
        light->SetScale(minEngine::Vector3(0.2f, 0.2f, 0.2f));
        light->SetRotation(minEngine::Vector3(0.0f, 0.0f, 0.0f));

        engine->Run();
    }
};

minEngine::Application* minEngine::CreateApplication()
{
    return new Playground();
}