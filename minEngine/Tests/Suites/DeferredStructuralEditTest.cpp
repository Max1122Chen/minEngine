#include "DeferredStructuralEditTest.h"
#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"

namespace minEngine
{
    class DeferredStructuralEditTestScope
    {
    public:
        DeferredStructuralEditTestScope()
        {
            Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
            ObjectManager::Initialize();

            Testing::TestAccess<SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();
        }

        ~DeferredStructuralEditTestScope()
        {
            m_SceneManager.Shutdown();
            Testing::TestAccess<SceneManager>::SetInstance(nullptr);

            ObjectManager::Shutdown();
            Testing::TestAccess<ObjectManager>::SetInstance(nullptr);
        }

    private:
        ObjectManager m_ObjectManager;
        SceneManager m_SceneManager;
    };

    namespace
    {
        class TickCounterComponent : public SceneComponent
        {
        public:
            int* TickCount = nullptr;

            void Tick(float /*deltaTime*/) override
            {
                if (TickCount)
                {
                    ++(*TickCount);
                }
            }
        };

        class SpawnDuringTickComponent : public SceneComponent
        {
        public:
            uint64_t* SpawnedId = nullptr;
            int* SpawnedTickCount = nullptr;

            void Tick(float /*deltaTime*/) override
            {
                if (SpawnedId == nullptr || *SpawnedId != 0)
                {
                    return;
                }

                Scene* scene = GetOwningScene();
                if (scene == nullptr)
                {
                    return;
                }

                const std::shared_ptr<GameObject> spawned = scene->SpawnGameObject();
                if (!spawned)
                {
                    return;
                }

                *SpawnedId = spawned->GetID();
                auto probe = std::make_shared<TickCounterComponent>();
                probe->TickCount = SpawnedTickCount;
                spawned->InsertRestoredComponent(probe, 0);
            }
        };

        class MarkPeerOnEofComponent : public SceneComponent
        {
        public:
            Component* Peer = nullptr;
            int* RunCount = nullptr;

            void DoEndOfFrameUpdate() override
            {
                if (RunCount)
                {
                    ++(*RunCount);
                }

                if (Peer)
                {
                    Peer->MarkForNeededEndOfFrameUpdate();
                }
            }
        };

        class DestroyOwnerOnBeginPlayComponent : public SceneComponent
        {
        public:
            void OnBeginPlay() override
            {
                Scene* scene = GetOwningScene();
                GameObject* owner = GetOwner();
                if (scene == nullptr || owner == nullptr)
                {
                    return;
                }

                scene->DestroyGameObject(owner->GetID());
            }
        };

        class RemoveDuringTickComponent : public SceneComponent
        {
        public:
            uint64_t VictimId = 0;
            bool* RemoveSucceeded = nullptr;

            void Tick(float /*deltaTime*/) override
            {
                Scene* scene = GetOwningScene();
                if (scene == nullptr || RemoveSucceeded == nullptr)
                {
                    return;
                }

                *RemoveSucceeded = scene->RemoveGameObjectById(VictimId);
            }
        };

        std::shared_ptr<TickCounterComponent> AttachTickCounter(GameObject& gameObject, int* tickCount)
        {
            auto probe = std::make_shared<TickCounterComponent>();
            probe->TickCount = tickCount;
            gameObject.InsertRestoredComponent(probe, gameObject.GetAllComponents().size());
            return probe;
        }

        bool RunSpawnImmediateFindAndDeferTickTest()
        {
            DeferredStructuralEditTestScope scope;

            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("dse-spawn-tick");
            if (!scene)
            {
                ME_LOG(LogTest, Error, "DeferredStructuralEdit: failed to create scene.");
                return false;
            }

            scene->DispatchBeginPlay();

            const std::shared_ptr<GameObject> spawner = scene->SpawnGameObject();
            uint64_t spawnedId = 0;
            int spawnedTickCount = 0;
            auto spawnComp = std::make_shared<SpawnDuringTickComponent>();
            spawnComp->SpawnedId = &spawnedId;
            spawnComp->SpawnedTickCount = &spawnedTickCount;
            spawner->InsertRestoredComponent(spawnComp, 0);

            scene->Tick(0.016f);

            if (spawnedId == 0 || scene->FindGameObjectById(spawnedId) == nullptr)
            {
                ME_LOG(LogTest, Error, "DeferredStructuralEdit: Spawn during Tick not immediately Findable.");
                return false;
            }

            if (scene->GetAllGameObjects().size() < 2)
            {
                ME_LOG(LogTest, Error, "DeferredStructuralEdit: Spawn during Tick not in GetAll.");
                return false;
            }

            if (spawnedTickCount != 0)
            {
                ME_LOG(LogTest, Error, "DeferredStructuralEdit: same-frame Tick count expected 0, got {}.", spawnedTickCount);
                return false;
            }

            scene->Tick(0.016f);
            if (spawnedTickCount != 1)
            {
                ME_LOG(LogTest, Error, "DeferredStructuralEdit: next-frame Tick count expected 1, got {}.", spawnedTickCount);
                return false;
            }

            return true;
        }

        bool RunSpawnBeginPlayImmediateTest()
        {
            DeferredStructuralEditTestScope scope;

            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("dse-spawn-beginplay");
            scene->DispatchBeginPlay();

            const std::shared_ptr<GameObject> go = scene->SpawnGameObject();
            if (!go || !go->HasBegunPlay())
            {
                ME_LOG(LogTest, Error, "DeferredStructuralEdit: Spawn after Play did not BeginPlay before return.");
                return false;
            }

            return true;
        }

