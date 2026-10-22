#include "GameplayTagTest.h"

#include "EngineTestFixture.h"

#include "Runtime/Function/GameplayFramework/Tags/GameplayTagContainer.h"
#include "Runtime/Function/GameplayFramework/Tags/GameplayTagListProbe.h"
#include "Runtime/Function/GameplayFramework/Tags/GameplayTagManager.h"
#include "Runtime/Function/GameplayFramework/Tags/NativeGameplayTags.h"

#include "Runtime/Core/Serialization/Json.h"
#include "Runtime/Core/Serialization/Serializer.h"

#include "doctest.h"

#include <map>
#include <unordered_map>

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

    CHECK(vulnerable == manager.Resolve("Status.Debuff.Vulnerable"));
    const bool orderedByName = (vulnerable < heavy) != (heavy < vulnerable);
    CHECK(orderedByName);
    CHECK_FALSE(vulnerable < vulnerable);

    manager.Shutdown();
    GameplayTagManager::SetInstance(nullptr);
}

TEST_CASE("gameplay-tags: name identity works unbound and as map keys [full]")
{
    using namespace minEngine;

    GameplayTag left;
    GameplayTag right;
    left.SetTagName("Authoring.Unregistered.Tag");
    right.SetTagName("Authoring.Unregistered.Tag");
    CHECK_FALSE(left.IsValid());
    CHECK_FALSE(right.IsValid());
    CHECK(left == right);
    CHECK_FALSE(left.Matches(right));

    std::unordered_map<GameplayTag, int> byHash;
    byHash[left] = 7;
    CHECK(byHash[right] == 7);

    std::map<GameplayTag, int> byOrder;
    byOrder[left] = 3;
    CHECK(byOrder[right] == 3);

    GameplayTag emptyA;
    GameplayTag emptyB;
    CHECK(emptyA == emptyB);
    CHECK_FALSE(emptyA == left);
}

TEST_CASE("gameplay-tags: json roundtrip binds by SetTagName [full]")
{
    using namespace minEngine;
    using namespace minEngine::Serialization;

    EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    GameplayTagManager manager;
    GameplayTagManager::SetInstance(&manager);
    manager.Initialize();

    const GameplayTag source = manager.Resolve("Status.Debuff.Vulnerable");
    REQUIRE(source.IsValid());

    Json json;
    const SerializeResult writeResult =
        Serializer::SerializeObjectToJson("minEngine::GameplayTag", &source, json);
    REQUIRE(writeResult.ok);

    GameplayTag loaded;
    std::vector<PendingObjectRef> unresolvedRefs;
    const SerializeResult readResult =
        Serializer::DeserializeObjectFromJson("minEngine::GameplayTag", &loaded, json, unresolvedRefs);
    REQUIRE(readResult.ok);
    CHECK(unresolvedRefs.empty());

    CHECK(loaded.IsValid());
    CHECK(loaded == source);
    CHECK(loaded.Matches(manager.Resolve("Status.Debuff")));

    GameplayTag unknown;
    unknown.SetTagName("Does.Not.Exist");
    CHECK_FALSE(unknown.IsValid());
    CHECK(unknown.GetName() == "Does.Not.Exist");

    GameplayTagListProbe probe;
    probe.Tag = source;
    probe.Tags.push_back(source);
    probe.Tags.push_back(manager.Resolve("Status"));

    Json probeJson;
    REQUIRE(Serializer::SerializeObjectToJson("minEngine::GameplayTagListProbe", &probe, probeJson).ok);

    GameplayTagListProbe loadedProbe;
    std::vector<PendingObjectRef> probeRefs;
    REQUIRE(Serializer::DeserializeObjectFromJson(
                "minEngine::GameplayTagListProbe", &loadedProbe, probeJson, probeRefs)
                .ok);
    CHECK(probeRefs.empty());
    CHECK(loadedProbe.Tag == source);
    REQUIRE(loadedProbe.Tags.size() == 2);
    CHECK(loadedProbe.Tags[0].IsValid());
    CHECK(loadedProbe.Tags[0] == source);
    CHECK(loadedProbe.Tags[1].Matches(manager.Resolve("Status")));

    manager.Shutdown();
    GameplayTagManager::SetInstance(nullptr);
}
