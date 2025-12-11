#pragma once
#include "Core.h"
#include "Runtime/Function/RuntimeGlobalContext.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Framework/Level/Level.h"

namespace minEngine
{
    class RuntimeGlobalContext;
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
        static WorldManager& GetWorldManager() { return *RuntimeGlobalContext::GetRuntimeGlobalContext().m_WorldManager; }
        
        void Tick(float deltaTime);

        RenderScene* GetRenderScene() const { return m_RenderScene; }

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