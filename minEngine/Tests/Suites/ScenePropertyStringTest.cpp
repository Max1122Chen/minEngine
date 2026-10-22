#include "ScenePropertyStringTest.h"

#include "Access/ObjectManagerTestAccess.h"
#include "Access/SceneManagerTestAccess.h"
#include "EngineTestFixture.h"

#include "Services/Scene/ScenePropertyValueCodec.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Reflection/PropertyAssign.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Core/Serialization/Json.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"

#include "doctest.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace
{
    class ScopedManagers
    {
    public:
        ScopedManagers()
        {
            minEngine::Testing::TestAccess<minEngine::ObjectManager>::SetInstance(&m_ObjectManager);
            minEngine::ObjectManager::Initialize();
            minEngine::Testing::TestAccess<minEngine::SceneManager>::SetInstance(&m_SceneManager);
            m_SceneManager.Initialize();
        }

        ~ScopedManagers()
        {
            m_SceneManager.Shutdown();
            minEngine::Testing::TestAccess<minEngine::SceneManager>::SetInstance(nullptr);
            minEngine::ObjectManager::Shutdown();
            minEngine::Testing::TestAccess<minEngine::ObjectManager>::SetInstance(nullptr);
        }

    private:
        minEngine::ObjectManager m_ObjectManager;
        minEngine::SceneManager m_SceneManager;
    };

    const minEngine::Reflection::MEProperty* FindPropertyInHierarchy(
        const minEngine::Reflection::MEClass* ownerClass,
        const std::string& propertyName)
    {
        const minEngine::Reflection::MEProperty* found = nullptr;
        minEngine::Reflection::ReflectionSystem::Get().ForEachPropertyInHierarchy(
            ownerClass->GetName(),
            [&](const minEngine::Reflection::MEProperty& candidate) -> bool
            {
                if (candidate.GetName() == propertyName)
                {
                    found = &candidate;
                    return false;
                }
                return true;
            });
        return found;
    }
}

TEST_CASE("scene-property-string: empty string write via blob pipeline does not crash [smoke]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedManagers managers;

    const std::shared_ptr<Scene> scene =
        SceneManager::Get().CreateNewScene("scene-property-string-empty");
    REQUIRE(scene != nullptr);
    const std::shared_ptr<GameObject> gameObject = scene->CreateGameObject();
    REQUIRE(gameObject != nullptr);
    gameObject->Rename("BeforeEmpty");
    REQUIRE(gameObject->GetName() == "BeforeEmpty");

    const Reflection::MEClass* ownerClass = gameObject->GetClass();
    REQUIRE(ownerClass != nullptr);

    // Same stages as automation WritePropertyValue: JSON to live, capture after blob,
    // restore before, then apply after. Never copy std::string through raw GetProperty
    // into an unconstructed byte buffer.
    const Serialization::SerializerOptions options{};
    std::string codecError;
    std::vector<uint8_t> beforeBlob;
    REQUIRE(ScenePropertyValueCodec::TryCaptureBlob(
        *gameObject, *ownerClass, "m_Name", beforeBlob, codecError, options));

    REQUIRE(ScenePropertyValueCodec::TryWriteJsonToLive(
        *gameObject, *ownerClass, "m_Name", Json(""), codecError, options));
    CHECK(gameObject->GetName().empty());

    std::vector<uint8_t> afterBlob;
    REQUIRE(ScenePropertyValueCodec::TryCaptureBlob(
        *gameObject, *ownerClass, "m_Name", afterBlob, codecError, options));
    CHECK(beforeBlob != afterBlob);

    REQUIRE(ScenePropertyValueCodec::TryRestoreBlob(
        *gameObject, *ownerClass, "m_Name", beforeBlob, codecError, options));
    CHECK(gameObject->GetName() == "BeforeEmpty");

    REQUIRE(ScenePropertyValueCodec::TryRestoreBlob(
        *gameObject, *ownerClass, "m_Name", afterBlob, codecError, options));
    CHECK(gameObject->GetName().empty());
}

TEST_CASE("scene-property-string: empty-to-empty blob write is unchanged [smoke]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedManagers managers;

    const std::shared_ptr<Scene> scene =
        SceneManager::Get().CreateNewScene("scene-property-string-unchanged");
    REQUIRE(scene != nullptr);
    const std::shared_ptr<GameObject> gameObject = scene->CreateGameObject();
    REQUIRE(gameObject != nullptr);
    gameObject->Rename("");
    REQUIRE(gameObject->GetName().empty());

    const Reflection::MEClass* ownerClass = gameObject->GetClass();
    REQUIRE(ownerClass != nullptr);

    const Serialization::SerializerOptions options{};
    std::string codecError;
    std::vector<uint8_t> beforeBlob;
    REQUIRE(ScenePropertyValueCodec::TryCaptureBlob(
        *gameObject, *ownerClass, "m_Name", beforeBlob, codecError, options));

    REQUIRE(ScenePropertyValueCodec::TryWriteJsonToLive(
        *gameObject, *ownerClass, "m_Name", Json(""), codecError, options));

    std::vector<uint8_t> afterBlob;
    REQUIRE(ScenePropertyValueCodec::TryCaptureBlob(
        *gameObject, *ownerClass, "m_Name", afterBlob, codecError, options));
    CHECK(beforeBlob == afterBlob);

    REQUIRE(ScenePropertyValueCodec::TryRestoreBlob(
        *gameObject, *ownerClass, "m_Name", beforeBlob, codecError, options));
    CHECK(gameObject->GetName().empty());
}

TEST_CASE("scene-property-string: constructed temp buffer can assign empty string [smoke]")
{
    using namespace minEngine;

    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedManagers managers;

    const std::shared_ptr<Scene> scene =
        SceneManager::Get().CreateNewScene("scene-property-string-construct");
    REQUIRE(scene != nullptr);
    const std::shared_ptr<GameObject> gameObject = scene->CreateGameObject();
    REQUIRE(gameObject != nullptr);
    gameObject->Rename("Keep");

    const Reflection::MEClass* ownerClass = gameObject->GetClass();
    REQUIRE(ownerClass != nullptr);
    const Reflection::MEProperty* nameProperty = FindPropertyInHierarchy(ownerClass, "m_Name");
    REQUIRE(nameProperty != nullptr);
    REQUIRE(nameProperty->GetValueConstructFn() != nullptr);
    REQUIRE(nameProperty->GetValueCopyAssignFn() != nullptr);

    // Inspector-style temporary: construct first, then Get/Assign. Raw zeroed storage is UB.
    std::vector<uint8_t> editBuffer(nameProperty->GetStorageSize());
    nameProperty->ConstructValue(editBuffer.data());

    struct ScopedDestruct
    {
        const Reflection::MEProperty* Property = nullptr;
        void* Storage = nullptr;
        ~ScopedDestruct()
        {
            if (Property != nullptr && Storage != nullptr && Property->GetValueDestructFn() != nullptr)
            {
                Property->DestructValue(Storage);
            }
        }
    } scoped{nameProperty, editBuffer.data()};

    REQUIRE(Reflection::GetPropertyValue(gameObject.get(), *nameProperty, editBuffer.data()));
    CHECK(*static_cast<const std::string*>(static_cast<const void*>(editBuffer.data())) == "Keep");

    *static_cast<std::string*>(static_cast<void*>(editBuffer.data())) = "";
    REQUIRE(Reflection::AssignProperty(gameObject.get(), *nameProperty, editBuffer.data()));
    CHECK(gameObject->GetName().empty());
}
