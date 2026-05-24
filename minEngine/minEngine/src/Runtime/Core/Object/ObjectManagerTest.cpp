#include "ObjectManagerTest.h"

#include "ObjectManager.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Framework/Components/MovementComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"

namespace minEngine
{
    class ObjectManagerTestScope
    {
    public:
        ObjectManagerTestScope()
        {
            ObjectManager::SetInstance(&m_Manager);
            m_Manager.Initialize();
        }

        ~ObjectManagerTestScope()
        {
            m_Manager.Shutdown();
            ObjectManager::SetInstance(nullptr);
        }

    private:
        ObjectManager m_Manager;
    };

    namespace
    {
        bool EnsureReflectionReady()
        {
            Reflection::ReflectionSystem& reflection = Reflection::ReflectionSystem::Get();
            if (reflection.IsReady())
            {
                return true;
            }

            if (!reflection.FinalizeReflection())
            {
                for (const std::string& error : reflection.GetLastErrors())
                {
                    ME_CORE_ERROR("{}", error);
                }
                return false;
            }

            reflection.ClearErrors();
            return true;
        }

        bool TestLocalSharedPtrUnregistersOnDestroy()
        {
            GUID guid;
            {
                std::shared_ptr<Scene> scene = NewObject<Scene>("TransientScene");
                guid = scene->GetGuid();
                if (FindObject(guid) != scene)
                {
                    ME_CORE_ERROR("ObjectManagerTest: FindObject should resolve live Scene.");
                    return false;
                }
                if (ObjectManager::Get().GetTrackedObjectCount() != 1)
                {
                    ME_CORE_ERROR(
                        "ObjectManagerTest: expected 1 tracked object, got {}.",
                        ObjectManager::Get().GetTrackedObjectCount());
                    return false;
                }
            }

            if (FindObject(guid) != nullptr)
            {
                ME_CORE_ERROR("ObjectManagerTest: FindObject should be null after shared_ptr release.");
                return false;
            }

            ObjectManager::Get().CollectGarbage();
            if (ObjectManager::Get().GetTrackedObjectCount() != 0)
            {
                ME_CORE_ERROR(
                    "ObjectManagerTest: expected 0 tracked objects after CollectGarbage, got {}.",
                    ObjectManager::Get().GetTrackedObjectCount());
                return false;
            }

            return true;
        }

        bool TestSceneHierarchyUnregistersWithoutRemoveObject()
        {
            GUID sceneGuid;
            GUID gameObjectGuid;
            GUID componentGuid;

            {
                std::shared_ptr<Scene> scene = NewObject<Scene>("HierarchyScene");
                sceneGuid = scene->GetGuid();

                std::shared_ptr<GameObject> gameObject = scene->CreateGameObject();
                gameObjectGuid = gameObject->GetGuid();

                std::shared_ptr<MovementComponent> movement = gameObject->AddComponent<MovementComponent>();
                componentGuid = movement->GetGuid();

                const size_t trackedWhileAlive = ObjectManager::Get().GetTrackedObjectCount();
                if (trackedWhileAlive < 3)
                {
                    ME_CORE_ERROR(
                        "ObjectManagerTest: expected at least 3 tracked objects (scene/go/component), got {}.",
                        trackedWhileAlive);
                    return false;
                }
            }

            if (FindObject(sceneGuid) != nullptr || FindObject(gameObjectGuid) != nullptr
                || FindObject(componentGuid) != nullptr)
            {
                ME_CORE_ERROR("ObjectManagerTest: scene hierarchy GUIDs should not resolve after release.");
                return false;
            }

            ObjectManager::Get().CollectGarbage();
            if (ObjectManager::Get().GetTrackedObjectCount() != 0)
            {
                ME_CORE_ERROR(
                    "ObjectManagerTest: registry should be empty after hierarchy teardown, count={}.",
                    ObjectManager::Get().GetTrackedObjectCount());
                return false;
            }

            return true;
        }

        bool TestCollectGarbageWithEngineRootsHonorsRegisteredRootSources()
        {
            std::shared_ptr<Scene> scene = NewObject<Scene>("EngineRootScene");
            scene->CreateGameObject();

            ObjectManager::Get().RegisterGarbageRootSource(scene.get(), [&scene](const ObjectReachabilityMarker& markReachable) {
                scene->MarkReachableObjects(markReachable);
            });

            ObjectManager::Get().CollectGarbageWithEngineRoots();
            ObjectManager::Get().UnregisterGarbageRootSource(scene.get());
            return ObjectManager::Get().GetTrackedObjectCount() >= 1;
        }
    }

    bool ShouldRunObjectManagerTestsOnly(int argc, char** argv)
    {
        for (int argIndex = 1; argIndex < argc; ++argIndex)
        {
            if (argv[argIndex] != nullptr && std::string_view(argv[argIndex]) == "--object-manager-test")
            {
                return true;
            }
        }
        return false;
    }

    bool RunObjectManagerTests(int argc, char** argv)
    {
        (void)argc;
        (void)argv;

        if (!EnsureReflectionReady())
        {
            return false;
        }

        ObjectManagerTestScope scope;

        if (!TestLocalSharedPtrUnregistersOnDestroy())
        {
            return false;
        }

        if (!TestSceneHierarchyUnregistersWithoutRemoveObject())
        {
            return false;
        }

        if (!TestCollectGarbageWithEngineRootsHonorsRegisteredRootSources())
        {
            return false;
        }

        ME_CORE_INFO("ObjectManagerTest: all tests passed.");
        return true;
    }
}
