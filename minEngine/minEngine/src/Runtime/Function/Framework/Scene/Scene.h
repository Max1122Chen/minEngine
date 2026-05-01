#pragma once
#include "Core.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Resource/Asset.h"

namespace minEngine
{
    class GameObject;

    ME_CLASS()
    class Scene : public Asset
    {  
        ME_GENERATED_BODY(Scene)
    public:
        Scene() = default;
        virtual ~Scene();

        void Tick(float deltaTime);

        std::shared_ptr<GameObject> CreateGameObject();
        void Reset();
        void RebuildRuntimeGameObjectIndex();
        const std::string& GetSceneName() const { return m_SceneName; }
        const std::vector<std::shared_ptr<GameObject>>& GetAllGameObjects() const { return m_GameObjects; }
        const std::unordered_map<uint64_t, GameObject*>& GetGameObjectsById() const { return m_GameObjectsById; }
        GameObject* FindGameObjectById(uint64_t id) const;
        bool RemoveGameObjectById(uint64_t id);
        uint64_t IncrementNextGOId() { return m_NextGOId++; }

    // private: // temporarily public for testing
        ME_PROPERTY()
        std::string m_SceneName;

        ME_PROPERTY(Instanced)
        std::vector<std::shared_ptr<GameObject>> m_GameObjects;

        std::unordered_map<uint64_t, GameObject*> m_GameObjectsById;

    private:
        uint64_t m_NextGOId{ 0 };
    };
}

#include "Scene.gen.h"