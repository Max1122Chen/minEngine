#include "PhysicsSystem.h"

#include "BoxColliderComponent.h"
#include "RigidBodyComponent.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/IssueReporting.h>

#include <cstdarg>

JPH_SUPPRESS_WARNINGS

namespace minEngine
{
    PhysicsSystem* PhysicsSystem::s_Instance = nullptr;

    class PhysicsJoltTrace
    {
    public:
        static void TraceCallback(const char* format, ...)
        {
            char buffer[1024];
            va_list argumentList;
            va_start(argumentList, format);
            vsnprintf(buffer, sizeof(buffer), format, argumentList);
            va_end(argumentList);
            ME_CORE_TRACE("Jolt: {}", buffer);
        }
    };

#ifdef JPH_ENABLE_ASSERTS
    class PhysicsJoltAssert
    {
    public:
        static bool AssertFailedCallback(
            const char* expression,
            const char* message,
            const char* file,
            JPH::uint line)
        {
            ME_CORE_ERROR("Jolt assert failed: {}:{} ({}) {}", file, line, expression, message != nullptr ? message : "");
            return true;
        }
    };
#endif

    void PhysicsSystem::SetInstance(PhysicsSystem* instance)
    {
        s_Instance = instance;
    }

    PhysicsSystem& PhysicsSystem::Get()
    {
        ME_ASSERT(s_Instance != nullptr, "PhysicsSystem is not initialized");
        return *s_Instance;
    }

    bool PhysicsSystem::HasInstance()
    {
        return s_Instance != nullptr;
    }

    void PhysicsSystem::Initialize()
    {
        if (m_Initialized)
        {
            return;
        }

        JPH::RegisterDefaultAllocator();
        JPH::Trace = PhysicsJoltTrace::TraceCallback;
#ifdef JPH_ENABLE_ASSERTS
        JPH::AssertFailed = PhysicsJoltAssert::AssertFailedCallback;
#endif

        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();

        m_Initialized = true;
        ME_CORE_INFO("PhysicsSystem initialized.");
    }

    void PhysicsSystem::Shutdown()
    {
        if (!m_Initialized)
        {
            return;
        }

        m_Worlds.clear();

        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;

        m_Initialized = false;
        ME_CORE_INFO("PhysicsSystem shutdown.");
    }

    PhysicsWorld& PhysicsSystem::GetOrCreateWorld(Scene* scene)
    {
        ME_ASSERT(scene != nullptr, "PhysicsSystem::GetOrCreateWorld requires a valid Scene");
        const auto existingWorld = m_Worlds.find(scene);
        if (existingWorld != m_Worlds.end())
        {
            return *existingWorld->second;
        }

        std::unique_ptr<PhysicsWorld> newWorld = std::make_unique<PhysicsWorld>();
        PhysicsWorld& worldReference = *newWorld;
        m_Worlds.emplace(scene, std::move(newWorld));
        return worldReference;
    }

    void PhysicsSystem::DestroyWorld(Scene* scene)
    {
        if (scene == nullptr)
        {
            return;
        }

        m_Worlds.erase(scene);
    }

    void PhysicsSystem::RebuildWorldBodies(Scene* scene)
    {
        if (!m_Initialized || scene == nullptr)
        {
            return;
        }

        GetOrCreateWorld(scene);

        for (const std::shared_ptr<GameObject>& gameObject : scene->GetAllGameObjects())
        {
            if (!gameObject)
            {
                continue;
            }

            RigidBodyComponent* rigidBodyComponent = nullptr;
            BoxColliderComponent* boxColliderComponent = nullptr;

            for (const std::shared_ptr<Component>& component : gameObject->GetAllComponents())
            {
                if (!component || !component->GetClass())
                {
                    continue;
                }

                if (component->IsA(RigidBodyComponent::StaticClass()))
                {
                    rigidBodyComponent = static_cast<RigidBodyComponent*>(component.get());
                }
                else if (component->IsA(BoxColliderComponent::StaticClass()))
                {
                    boxColliderComponent = static_cast<BoxColliderComponent*>(component.get());
                }
            }

            if (rigidBodyComponent != nullptr)
            {
                rigidBodyComponent->RefreshPhysicsBody(boxColliderComponent);
            }
        }
    }

    void PhysicsSystem::SimulateActiveScene(float deltaTime)
    {
        if (!SceneManager::HasInstance())
        {
            return;
        }

        const std::shared_ptr<Scene> activeScene = SceneManager::Get().GetCurrentActiveScene();
        if (!activeScene)
        {
            return;
        }

        PhysicsWorld& world = GetOrCreateWorld(activeScene.get());
        world.SyncBodiesFromScene();
        world.Step(deltaTime);
        world.SyncBodiesToScene();
    }
}
