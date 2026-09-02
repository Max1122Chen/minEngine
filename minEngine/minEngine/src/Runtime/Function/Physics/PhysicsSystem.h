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
    class PhysicsContactTestScope;
    class PhysicsLineTraceTestScope;
    class PhysicsShapesTestScope;

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
        void OnBeginPIE(Scene* pieScene);
        void OnEndPIE(Scene* pieScene);

        PhysicsWorld& GetOrCreateWorld(Scene* scene);
        void DestroyWorld(Scene* scene);
        void RebuildWorldBodies(Scene* scene);

    private:
        friend class Engine;
        friend class PhysicsSmokeTestScope;
        friend class PhysicsSyncTestScope;
        friend class PhysicsLoadTestScope;
        friend class PhysicsContactTestScope;
        friend class PhysicsLineTraceTestScope;
        friend class PhysicsShapesTestScope;

        static void SetInstance(PhysicsSystem* instance);

        static PhysicsSystem* s_Instance;

        bool m_Initialized = false;
        std::unordered_map<Scene*, std::unique_ptr<PhysicsWorld>> m_Worlds;
    };
}
