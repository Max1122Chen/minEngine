#include "GameInstance.h"

#include "Runtime/Core/Log/LogSystem.h"

namespace minEngine
{
    void GameInstance::Init()
    {
        ME_ASSERT(!m_bInitialized, "GameInstance::Init called twice");
        OnInit();
        m_Subsystems.Initialize(this);
        m_bInitialized = true;
    }

    void GameInstance::Shutdown()
    {
        if (!m_bInitialized)
        {
            m_PrimaryScene = nullptr;
            return;
        }

        OnShutdown();
        m_Subsystems.Deinitialize();
        m_PrimaryScene = nullptr;
        m_bInitialized = false;
    }

    void GameInstance::TickSubsystems(float deltaTime)
    {
        if (!m_Subsystems.IsInitialized())
        {
            return;
        }

        m_Subsystems.ForEachSubsystem([deltaTime](Subsystem* subsystem) {
            if (subsystem == nullptr || !subsystem->IsA(TickableGameInstanceSubsystem::StaticClass()))
            {
                return;
            }

            TickableGameInstanceSubsystem* tickable =
                static_cast<TickableGameInstanceSubsystem*>(subsystem);
            if (tickable->IsTickable())
            {
                tickable->Tick(deltaTime);
            }
        });
    }
}
