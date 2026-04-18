#pragma once
#include "Core.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"

namespace minEngine
{
    class GameObject;

    ME_CLASS()
    class Scene : public MEObject
    {  
        ME_REFLECTION_FRIEND(Scene)
    public:
        Scene() = default;
        virtual ~Scene() = default;

        void Tick(float deltaTime);

        std::shared_ptr<GameObject> CreateGameObject();
        void Reset();
        void RebuildRuntimeGameObjectIndex();
        const std::string& GetSceneName() const { return sceneName; }
        const std::vector<std::shared_ptr<GameObject>>& GetGameObjects() const { return m_GameObjects; }
        const std::unordered_map<uint64_t, GameObject*>& GetGameObjectsById() const { return m_GameObjectsById; }
        uint64_t IncrementNextGOId() { return m_NextGOId++; }

    // private: // temporarily public for testing
        ME_PROPERTY()
        std::string sceneName;

        ME_PROPERTY()
        std::vector<std::shared_ptr<GameObject>> m_GameObjects;

        std::unordered_map<uint64_t, GameObject*> m_GameObjectsById;

    private:
        uint64_t m_NextGOId{ 0 };
    };
}

#include "Scene.gen.h"