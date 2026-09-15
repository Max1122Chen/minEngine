#include "Runtime/Core/Delegates/Delegates.h"
#include "Access/ObjectManagerTestAccess.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Reflection/MEProperties.h"
#include "Runtime/Core/Reflection/ReflectionSample.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"

#include "doctest.h"

#include "EngineTestFixture.h"

namespace minEngine
{
    class DelegateObjectManagerScope
    {
    public:
        DelegateObjectManagerScope()
        {
            Testing::TestAccess<ObjectManager>::SetInstance(&m_Manager);
            m_Manager.Initialize();
        }

        ~DelegateObjectManagerScope()
        {
            m_Manager.Shutdown();
            Testing::TestAccess<ObjectManager>::SetInstance(nullptr);
        }

    private:
        ObjectManager m_Manager;
    };

    class RawListener
    {
    public:
        void OnZero() { ++ZeroCount; }
        void OnOne(int value)
        {
            ++OneCount;
            LastOneValue = value;
        }
        void OnTwo(int a, int b)
        {
            ++TwoCount;
            LastTwoSum = a + b;
        }

        int ZeroCount = 0;
        int OneCount = 0;
        int TwoCount = 0;
        int LastOneValue = 0;
        int LastTwoSum = 0;
    };

    class ReentrantListener
    {
    public:
        explicit ReentrantListener(MulticastDelegate<>& inDelegate)
            : m_Delegate(inDelegate)
        {
        }

        void OnFire()
        {
            ++FireCount;
            if (HandleToRemove.IsValid())
            {
                m_Delegate.Remove(HandleToRemove);
            }
        }

        int FireCount = 0;
        DelegateHandle HandleToRemove;

    private:
        MulticastDelegate<>& m_Delegate;
    };
}

TEST_CASE("delegates: multicast AddRaw Broadcast Remove [full]")
{
    using namespace minEngine;

    DECLARE_MULTICAST_DELEGATE_OneParam(FOnValue, int);
    FOnValue onValue;

    RawListener a;
    RawListener b;
    const DelegateHandle handleA = onValue.AddRaw(&a, &RawListener::OnOne);
    const DelegateHandle handleB = onValue.AddRaw(&b, &RawListener::OnOne);
    REQUIRE(handleA.IsValid());
    REQUIRE(handleB.IsValid());
    REQUIRE(onValue.IsBound());
    CHECK(onValue.GetBindingCount() == 2);

    onValue.Broadcast(7);
    CHECK(a.OneCount == 1);
    CHECK(b.OneCount == 1);
    CHECK(a.LastOneValue == 7);
    CHECK(b.LastOneValue == 7);

    onValue.Remove(handleA);
    onValue.Remove(handleA); // idempotent
    CHECK(onValue.GetBindingCount() == 1);

    onValue.Broadcast(11);
    CHECK(a.OneCount == 1);
    CHECK(b.OneCount == 2);
    CHECK(b.LastOneValue == 11);

    onValue.Clear();
    CHECK_FALSE(onValue.IsBound());
    onValue.Broadcast(99);
    CHECK(b.OneCount == 2);
}

TEST_CASE("delegates: AddLambda and two-param Broadcast [full]")
{
    using namespace minEngine;

    DECLARE_MULTICAST_DELEGATE(FOnZero);
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPair, int, int);

    FOnZero onZero;
    int zeroHits = 0;
    onZero.AddLambda([&zeroHits]() { ++zeroHits; });
    onZero.Broadcast();
    CHECK(zeroHits == 1);

    FOnPair onPair;
    RawListener listener;
    onPair.AddRaw(&listener, &RawListener::OnTwo);
    onPair.Broadcast(3, 4);
    CHECK(listener.TwoCount == 1);
    CHECK(listener.LastTwoSum == 7);
}

TEST_CASE("delegates: RemoveAll by instance [full]")
{
    using namespace minEngine;

    DECLARE_MULTICAST_DELEGATE(FOnZero);
    FOnZero onZero;

    RawListener listener;
    onZero.AddRaw(&listener, &RawListener::OnZero);
    onZero.AddRaw(&listener, &RawListener::OnZero);
    CHECK(onZero.GetBindingCount() == 2);

    onZero.RemoveAll(&listener);
    CHECK_FALSE(onZero.IsBound());

    onZero.Broadcast();
    CHECK(listener.ZeroCount == 0);
}

TEST_CASE("delegates: Broadcast snapshot allows Remove mid-invoke [full]")
{
    using namespace minEngine;

    MulticastDelegate<> onFire;
    ReentrantListener first(onFire);
    ReentrantListener second(onFire);

    const DelegateHandle handleFirst = onFire.AddRaw(&first, &ReentrantListener::OnFire);
    const DelegateHandle handleSecond = onFire.AddRaw(&second, &ReentrantListener::OnFire);
    first.HandleToRemove = handleSecond;

    onFire.Broadcast();

    // Snapshot: both fire even though first removed second mid-broadcast.
    CHECK(first.FireCount == 1);
    CHECK(second.FireCount == 1);
    CHECK(onFire.GetBindingCount() == 1);
    CHECK(handleFirst.IsValid());

    onFire.Broadcast();
    CHECK(first.FireCount == 2);
    CHECK(second.FireCount == 1);
}

