#pragma once
#include "Core.h"

namespace minEngine
{
    class Level;
    class RenderScene;
    class Component;
    class PrimitiveComponent;

    class WorldManager
    {
    public:
        WorldManager() = default;
        virtual ~WorldManager() = default;

        void Initialize();
        void Shutdown();
        
        void Tick(float deltaTime);

        void MarkComponentForNeededEndOfFrameUpdate(Component* component);
        void SendAllEndOfFrameUpdates();    // to render thread

        // TODO: add level loading functionality
        // void LoadLevel(const std::shared_ptr<Level>& level);

    // private: // temporarily public for testing
        std::shared_ptr<Level> m_CurrentActiveLevel{ nullptr };

        // Components that need end of frame render data update
        std::vector<Component*> m_ComponentsThatNeedEndOfFrameUpdate;

    private: 
        RenderScene* m_RenderScene{ nullptr };

    };
}