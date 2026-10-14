#pragma once

#include "Core.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Function/Framework/Subsystems/Subsystem.h"
#include "Runtime/Function/Framework/Subsystems/SubsystemCollection.h"

namespace minEngine
{
    class Scene;

    /**
     * Session host: Standalone one instance; PIE one per instance.
     * Owns GameInstanceSubsystem collection.
     */
    ME_CLASS()
    class GameInstance : public MEObject
    {
        ME_GENERATED_BODY()

    public:
        GameInstance() = default;
        ~GameInstance() override = default;

        void Init();
        void Shutdown();

        Scene* GetPrimaryScene() const { return m_PrimaryScene; }
        void SetPrimaryScene(Scene* scene) { m_PrimaryScene = scene; }

        int32_t GetPIEInstanceId() const { return m_PIEInstanceId; }
        void SetPIEInstanceId(int32_t pieInstanceId) { m_PIEInstanceId = pieInstanceId; }

        bool IsDedicatedServerInstance() const { return false; }

        template<typename T>
        T* GetSubsystem() const
        {
            return m_Subsystems.GetSubsystem<T>();
        }

        SubsystemCollection<GameInstanceSubsystem>& GetSubsystemCollection() { return m_Subsystems; }
        const SubsystemCollection<GameInstanceSubsystem>& GetSubsystemCollection() const { return m_Subsystems; }

        template<typename T>
        static T* GetSubsystem(const GameInstance* gameInstance)
        {
            if (gameInstance == nullptr)
            {
                return nullptr;
            }
            return gameInstance->GetSubsystem<T>();
        }

        virtual void OnInit() {}
        virtual void OnShutdown() {}

        /** Tick TickableGameInstanceSubsystem instances (Engine LogicalTick phase). */
        void TickSubsystems(float deltaTime);

    private:
        SubsystemCollection<GameInstanceSubsystem> m_Subsystems;
        Scene* m_PrimaryScene = nullptr;
        int32_t m_PIEInstanceId = -1;
        bool m_bInitialized = false;
    };
}

#include "GameInstance.gen.h"
