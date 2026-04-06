#pragma once
#include "Core.h"


namespace minEngine
{
    class GameObject;

    class Scene
    {  
    public:
        Scene() = default;
        virtual ~Scene() = default;

        void Tick(float deltaTime);

        std::shared_ptr<GameObject> CreateGameObject();
        std::shared_ptr<GameObject> CreateGameObject(uint64_t id);
        const std::string& GetSceneName() const { return sceneName; }

    // private: // temporarily public for testing
        std::string sceneName;
        std::unordered_map<uint64_t, std::shared_ptr<GameObject>> m_GameObjects;

    private:
        uint64_t m_NextObjectId{ 0 };
    };
}