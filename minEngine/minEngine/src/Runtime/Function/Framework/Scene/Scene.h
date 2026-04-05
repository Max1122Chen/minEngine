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


    // private: // temporarily public for testing
        std::unordered_map<uint64_t, std::shared_ptr<GameObject>> m_GameObjects;
    };
}