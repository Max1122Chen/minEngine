#include "GameplayTagTest.h"

#include "Runtime/Function/GameplayFramework/Tags/GameplayTagContainer.h"
#include "Runtime/Function/GameplayFramework/Tags/GameplayTagManager.h"
#include "Runtime/Function/GameplayFramework/Tags/NativeGameplayTags.h"

#include "doctest.h"

namespace
{
    ME_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Status_Debuff_Vulnerable, "Status.Debuff.Vulnerable");
    ME_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Status_Debuff_Heavy, "Status.Debuff.Heavy");
}

TEST_CASE("gameplay-tags: native macros and hierarchy matching [full]")
{
    using namespace minEngine;

    GameplayTagManager manager;
    GameplayTagManager::SetInstance(&manager);
    manager.Initialize();

    const GameplayTag channelDefault = TAG_Channel_Default;
    CHECK(channelDefault.IsValid());
    CHECK(channelDefault.GetName() == "Channel.Default");

    const GameplayTag vulnerable = TAG_Test_Status_Debuff_Vulnerable;
    const GameplayTag heavy = TAG_Test_Status_Debuff_Heavy;
    const GameplayTag debuff = manager.Resolve("Status.Debuff");
    const GameplayTag status = manager.Resolve("Status");

    CHECK(vulnerable.Matches(vulnerable));
    CHECK(vulnerable.Matches(debuff));
    CHECK(vulnerable.Matches(status));
    CHECK_FALSE(vulnerable.Matches(heavy));
    CHECK_FALSE(debuff.Matches(vulnerable));

    GameplayTagContainer container(manager);
    container.Add(vulnerable);
    CHECK(container.Has(vulnerable));
    CHECK(container.Has(debuff));
    CHECK(container.Has(status));
    CHECK_FALSE(container.Has(heavy));
    CHECK(container.GetCount(vulnerable) == 1);

    container.Add(vulnerable);
    CHECK(container.GetCount(vulnerable) == 2);
    CHECK(container.Remove(vulnerable));
    CHECK(container.GetCount(vulnerable) == 1);
    CHECK(container.Remove(vulnerable));
    CHECK(container.GetCount(vulnerable) == 0);
    CHECK_FALSE(container.Has(vulnerable));

    CHECK_FALSE(manager.TryResolve("Does.Not.Exist").IsValid());

    manager.Shutdown();
    GameplayTagManager::SetInstance(nullptr);
}
