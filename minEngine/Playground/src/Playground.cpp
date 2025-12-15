#include "minEngine.h"

#include "Runtime/Function/Framework/Components/CameraComponent.h"
#include "Runtime/Function/Framework/Components/MovementComponent.h"

#include "Runtime/Function/Input/InputSystem.h"
#include "Runtime/Function/Input/InputAction.h"
#include "Runtime/Function/Input/InputMappingContext.h"
#include "Runtime/Function/Input/InputModifiers.h"

using namespace minEngine;

class Playground : public Application
{
public:
    Playground() = default;
    ~Playground() = default;

    Engine* engine = nullptr;

    virtual void Initialize() override
    {
        engine = new Engine();
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
        // Set up IMC
        InputAction* IA_Look = new InputAction("IA_Look", InputActionValueType::Axis2D);
        InputAction* IA_Move = new InputAction("IA_Move", InputActionValueType::Axis3D);
        InputAction* IA_UpAndDown = new InputAction("IA_UpAndDown", InputActionValueType::Axis1D);
        
        InputMappingContext* inputMappingContext = new InputMappingContext({
            { IA_Look, InputKeys::Mouse2D },
            { IA_Move, InputKeys::Key_W },
            { IA_Move, InputKeys::Key_S, { std::make_shared<InputModifierNegate>() } },
            { IA_Move, InputKeys::Key_A, { std::make_shared<InputModifierNegate>(), std::make_shared<InputModifierSwizzleAxis>(InputSwizzleAxisOrder::ZYX) } },
            { IA_Move, InputKeys::Key_D, { std::make_shared<InputModifierSwizzleAxis>(InputSwizzleAxisOrder::ZYX) } },
            { IA_UpAndDown, InputKeys::Key_E, { std::make_shared<InputModifierSwizzleAxis>(InputSwizzleAxisOrder::YXZ) } },
            { IA_UpAndDown, InputKeys::Key_Q, { std::make_shared<InputModifierNegate>(), std::make_shared<InputModifierSwizzleAxis>(InputSwizzleAxisOrder::YXZ) } }
        });

        InputSystem::GetInputSystem().AddInputMappingContext(inputMappingContext, 0);

        // Set up a level
        minEngine::WorldManager& worldManager = minEngine::WorldManager::GetWorldManager();

        worldManager.m_CurrentActiveLevel = std::make_shared<minEngine::Level>();

        minEngine::Level& level = *worldManager.m_CurrentActiveLevel;
       
        // Create a player game object
        std::shared_ptr<GameObject> player = level.CreateGameObject();
        auto playerSceneComponent = player->CreateAndAddComponent<SceneComponent>();
        player->SetRootComponent(playerSceneComponent);

        player->CreateAndAddComponent<MovementComponent>();

        auto inputComponent = player->CreateAndAddComponent<InputComponent>();
        inputComponent->RegisterInputComponent();
        inputComponent->BindAction(IA_Move, InputTriggerEvent::Triggered,
            [inputComponent](const InputActionValue& value)
            {
                Vector3 forward = inputComponent->GetOwner()->GetRootComponent()->GetForwardVector() * value.Value.x ;
                Vector3 right = inputComponent->GetOwner()->GetRootComponent()->GetRightVector() * value.Value.z ;
                inputComponent->GetOwner()->GetComponent<MovementComponent>()->AddMovementInput(forward + right, value.GetMagnitude() * 0.01f);
            });
        inputComponent->BindAction(IA_Look, InputTriggerEvent::Triggered,
            [inputComponent](const InputActionValue& value)
            {
            });


        auto playerCameraComponent = player->CreateAndAddComponent<CameraComponent>();
        playerCameraComponent->AttachToComponent(playerSceneComponent.get(), AttachmentTransformRules::KeepRelativeTransform);
        playerCameraComponent->SetSelfAsMainCamera();
        playerSceneComponent->SetPosition(Vector3(-5.0f, 0.0f, 0.0f));

        // cube vertex data --------------------------------
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

        // ----------------------------------------------

        // create backpack 
        // minEngine::StaticMesh backpackMesh("D:/Dev/GitRepo/minEngine/minEngine/Assets/Models/backpack/backpack.obj");

        // create cube
        minEngine::StaticMesh cubeMesh(modelVertices, sizeof(modelVertices), 36,{
            minEngine::VertexElement("a_Position", minEngine::VertexElementType::Float3),
            minEngine::VertexElement("a_TexCoord", minEngine::VertexElementType::Float2),
            minEngine::VertexElement("a_Normal", minEngine::VertexElementType::Float3)
        });


        // create light
        minEngine::StaticMesh lightMesh(lightVertices, sizeof(lightVertices), 36,{
            minEngine::VertexElement("a_Position", minEngine::VertexElementType::Float3)
        });

        minEngine::Material backpackMaterial;
        backpackMaterial.m_Shader = std::make_shared<minEngine::OpenGLShader>("D:/Dev/GitRepo/minEngine/minEngine/Shaders/Phong.vert", "D:/Dev/GitRepo/minEngine/minEngine/Shaders/Phong.frag");
        backpackMaterial.m_Diffuse.Texture = std::make_shared<minEngine::OpenGLTexture2D>("D:/Dev/GitRepo/minEngine/minEngine/Assets/Models/backpack/diffuse.jpg", 0);
        backpackMaterial.m_Specular.Texture = std::make_shared<minEngine::OpenGLTexture2D>("D:/Dev/GitRepo/minEngine/minEngine/Assets/Models/backpack/specular.jpg", 1);

        minEngine::Material cubeMaterial;
        cubeMaterial.m_Shader = std::make_shared<minEngine::OpenGLShader>("D:/Dev/GitRepo/minEngine/minEngine/Shaders/Phong.vert", "D:/Dev/GitRepo/minEngine/minEngine/Shaders/Phong.frag");
        cubeMaterial.m_Diffuse.Texture = std::make_shared<minEngine::OpenGLTexture2D>("D:/Dev/GitRepo/minEngine/minEngine/Assets/Textures/container.jpg", 0);

        // create light material
        minEngine::Material lightMaterial;
        lightMaterial.m_Shader = std::make_shared<minEngine::OpenGLShader>("D:/Dev/GitRepo/minEngine/minEngine/Shaders/Light.vert", "D:/Dev/GitRepo/minEngine/minEngine/Shaders/Light.frag");
        lightMaterial.m_Diffuse.Value = minEngine::Vector4(1.0f, 1.0f, 1.0f, 1.0f);

        minEngine::Material spotLightMaterial;
        spotLightMaterial.m_Shader = lightMaterial.m_Shader;
        spotLightMaterial.m_Diffuse.Value = minEngine::Vector4(145.0f/255.0f, 245.0f/255.0f, 138.0f/255.0f, 1.0f);

        // create Backpack game object
        // auto backpack = level.CreateGameObject();
        // std::shared_ptr<minEngine::StaticMeshComponent> backpackMeshComponent = backpack->CreateAndAddComponent<minEngine::StaticMeshComponent>();
        // backpack->SetRootComponent(backpackMeshComponent);
        // backpackMeshComponent->SetMesh(std::make_shared<minEngine::StaticMesh>(backpackMesh));
        // backpackMeshComponent->SetMaterial(std::make_shared<minEngine::Material>(backpackMaterial));
        // backpack->SetPosition(minEngine::Vector3(0.0f, 0.0f, 0.5f));
        // backpack->SetScale(minEngine::Vector3(1.0f, 1.0f, 1.0f));
        

        // create Cube game object
        std::vector<minEngine::Transform> cubeTransforms = 
        {
            minEngine::Transform(minEngine::Vector3(0.0f, 0.0f, 0.0f), minEngine::Vector3(15.0f, 15.0f, 15.0f), minEngine::Vector3(1.0f, 1.0f, 1.0f)),
            minEngine::Transform(minEngine::Vector3(2.0f, 1.0f, -1.0f), minEngine::Vector3(30.0f, 45.0f, 60.0f), minEngine::Vector3(1.5f, 1.5f, 1.5f)),
            minEngine::Transform(minEngine::Vector3(-2.0f, -1.0f, 1.0f), minEngine::Vector3(45.0f, 30.0f, 15.0f), minEngine::Vector3(0.5f, 0.5f, 0.5f)),
            minEngine::Transform(minEngine::Vector3(1.0f, -2.0f, -2.0f), minEngine::Vector3(60.0f, 45.0f, 30.0f), minEngine::Vector3(2.5f, 2.5f, 2.5f)),
            minEngine::Transform(minEngine::Vector3(-1.0f, 2.0f, 2.0f), minEngine::Vector3(75.0f, 60.0f, 45.0f), minEngine::Vector3(0.1f, 0.1f, 0.1f))
        };
        std::vector<std::shared_ptr<minEngine::GameObject>> cubes;
        for(int i = 0; i < 1; ++i)
        {
            auto cube = level.CreateGameObject();
            std::shared_ptr<minEngine::StaticMeshComponent> cubeMeshComponent = cube->CreateAndAddComponent<minEngine::StaticMeshComponent>();
            cube->SetRootComponent(cubeMeshComponent);

            // set cube mesh and material
            cubeMeshComponent->SetMesh(std::make_shared<minEngine::StaticMesh>(cubeMesh));
            cubeMeshComponent->SetMaterial(std::make_shared<minEngine::Material>(cubeMaterial));

            // set cube transforms
            cube->SetTransform(cubeTransforms[i]);

            cubes.push_back(cube);
        }

        // create PointLight game object
        auto light = level.CreateGameObject();
        std::shared_ptr<minEngine::StaticMeshComponent> lightMeshComponent = light->CreateAndAddComponent<minEngine::StaticMeshComponent>();
        light->SetRootComponent(lightMeshComponent);

        std::shared_ptr<minEngine::PointLightComponent> lightComponent = light->CreateAndAddComponent<minEngine::PointLightComponent>();
        lightComponent->AttachToComponent(lightMeshComponent.get(), minEngine::AttachmentTransformRules::KeepRelativeTransform);

        // create DirectionalLight game object
        auto dirLight = level.CreateGameObject();
        std::shared_ptr<minEngine::DirectionalLightComponent> dirLightComponent = dirLight->CreateAndAddComponent<minEngine::DirectionalLightComponent>();
        dirLight->SetRootComponent(dirLightComponent);

        // create SpotLight game object
        auto spotLight = level.CreateGameObject();
        std::shared_ptr<minEngine::StaticMeshComponent> spotLightMeshComponent = spotLight->CreateAndAddComponent<minEngine::StaticMeshComponent>();
        spotLight->SetRootComponent(spotLightMeshComponent);

        std::shared_ptr<minEngine::SpotLightComponent> spotLightComponent = spotLight->CreateAndAddComponent<minEngine::SpotLightComponent>();
        spotLightComponent->AttachToComponent(spotLightMeshComponent.get(), minEngine::AttachmentTransformRules::KeepRelativeTransform);

        // set light mesh and material
        lightMeshComponent->SetMesh(std::make_shared<minEngine::StaticMesh>(lightMesh));
        lightMeshComponent->SetMaterial(std::make_shared<minEngine::Material>(lightMaterial));

        // set spot light mesh and material
        spotLightMeshComponent->SetMesh(std::make_shared<minEngine::StaticMesh>(lightMesh));
        spotLightMeshComponent->SetMaterial(std::make_shared<minEngine::Material>(spotLightMaterial));


        // set light transforms
        light->SetPosition(minEngine::Vector3(0.0f, 3.0f, 0.0f));
        light->SetScale(minEngine::Vector3(0.5f, 0.5f, 0.5f));
        light->SetRotation(minEngine::Vector3(0.0f, 45.0f, 0.0f));

        lightComponent->SetLightColor(minEngine::Vector4(1.0f, 1.0f, 1.0f, 1.0f));

        dirLightComponent->SetDirection(minEngine::Vector3(0.0f, -1.0f, -0.5f));
        dirLightComponent->SetLightColor(minEngine::Vector4(138.0/255.0f, 245.0/255.0f, 228.0/255.0f, 1.0f));

        spotLight->SetPosition(minEngine::Vector3(-2.0f, 2.0f, 2.0f) * 2.0f);
        spotLightComponent->SetDirection(minEngine::Vector3(1.0f, -1.0f, -1.0f));
        spotLightComponent->SetLightColor(minEngine::Vector4(227.0/255.0f, 138.0/255.0f, 245.0/255.0f, 1.0f));


        engine->Run();
    }
};

minEngine::Application* minEngine::CreateApplication()
{
    return new Playground();
}