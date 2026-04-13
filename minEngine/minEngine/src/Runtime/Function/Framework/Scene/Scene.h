#pragma once
#include "Core.h"
#include "Runtime/Core/Object/MEObject.h"

namespace minEngine
{
    class GameObject;

    ME_CLASS()
    class Scene : public MEObject
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

#include "Scene.gen.h"