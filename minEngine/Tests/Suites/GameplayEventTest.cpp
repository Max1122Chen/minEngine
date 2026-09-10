#include "GameplayEventTest.h"

#include "Access/ObjectManagerTestAccess.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/GameplayFramework/Events/GameplayEvent.h"
#include "Runtime/Function/GameplayFramework/Events/GameplayEventSystemComponent.h"
#include "Runtime/Function/GameplayFramework/Tags/GameplayTagContainer.h"
#include "Runtime/Function/GameplayFramework/Tags/GameplayTagManager.h"
#include "Runtime/Function/GameplayFramework/Tags/NativeGameplayTags.h"

#include "doctest.h"

#include "EngineTestFixture.h"

namespace
{
    ME_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Event_Damage, "GameplayEvent.Combat.Damage");
    ME_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Event_Heal, "GameplayEvent.Combat.Heal");
}

namespace minEngine
{
    class GameplayEventTestScope
    {
    public:
        GameplayEventTestScope()
        {
            Testing::TestAccess<ObjectManager>::SetInstance(&m_ObjectManager);
            m_ObjectManager.Initialize();

            GameplayTagManager::SetInstance(&m_TagManager);
            m_TagManager.Initialize();
        }

        ~GameplayEventTestScope()
        {
            m_TagManager.Shutdown();
            GameplayTagManager::SetInstance(nullptr);

            m_ObjectManager.Shutdown();
            Testing::TestAccess<ObjectManager>::SetInstance(nullptr);
        }

    private:
        ObjectManager m_ObjectManager;
        GameplayTagManager m_TagManager;
    };
}

TEST_CASE("gameplay-events: subscribe filter priority and scene isolation [full]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());

    GameplayEventTestScope scope;

    GameplayTagManager& tags = GameplayTagManager::Get();
    const GameplayTag damageTag = TAG_Test_Event_Damage;
    const GameplayTag healTag = TAG_Test_Event_Heal;
    const GameplayTag combatTag = tags.Resolve("GameplayEvent.Combat");

    std::shared_ptr<Scene> sceneA = NewObject<Scene>("event-scene-a");
    std::shared_ptr<GameObject> goA = sceneA->CreateGameObject();
    std::shared_ptr<GameplayEventSystemComponent> busA = goA->AddComponent<GameplayEventSystemComponent>();
    REQUIRE(busA != nullptr);
    REQUIRE(sceneA->GetGameplayEventSystem() == busA.get());

    std::shared_ptr<Scene> sceneB = NewObject<Scene>("event-scene-b");
    std::shared_ptr<GameObject> goB = sceneB->CreateGameObject();
    std::shared_ptr<GameplayEventSystemComponent> busB = goB->AddComponent<GameplayEventSystemComponent>();
    REQUIRE(busB != nullptr);
    REQUIRE(sceneB->GetGameplayEventSystem() == busB.get());

    int damageHits = 0;
    int healHits = 0;
    int sceneBHits = 0;
    int highPriorityOrder = -1;
    int lowPriorityOrder = -1;
    int callOrder = 0;

    GameplayEventSubscribeOptions damageOpts;
    damageOpts.Channel = busA->GetDefaultChannel();
    damageOpts.RequiredAny = {damageTag};
    damageOpts.Priority = 0;
    damageOpts.Handler = [&](const GameplayEvent&) { ++damageHits; };
    const std::string damageListener = busA->Subscribe(damageOpts);
    REQUIRE_FALSE(damageListener.empty());

    GameplayEventSubscribeOptions healOpts;
    healOpts.Channel = busA->GetDefaultChannel();
    healOpts.RequiredAll = {healTag};
    healOpts.Handler = [&](const GameplayEvent&) { ++healHits; };
    busA->Subscribe(healOpts);

    GameplayEventSubscribeOptions highOpts;
    highOpts.Channel = busA->GetDefaultChannel();
    highOpts.RequiredAny = {combatTag};
    highOpts.Priority = 10;
    highOpts.Handler = [&](const GameplayEvent&) {
        highPriorityOrder = callOrder++;
    };
    busA->Subscribe(highOpts);

    GameplayEventSubscribeOptions lowOpts;
    lowOpts.Channel = busA->GetDefaultChannel();
    lowOpts.RequiredAny = {combatTag};
    lowOpts.Priority = 1;
    lowOpts.Handler = [&](const GameplayEvent&) {
        lowPriorityOrder = callOrder++;
    };
    busA->Subscribe(lowOpts);

    GameplayEventSubscribeOptions sceneBOpts;
    sceneBOpts.Channel = busB->GetDefaultChannel();
    sceneBOpts.RequiredAny = {damageTag};
    sceneBOpts.Handler = [&](const GameplayEvent&) { ++sceneBHits; };
    busB->Subscribe(sceneBOpts);

    GameplayEvent damageEvent(tags);
    damageEvent.Tags.Add(damageTag);
    busA->Dispatch(damageEvent);

    CHECK(damageHits == 1);
    CHECK(healHits == 0);
    CHECK(sceneBHits == 0);
    CHECK(highPriorityOrder == 0);
    CHECK(lowPriorityOrder == 1);

    GameplayEvent healEvent(tags);
    healEvent.Tags.Add(healTag);
    busA->Dispatch(healEvent);
    CHECK(healHits == 1);
    CHECK(damageHits == 1);

    busA->Unsubscribe(damageListener);

    int reentrantHits = 0;
    GameplayEventSubscribeOptions reentrantOpts;
    reentrantOpts.Channel = busA->GetDefaultChannel();
    reentrantOpts.ListenerId = "reentrant";
    reentrantOpts.RequiredAny = {damageTag};
    reentrantOpts.Handler = [&](const GameplayEvent&) {
        ++reentrantHits;
        if (reentrantHits == 1)
        {
            GameplayEvent nested(tags);
            nested.Tags.Add(damageTag);
            busA->Dispatch(nested);
        }
    };
    busA->Subscribe(reentrantOpts);

    busA->SetMaxDispatchDepth(2);
    reentrantHits = 0;
    busA->Dispatch(damageEvent);
    CHECK(reentrantHits == 2);
}

TEST_CASE("gameplay-events: default channel tag [full]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());

    GameplayEventTestScope scope;

    std::shared_ptr<Scene> scene = NewObject<Scene>("event-default-channel");
    std::shared_ptr<GameObject> go = scene->CreateGameObject();
    std::shared_ptr<GameplayEventSystemComponent> bus = go->AddComponent<GameplayEventSystemComponent>();
    REQUIRE(bus != nullptr);

    const GameplayEventChannel channel = bus->GetDefaultChannel();
    CHECK(channel.Tag.GetName() == "Channel.Default");
    CHECK(scene->GetGameplayEventSystem() == bus.get());
}
