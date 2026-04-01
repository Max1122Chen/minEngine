#include "minEngine.h"

#include "imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#include "Runtime/Function/Framework/Components/CameraComponent.h"
#include "Runtime/Function/Framework/Components/MovementComponent.h"

#include "Runtime/Function/Input/InputSystem.h"
#include "Runtime/Function/Input/InputAction.h"
#include "Runtime/Function/Input/InputMappingContext.h"
#include "Runtime/Function/Input/InputModifiers.h"

#include "Runtime/Function/Render/Texture.h"
#include "Runtime/Resource/AssetManager.h"

namespace minEngine
{
    class Editor : public Application
    {
    public:
        Editor() = default;
        virtual ~Editor() = default;

        virtual void Initialize() override
        {
            engine = new Engine();
            engine->Initialize();

            // Set up IMGUI
            ImGui::CreateContext();
            ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
            ImGui_ImplGlfw_InitForOpenGL(static_cast<GLFWwindow*>(RuntimeGlobalContext::GetRuntimeGlobalContext().m_WindowSystem->GetWindowHandle()), true);
            ImGui_ImplOpenGL3_Init();
        }

        virtual void Shutdown() override
        {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();

            engine->Shutdown();
            delete engine;
            engine = nullptr;
        }

        virtual void Run() override
        {
            SetupDebugScene();

            WindowSystem* windowSystem = RuntimeGlobalContext::GetRuntimeGlobalContext().m_WindowSystem.get();
            while (!windowSystem->ShouldClose())
            {
                // Start the Dear ImGui frame
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplGlfw_NewFrame();
                ImGui::NewFrame();

                float deltaTime = engine->CalculateDeltaTime();
                engine->TickOneFrame(deltaTime);

                // Render ImGui
                ImGui::Render();
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            }
        }