TEST_CASE("delegates: AddMEObject skips after destroy [full]")
{
    using namespace minEngine;

    EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    DelegateObjectManagerScope scope;

    DECLARE_MULTICAST_DELEGATE(FOnReset);
    FOnReset onReset;

    minEngine::GUID componentGuid;
    {
        std::shared_ptr<Scene> scene = NewObject<Scene>("DelegateScene");
        std::shared_ptr<GameObject> gameObject = scene->CreateGameObject();
        std::shared_ptr<ReflectionSampleComponent> sample =
            gameObject->AddComponent<ReflectionSampleComponent>();
        componentGuid = sample->GetGuid();
        REQUIRE(componentGuid.IsValid());

        sample->SetFunctionTestCounter(9);
        const DelegateHandle handle = onReset.AddMEObject(sample.get(), &ReflectionSampleComponent::ResetCounter);
        REQUIRE(handle.IsValid());

        onReset.Broadcast();
        CHECK(sample->GetCounter() == 0);

        sample->SetFunctionTestCounter(3);
    }

    CHECK(FindObject(componentGuid) == nullptr);
    CHECK(onReset.GetBindingCount() == 1);

    // Must not crash; stale MEObject binding is skipped and compacted.
    onReset.Broadcast();
    CHECK(onReset.GetBindingCount() == 0);
}

TEST_CASE("delegates: dynamic AddScript mock Broadcast [full]")
{
    using namespace minEngine;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnScriptZero);
    FOnScriptZero onZero;

    int callCount = 0;
    CallableScriptFunction callable(
        &callCount,
        [](void* user, void* const* /*args*/, int arity)
        {
            CHECK(arity == 0);
            ++(*static_cast<int*>(user));
        },
        nullptr);

    const DelegateHandle handle = onZero.AddScript(std::move(callable));
    REQUIRE(handle.IsValid());
    CHECK(onZero.GetBindingCount() == 1);

    onZero.Broadcast();
    CHECK(callCount == 1);

    onZero.Broadcast();
    CHECK(callCount == 2);

    onZero.Remove(handle);
    onZero.Broadcast();
    CHECK(callCount == 2);
}

TEST_CASE("delegates: dynamic AddScript one-param [full]")
{
    using namespace minEngine;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnScriptOne, int);
    FOnScriptOne onOne;

    int lastValue = 0;
    CallableScriptFunction callable(
        &lastValue,
        [](void* user, void* const* args, int arity)
        {
            REQUIRE(arity == 1);
            REQUIRE(args != nullptr);
            *static_cast<int*>(user) = *static_cast<int*>(args[0]);
        },
        nullptr);

    REQUIRE(onOne.AddScript(std::move(callable)).IsValid());
    onOne.Broadcast(42);
    CHECK(lastValue == 42);
}

TEST_CASE("delegates: dynamic AddDynamic ResetCounter and destroy [full]")
{
    using namespace minEngine;

    EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    DelegateObjectManagerScope scope;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDynamicReset);
    FOnDynamicReset onReset;

    minEngine::GUID componentGuid;
    {
        std::shared_ptr<Scene> scene = NewObject<Scene>("DynamicDelegateScene");
        std::shared_ptr<GameObject> gameObject = scene->CreateGameObject();
        std::shared_ptr<ReflectionSampleComponent> sample =
            gameObject->AddComponent<ReflectionSampleComponent>();
        componentGuid = sample->GetGuid();
        REQUIRE(componentGuid.IsValid());

        sample->SetFunctionTestCounter(11);
        const DelegateHandle handle = ME_ADD_DYNAMIC(onReset, sample.get(), ResetCounter);
        REQUIRE(handle.IsValid());
        CHECK(onReset.GetBindingCount() == 1);

        onReset.Broadcast();
        CHECK(sample->GetCounter() == 0);

        sample->SetFunctionTestCounter(5);
        onReset.AddLambda(
            []()
            {
            });
        CHECK(onReset.GetBindingCount() == 2);
    }

    CHECK(FindObject(componentGuid) == nullptr);
    onReset.Broadcast();
    CHECK(onReset.IsBound());
}

TEST_CASE("delegates: dynamic AddDynamic missing function returns invalid [full]")
{
    using namespace minEngine;

    EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());

    DelegateObjectManagerScope scope;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMissing);
    FOnMissing onMissing;

    std::shared_ptr<Scene> scene = NewObject<Scene>("DynamicDelegateMissingScene");
    std::shared_ptr<GameObject> gameObject = scene->CreateGameObject();
    std::shared_ptr<ReflectionSampleComponent> sample =
        gameObject->AddComponent<ReflectionSampleComponent>();

    const DelegateHandle handle = onMissing.AddDynamic(sample.get(), "DoesNotExist");
    CHECK_FALSE(handle.IsValid());
    CHECK_FALSE(onMissing.IsBound());
}

TEST_CASE("delegates: MEDynamicMulticastDelegateProperty category [full]")
{
    using namespace minEngine;
    using namespace minEngine::Reflection;

    MEDynamicMulticastDelegateProperty property("OnEvent");
    property.SetArity(0);
    CHECK(property.GetCategory() == MEPropertyCategory::MulticastDelegate);
    CHECK(property.GetArity() == 0);

    property.SetAnnotations(
        static_cast<PropertySpecifierMask>(PropertySpecifier::ScriptAssignable),
        PropertyMetadata{});
    CHECK(property.HasSpecifier(PropertySpecifier::ScriptAssignable));

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEvent);
    FOnEvent storage;
    property.SetAccessors(
        [](const void* owner) -> const void*
        {
            return owner;
        },
        [](void* owner) -> void*
        {
            return owner;
        });
    CHECK(property.GetDelegateBase(static_cast<void*>(&storage)) == static_cast<DynamicMulticastDelegateBase*>(&storage));
    CHECK(property.GetDelegateBase(static_cast<const void*>(&storage))->GetArity() == 0);
}
