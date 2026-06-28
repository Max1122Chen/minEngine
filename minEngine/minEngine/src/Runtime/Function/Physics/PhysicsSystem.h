#pragma once

#include "Core.h"
#include "Runtime/Function/Physics/PhysicsWorld.h"

#include <memory>
#include <unordered_map>

namespace minEngine
{
    class Scene;
    class PhysicsSyncTestScope;
    class PhysicsLoadTestScope;

    class PhysicsSystem
    {
    public:
        PhysicsSystem() = default;
        ~PhysicsSystem() = default;

        void Initialize();
        void Shutdown();

        static PhysicsSystem& Get();
        static bool HasInstance();

        void SimulateActiveScene(float deltaTime);

        PhysicsWorld& GetOrCreateWorld(Scene* scene);
        void DestroyWorld(Scene* scene);
        void RebuildWorldBodies(Scene* scene);

    private:
        friend class Engine;
        friend class PhysicsSmokeTestScope;
        friend class PhysicsSyncTestScope;
        friend class PhysicsLoadTestScope;

        static void SetInstance(PhysicsSystem* instance);

        static PhysicsSystem* s_Instance;

        bool m_Initialized = false;
        std::unordered_map<Scene*, std::unique_ptr<PhysicsWorld>> m_Worlds;
    };
}