        inline void SetupDebugScene()
        {
            // Set up IMC
            InputAction IA_Look("IA_Look", InputActionValueType::Axis2D);
            InputAction IA_Move("IA_Move", InputActionValueType::Axis3D);
            InputAction IA_UpAndDown("IA_UpAndDown", InputActionValueType::Axis1D);
            InputAction IA_ScrollMove("IA_ScrollMove", InputActionValueType::Axis1D);
            
            InputMappingContext inputMappingContext({
                { &IA_Look, InputKeys::Mouse2D },
                { &IA_Move, InputKeys::Key_W },
                { &IA_Move, InputKeys::Key_S, { std::make_shared<InputModifierNegate>() } },
                { &IA_Move, InputKeys::Key_A, { std::make_shared<InputModifierNegate>(), std::make_shared<InputModifierSwizzleAxis>(InputSwizzleAxisOrder::ZYX) } },
                { &IA_Move, InputKeys::Key_D, { std::make_shared<InputModifierSwizzleAxis>(InputSwizzleAxisOrder::ZYX) } },
                { &IA_UpAndDown, InputKeys::Key_E },
                { &IA_UpAndDown, InputKeys::Key_Q, { std::make_shared<InputModifierNegate>() } },
                { &IA_ScrollMove, InputKeys::MouseScroll },
            });

            InputSystem::GetInputSystem().AddInputMappingContext(&inputMappingContext, 0);

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
            inputComponent->BindAction(&IA_Move, InputTriggerEvent::Triggered,
                [inputComponent](const InputActionValue& value)
                {
                    Vector3 forward = inputComponent->GetOwner()->GetRootComponent()->GetForwardVector() * value.Value.x ;
                    Vector3 right = inputComponent->GetOwner()->GetRootComponent()->GetRightVector() * value.Value.z ;
                    inputComponent->GetOwner()->GetComponent<MovementComponent>()->AddMovementInput(forward + right, value.GetMagnitude() * 0.01f);
                });
            inputComponent->BindAction(&IA_UpAndDown, InputTriggerEvent::Triggered,
                [inputComponent](const InputActionValue& value)
                {
                    Vector3 up = inputComponent->GetOwner()->GetRootComponent()->GetUpVector();
                    inputComponent->GetOwner()->GetComponent<MovementComponent>()->AddMovementInput(up, value.Value.x * 0.01f);
                });
            inputComponent->BindAction(&IA_ScrollMove, InputTriggerEvent::Triggered,
                [inputComponent](const InputActionValue& value)
                {
                    Vector3 forward = inputComponent->GetOwner()->GetRootComponent()->GetForwardVector();
                    inputComponent->GetOwner()->GetComponent<MovementComponent>()->AddMovementInput(forward, value.Value.x * 0.05f);
                });

            float lastMouseX = 0.0f;
            float lastMouseY = 0.0f;
            inputComponent->BindAction(&IA_Look, InputTriggerEvent::Triggered,
                [inputComponent, &lastMouseX, &lastMouseY](const InputActionValue& value)
                {
                    constexpr float kMouseSensitivity = 0.08f;
                    constexpr float kPitchMin = -89.0f;
                    constexpr float kPitchMax = 89.0f;

                    auto root = inputComponent->GetOwner()->GetRootComponent();
                    Vector3 rotation = root->GetRotation();
                    float deltaX = value.Value.x - lastMouseX;
                    float deltaY = lastMouseY - value.Value.y; // Invert Y axis
                    lastMouseX = value.Value.x;
                    lastMouseY = value.Value.y;
                    rotation.y -= deltaX * kMouseSensitivity;
                    rotation.z += deltaY * kMouseSensitivity;

                    if (rotation.z < kPitchMin)
                    {
                        rotation.z = kPitchMin;
                    }
                    else if (rotation.z > kPitchMax)
                    {
                        rotation.z = kPitchMax;
                    }

                    root->SetRotation(rotation);
                });


            auto playerCameraComponent = player->CreateAndAddComponent<CameraComponent>();
            playerCameraComponent->AttachToComponent(playerSceneComponent.get(), AttachmentTransformRules::KeepRelativeTransform);
            playerCameraComponent->SetSelfAsMainCamera();
            playerSceneComponent->SetPosition(Vector3(-5.0f, 0.0f, 0.0f));

            // cube vertex data --------------------------------
            float cubeVertices[] = {
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

            // create grass
            float planeVertices[] = {
                // positions          // texcoords   // normals
                1.0f, 0.0f,  1.0f,  1.0f, 0.0f,   0.0f, 1.0f, 0.0f,
                1.0f, 0.0f, -1.0f,  1.0f, 1.0f,   0.0f, 1.0f, 0.0f,
                -1.0f, 0.0f, -1.0f,  0.0f, 1.0f,   0.0f, 1.0f, 0.0f,

                1.0f, 0.0f,  1.0f,  1.0f, 0.0f,   0.0f, 1.0f, 0.0f,
                -1.0f, 0.0f, -1.0f,  0.0f, 1.0f,   0.0f, 1.0f, 0.0f,
                -1.0f, 0.0f,  1.0f,  0.0f, 0.0f,   0.0f, 1.0f, 0.0f
            };

            // create plane mesh
            minEngine::StaticMesh plane(planeVertices, sizeof(planeVertices), 6,{
                VertexElement("a_Position", VertexElementType::Float3),
                VertexElement("a_TexCoord", VertexElementType::Float2),
                VertexElement("a_Normal", VertexElementType::Float3)
            });

            // create backpack 
            // minEngine::StaticMesh backpackMesh("D:/Dev/GitRepo/minEngine/minEngine/Assets/Models/backpack/backpack.obj");

            // create cube mesh
            minEngine::StaticMesh cubeMesh(cubeVertices, sizeof(cubeVertices), 36,{
                VertexElement("a_Position", VertexElementType::Float3),
                VertexElement("a_TexCoord", VertexElementType::Float2),
                VertexElement("a_Normal", VertexElementType::Float3)
            });


            // create light mesh
            minEngine::StaticMesh lightMesh(lightVertices, sizeof(lightVertices), 36,{
                VertexElement("a_Position", VertexElementType::Float3)
            });

            minEngine::Material windowMaterial;
            windowMaterial.m_Shader = std::make_shared<minEngine::OpenGLShader>("D:/Dev/GitRepo/minEngine/minEngine/Shaders/Phong.vert", "D:/Dev/GitRepo/minEngine/minEngine/Shaders/Phong.frag");
            windowMaterial.m_Diffuse.Texture = AssetManager::GetAssetManager().LoadTexture2D("D:/Dev/GitRepo/minEngine/minEngine/Assets/Textures/window.png", 0);

            minEngine::Material grassMaterial;
            grassMaterial.m_Shader = std::make_shared<minEngine::OpenGLShader>("D:/Dev/GitRepo/minEngine/minEngine/Shaders/Phong.vert", "D:/Dev/GitRepo/minEngine/minEngine/Shaders/Phong.frag");
            grassMaterial.m_Diffuse.Texture = AssetManager::GetAssetManager().LoadTexture2D("D:/Dev/GitRepo/minEngine/minEngine/Assets/Textures/grass.png", 0);

            minEngine::Material backpackMaterial;
            backpackMaterial.m_Shader = std::make_shared<minEngine::OpenGLShader>("D:/Dev/GitRepo/minEngine/minEngine/Shaders/Phong.vert", "D:/Dev/GitRepo/minEngine/minEngine/Shaders/Phong.frag");
            backpackMaterial.m_Diffuse.Texture = AssetManager::GetAssetManager().LoadTexture2D("D:/Dev/GitRepo/minEngine/minEngine/Assets/Models/backpack/diffuse.jpg", 0);
            backpackMaterial.m_Specular.Texture = AssetManager::GetAssetManager().LoadTexture2D("D:/Dev/GitRepo/minEngine/minEngine/Assets/Models/backpack/specular.jpg", 1);

            minEngine::Material cubeMaterial;
            cubeMaterial.m_Shader = std::make_shared<minEngine::OpenGLShader>("D:/Dev/GitRepo/minEngine/minEngine/Shaders/Phong.vert", "D:/Dev/GitRepo/minEngine/minEngine/Shaders/Phong.frag");
            cubeMaterial.m_Diffuse.Texture = AssetManager::GetAssetManager().LoadTexture2D("D:/Dev/GitRepo/minEngine/minEngine/Assets/Textures/container.jpg", 0);
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

            // create window game object
            std::vector<minEngine::Transform> windowTransforms = 
            {
                minEngine::Transform(minEngine::Vector3(0.0f, 0.0f, -5.0f), minEngine::Vector3(0.0f, 0.0f, 90.0f), minEngine::Vector3(1.0f, 1.0f, 1.0f)),
                minEngine::Transform(minEngine::Vector3(5.0f, 0.0f, -10.0f), minEngine::Vector3(0.0f, 45.0f, 90.0f), minEngine::Vector3(2.0f, 2.0f, 2.0f)),
                minEngine::Transform(minEngine::Vector3(-5.0f, 0.0f, -10.0f), minEngine::Vector3(0.0f, -45.0f, 90.0f), minEngine::Vector3(2.0f, 2.0f, 2.0f))            
            };
            std::vector<std::shared_ptr<minEngine::GameObject>> windows;
            for(int i = 0; i < windowTransforms.size(); ++i)
            {
                auto window = level.CreateGameObject();
                std::shared_ptr<minEngine::StaticMeshComponent> windowMeshComponent = window->CreateAndAddComponent<minEngine::StaticMeshComponent>();
                window->SetRootComponent(windowMeshComponent);

                // set window mesh and material
                windowMeshComponent->SetMesh(std::make_shared<minEngine::StaticMesh>(plane));
                windowMeshComponent->SetMaterial(std::make_shared<minEngine::Material>(windowMaterial));

                // set window transforms
                window->SetTransform(windowTransforms[i]);

                windows.push_back(window);
            }


            // create grass game object
            std::vector<minEngine::Transform> grassTransforms = 
            {
                minEngine::Transform(minEngine::Vector3(5.0f, -0.5f, 0.0f), minEngine::Vector3(0.0f, 0.0f, 90.0f), minEngine::Vector3(1.0f, 1.0f, 1.0f)),
                minEngine::Transform(minEngine::Vector3(5.0f, -0.5f, 5.0f), minEngine::Vector3(0.0f, 0.0f, 90.0f), minEngine::Vector3(1.0f, 1.0f, 1.0f)),
                minEngine::Transform(minEngine::Vector3(10.0f, -0.5f, 10.0f), minEngine::Vector3(0.0f, 0.0f, 90.0f), minEngine::Vector3(1.0f, 1.0f, 1.0f))            
            };
            std::vector<std::shared_ptr<minEngine::GameObject>> grasses;
            for(int i = 0; i < grassTransforms.size(); ++i)
            {
                auto grass = level.CreateGameObject();
                std::shared_ptr<minEngine::StaticMeshComponent> grassMeshComponent = grass->CreateAndAddComponent<minEngine::StaticMeshComponent>();
                grass->SetRootComponent(grassMeshComponent);

                // set grass mesh and material
                grassMeshComponent->SetMesh(std::make_shared<minEngine::StaticMesh>(plane));
                grassMeshComponent->SetMaterial(std::make_shared<minEngine::Material>(grassMaterial));

                // set grass transforms
                grass->SetTransform(grassTransforms[i]);

                grasses.push_back(grass);
            }

            // create plane game object
            auto planeGO = level.CreateGameObject();
            std::shared_ptr<minEngine::StaticMeshComponent> planeMeshComponent = planeGO->CreateAndAddComponent<minEngine::StaticMeshComponent>();
            planeGO->SetRootComponent(planeMeshComponent);

            // create plane mesh and material
            planeMeshComponent->SetMesh(std::make_shared<minEngine::StaticMesh>(plane));
            planeMeshComponent->SetMaterial(std::make_shared<minEngine::Material>(cubeMaterial));

            planeGO->SetTransform(minEngine::Transform(minEngine::Vector3(0.0f, -1.0f, 0.0f), minEngine::Vector3(0.0f, 0.0f, 0.0f), minEngine::Vector3(10.0f, 1.0f, 10.0f)));


            // // create PointLight game object
            // auto light = level.CreateGameObject();
            // std::shared_ptr<minEngine::StaticMeshComponent> lightMeshComponent = light->CreateAndAddComponent<minEngine::StaticMeshComponent>();
            // light->SetRootComponent(lightMeshComponent);

            // std::shared_ptr<minEngine::PointLightComponent> lightComponent = light->CreateAndAddComponent<minEngine::PointLightComponent>();
            // lightComponent->AttachToComponent(lightMeshComponent.get(), minEngine::AttachmentTransformRules::KeepRelativeTransform);

            // create DirectionalLight game object
            auto dirLight = level.CreateGameObject();
            std::shared_ptr<minEngine::DirectionalLightComponent> dirLightComponent = dirLight->CreateAndAddComponent<minEngine::DirectionalLightComponent>();
            dirLight->SetRootComponent(dirLightComponent);

            // // create SpotLight game object
            // auto spotLight = level.CreateGameObject();
            // std::shared_ptr<minEngine::StaticMeshComponent> spotLightMeshComponent = spotLight->CreateAndAddComponent<minEngine::StaticMeshComponent>();
            // spotLight->SetRootComponent(spotLightMeshComponent);

            // std::shared_ptr<minEngine::SpotLightComponent> spotLightComponent = spotLight->CreateAndAddComponent<minEngine::SpotLightComponent>();
            // spotLightComponent->AttachToComponent(spotLightMeshComponent.get(), minEngine::AttachmentTransformRules::KeepRelativeTransform);

            // // set light mesh and material
            // lightMeshComponent->SetMesh(std::make_shared<minEngine::StaticMesh>(lightMesh));
            // lightMeshComponent->SetMaterial(std::make_shared<minEngine::Material>(lightMaterial));
            // lightMeshComponent->SetCastShadow(false);

            // // set spot light mesh and material
            // spotLightMeshComponent->SetMesh(std::make_shared<minEngine::StaticMesh>(lightMesh));
            // spotLightMeshComponent->SetMaterial(std::make_shared<minEngine::Material>(spotLightMaterial));
            // spotLightMeshComponent->SetCastShadow(false);


            // // set light transforms
            // light->SetPosition(minEngine::Vector3(0.0f, 3.0f, 0.0f));
            // light->SetScale(minEngine::Vector3(0.5f, 0.5f, 0.5f));
            // light->SetRotation(minEngine::Vector3(0.0f, 45.0f, 0.0f));

            // lightComponent->SetLightColor(minEngine::Vector4(1.0f, 1.0f, 1.0f, 1.0f));

            dirLightComponent->SetDirection(minEngine::Vector3(0.0f, -1.0f, -2.0f));
            dirLightComponent->SetLightColor(minEngine::Vector4(138.0/255.0f, 245.0/255.0f, 228.0/255.0f, 1.0f));
            dirLightComponent->SetCastShadow(true);

            // spotLight->SetPosition(minEngine::Vector3(-2.0f, 2.0f, 2.0f) * 2.0f);
            // spotLightComponent->SetDirection(minEngine::Vector3(1.0f, -1.0f, -1.0f));
            // spotLightComponent->SetLightColor(minEngine::Vector4(227.0/255.0f, 138.0/255.0f, 245.0/255.0f, 1.0f));


            engine->Run();

            InputSystem::GetInputSystem().RemoveInputMappingContext(&inputMappingContext);
        
        }

    private: 
        Engine* engine = nullptr;
    };

    Application* CreateApplication()
    {
        return new Editor();
    }
}