        bool RunDestroyDeferredFlushTest()
        {
            DeferredStructuralEditTestScope scope;

            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("dse-destroy");
            scene->DispatchBeginPlay();

            const std::shared_ptr<GameObject> go = scene->SpawnGameObject();
            const uint64_t id = go->GetID();
            int tickCount = 0;
            AttachTickCounter(*go, &tickCount);

            if (!scene->DestroyGameObject(id))
            {
                ME_LOG(LogTest, Error, "DeferredStructuralEdit: DestroyGameObject failed.");
                return false;
            }

            if (!go->IsPendingDestroy() || scene->FindGameObjectById(id) == nullptr)
            {
                ME_LOG(LogTest, Error, "DeferredStructuralEdit: pending destroy should remain Findable.");
                return false;
            }

            scene->Tick(0.016f);
            if (tickCount != 0)
            {
                ME_LOG(LogTest, Error, "DeferredStructuralEdit: pending destroy should skip Tick.");
                return false;
            }

            SceneManager::Get().FlushDeferredStructuralEdits();
            if (scene->FindGameObjectById(id) != nullptr)
            {
                ME_LOG(LogTest, Error, "DeferredStructuralEdit: Flush did not erase destroyed GO.");
                return false;
            }

            return true;
        }

        bool RunEofDirtySpreadsNextFrameTest()
        {
            DeferredStructuralEditTestScope scope;

            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("dse-eof");
            const std::shared_ptr<GameObject> goA = scene->CreateGameObject();
            const std::shared_ptr<GameObject> goB = scene->CreateGameObject();

            int runA = 0;
            int runB = 0;
            auto eofA = std::make_shared<MarkPeerOnEofComponent>();
            auto eofB = std::make_shared<MarkPeerOnEofComponent>();
            eofA->RunCount = &runA;
            eofB->RunCount = &runB;
            eofA->Peer = eofB.get();
            goA->InsertRestoredComponent(eofA, 0);
            goB->InsertRestoredComponent(eofB, 0);

            eofA->MarkForNeededEndOfFrameUpdate();

            SceneManager::Get().SendAllEndOfFrameUpdates();
            if (runA != 1 || runB != 0)
            {
                ME_LOG(
                    LogTest,
                    Error,
                    "DeferredStructuralEdit: EOF snapshot expected A=1 B=0, got A={} B={}.",
                    runA,
                    runB);
                return false;
            }

            SceneManager::Get().SendAllEndOfFrameUpdates();
            if (runA != 1 || runB != 1)
            {
                ME_LOG(
                    LogTest,
                    Error,
                    "DeferredStructuralEdit: next EOF expected A=1 B=1, got A={} B={}.",
                    runA,
                    runB);
                return false;
            }

            return true;
        }

        bool RunRemoveDuringTickRejectedTest()
        {
            DeferredStructuralEditTestScope scope;

            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("dse-remove-guard");
            const std::shared_ptr<GameObject> victim = scene->CreateGameObject();
            const uint64_t victimId = victim->GetID();

            const std::shared_ptr<GameObject> actor = scene->CreateGameObject();
            bool removeSucceeded = true;
            auto remover = std::make_shared<RemoveDuringTickComponent>();
            remover->VictimId = victimId;
            remover->RemoveSucceeded = &removeSucceeded;
            actor->InsertRestoredComponent(remover, 0);

            scene->Tick(0.016f);
            if (removeSucceeded)
            {
                ME_LOG(LogTest, Error, "DeferredStructuralEdit: Remove during Tick should fail.");
                return false;
            }

            if (scene->FindGameObjectById(victimId) == nullptr)
            {
                ME_LOG(LogTest, Error, "DeferredStructuralEdit: victim should survive rejected Remove.");
                return false;
            }

            return true;
        }

        bool RunBeginPlayDestroyDefersEraseTest()
        {
            DeferredStructuralEditTestScope scope;

            const std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene("dse-bp-destroy");
            const std::shared_ptr<GameObject> go = scene->CreateGameObject();
            const uint64_t id = go->GetID();
            auto destroyOnBeginPlay = std::make_shared<DestroyOwnerOnBeginPlayComponent>();
            go->InsertRestoredComponent(destroyOnBeginPlay, 0);

            scene->DispatchBeginPlay();

            if (!go->IsPendingDestroy())
            {
                ME_LOG(LogTest, Error, "DeferredStructuralEdit: Destroy during BeginPlay did not mark pending.");
                return false;
            }

            if (scene->FindGameObjectById(id) == nullptr)
            {
                ME_LOG(LogTest, Error, "DeferredStructuralEdit: BeginPlay Destroy erased too early.");
                return false;
            }

            SceneManager::Get().FlushDeferredStructuralEdits();
            if (scene->FindGameObjectById(id) != nullptr)
            {
                ME_LOG(LogTest, Error, "DeferredStructuralEdit: Flush after BeginPlay Destroy left GO.");
                return false;
            }

            return true;
        }
    }

    bool RunDeferredStructuralEditTests()
    {
        return RunSpawnImmediateFindAndDeferTickTest()
            && RunSpawnBeginPlayImmediateTest()
            && RunDestroyDeferredFlushTest()
            && RunEofDirtySpreadsNextFrameTest()
            && RunRemoveDuringTickRejectedTest()
            && RunBeginPlayDestroyDefersEraseTest();
    }
}

#include "doctest.h"
#include "EngineTestFixture.h"

TEST_CASE("deferred-structural-edit: spawn destroy eof [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    CHECK(minEngine::RunDeferredStructuralEditTests());
}
