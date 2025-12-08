#pragma once
#include "Core.h"


namespace minEngine
{
    class GameObject;

    class Level
    {  
    public:
        Level() = default;
        virtual ~Level() = default;

        void Tick(float deltaTime);

        std::shared_ptr<GameObject> CreateGameObject();


    // private: // temporarily public for testing
        std::unordered_map<uint64_t, std::shared_ptr<GameObject>> m_GameObjects;
    };
}