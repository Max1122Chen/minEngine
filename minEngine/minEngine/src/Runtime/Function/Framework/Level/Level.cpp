#include "Level.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"

namespace minEngine
{
    void Level::Tick(float deltaTime)
    {
        for (auto& [id, gameObject] : m_GameObjects)
        {
            gameObject->Tick(deltaTime);
        }
    }

    std::shared_ptr<GameObject> Level::CreateGameObject()
    {
        static uint64_t nextID = 0; // simple ID generator
        uint64_t id = nextID++;
        auto gameObject = std::make_shared<GameObject>(id);
        m_GameObjects[id] = gameObject;
        return gameObject;
    }
}