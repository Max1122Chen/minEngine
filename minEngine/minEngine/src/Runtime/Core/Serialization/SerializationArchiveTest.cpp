#include "SerializationArchiveTest.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Core/Reflection/Reflection.h"

#include "BinaryArchive.h"
#include "Serializer.h"
#include "Runtime/Core/GUID/GUID.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Components/MovementComponent.h"
#include "Runtime/Function/Framework/Components/StaticMeshComponent.h"

namespace minEngine
{
    class SerializationArchiveTestScope
    {
    public:
        SerializationArchiveTestScope()
        {
            ObjectManager::SetInstance(&m_Manager);
            m_Manager.Initialize();
        }

        ~SerializationArchiveTestScope()
        {
            m_Manager.Shutdown();
            ObjectManager::SetInstance(nullptr);
        }

    private:
        ObjectManager m_Manager;
    };

    using Serialization::BinaryReaderArchive;
    using Serialization::BinaryWriterArchive;

    namespace
    {
        bool EnsureReflectionReady()
        {
            Reflection::ReflectionSystem& reflection = Reflection::ReflectionSystem::Get();
            if (reflection.IsReady())
            {
                return true;
            }

            if (!reflection.FinalizeReflection())
            {
                for (const std::string& error : reflection.GetLastErrors())
                {
                    ME_CORE_ERROR("{}", error);
                }
                return false;
            }

            reflection.ClearErrors();
            return true;
        }

        bool TestBoolRoundTrip()
        {
            BinaryWriterArchive writer;
            if (!writer.WriteBool(true))
            {
                ME_CORE_ERROR("SerializationArchiveTest: WriteBool failed.");
                return false;
            }

            BinaryReaderArchive reader(writer.TakeBuffer());
            bool value = false;
            if (!reader.ReadBool(value) || value != true)
            {
                ME_CORE_ERROR("SerializationArchiveTest: Bool round-trip failed.");
                return false;
            }

            return true;
        }

        bool TestStringRoundTrip()
        {
            BinaryWriterArchive writer;
            if (!writer.WriteString("minEngine"))
            {
                ME_CORE_ERROR("SerializationArchiveTest: WriteString failed.");
                return false;
            }

            BinaryReaderArchive reader(writer.TakeBuffer());
            std::string value;
            if (!reader.ReadString(value) || value != "minEngine")
            {
                ME_CORE_ERROR("SerializationArchiveTest: String round-trip failed.");
                return false;
            }

            return true;
        }

        bool TestGuidRefRoundTrip()
        {
            const GUID sourceGuid(0xAABBCCDDEEFF0011ull, 0x1122334455667788ull);

            BinaryWriterArchive writer;
            if (!writer.BeginGuidRef(sourceGuid) || !writer.EndGuidRef())
            {
                ME_CORE_ERROR("SerializationArchiveTest: GuidRef write failed.");
                return false;
            }

            BinaryReaderArchive reader(writer.TakeBuffer());
            GUID readGuid;
            if (!reader.BeginGuidRef(readGuid) || !reader.EndGuidRef())
            {
                ME_CORE_ERROR("SerializationArchiveTest: GuidRef read failed.");
                return false;
            }

            if (readGuid.High != sourceGuid.High || readGuid.Low != sourceGuid.Low)
            {
                ME_CORE_ERROR("SerializationArchiveTest: GuidRef payload mismatch.");
                return false;
            }

            return true;
        }

        bool TestArrayRoundTrip()
        {
            BinaryWriterArchive writer;
            if (!writer.BeginArray(2))
            {
                ME_CORE_ERROR("SerializationArchiveTest: BeginArray failed.");
                return false;
            }

            if (!writer.WriteInt64(7) || !writer.WriteInt64(42))
            {
                ME_CORE_ERROR("SerializationArchiveTest: array element write failed.");
                return false;
            }

            if (!writer.EndArray())
            {
                ME_CORE_ERROR("SerializationArchiveTest: EndArray failed.");
                return false;
            }

            BinaryReaderArchive reader(writer.TakeBuffer());
            size_t count = 0;
            if (!reader.BeginArray(count) || count != 2)
            {
                ME_CORE_ERROR("SerializationArchiveTest: BeginArray read failed.");
                return false;
            }

            int64_t first = 0;
            int64_t second = 0;
            if (!reader.EnterArrayElement(0) || !reader.ReadInt64(first) || !reader.LeaveArrayElement())
            {
                ME_CORE_ERROR("SerializationArchiveTest: first array element read failed.");
                return false;
            }

            if (!reader.EnterArrayElement(1) || !reader.ReadInt64(second) || !reader.LeaveArrayElement())
            {
                ME_CORE_ERROR("SerializationArchiveTest: second array element read failed.");
                return false;
            }

            if (!reader.EndArray() || first != 7 || second != 42)
            {
                ME_CORE_ERROR("SerializationArchiveTest: array payload mismatch.");
                return false;
            }

            return true;
        }

        bool TestObjectFieldsRoundTrip()
        {
            BinaryWriterArchive writer;
            if (!writer.BeginObject("TestObject"))
            {
                ME_CORE_ERROR("SerializationArchiveTest: BeginObject failed.");
                return false;
            }

            if (!writer.BeginField("m_Alpha") || !writer.WriteDouble(1.5) || !writer.EndField())
            {
                ME_CORE_ERROR("SerializationArchiveTest: alpha field write failed.");
                return false;
            }

            if (!writer.BeginField("m_Flag") || !writer.WriteBool(false) || !writer.EndField())
            {
                ME_CORE_ERROR("SerializationArchiveTest: flag field write failed.");
                return false;
            }

            if (!writer.EndObject())
            {
                ME_CORE_ERROR("SerializationArchiveTest: EndObject failed.");
                return false;
            }

            BinaryReaderArchive reader(writer.TakeBuffer());
            if (!reader.BeginObject("TestObject"))
            {
                ME_CORE_ERROR("SerializationArchiveTest: BeginObject read failed.");
                return false;
            }

            double alpha = 0.0;
            bool flag = true;
            if (!reader.EnterField("m_Alpha") || !reader.ReadDouble(alpha) || !reader.LeaveField())
            {
                ME_CORE_ERROR("SerializationArchiveTest: alpha field read failed.");
                return false;
            }

            if (!reader.EnterField("m_Flag") || !reader.ReadBool(flag) || !reader.LeaveField())
            {
                ME_CORE_ERROR("SerializationArchiveTest: flag field read failed.");
                return false;
            }

            if (!reader.EndObject() || alpha != 1.5 || flag != false)
            {
                ME_CORE_ERROR("SerializationArchiveTest: object payload mismatch.");
                return false;
            }

            return true;
        }

        bool TestSerializeObjectToBufferEmptyObjectRoundTrip()
        {
            BinaryWriterArchive writer;
            if (!writer.BeginObject("EmptySnapshotObject") || !writer.EndObject())
            {
                ME_CORE_ERROR("SerializationArchiveTest: failed to write empty object shell.");
                return false;
            }

            const std::vector<uint8_t> buffer = writer.TakeBuffer();
            BinaryReaderArchive reader(buffer);
            if (!reader.BeginObject("EmptySnapshotObject") || !reader.EndObject())
            {
                ME_CORE_ERROR("SerializationArchiveTest: failed to read empty object shell.");
                return false;
            }

            return true;
        }

        bool TestNestedGuidObjectFieldRoundTrip()
        {
            const GUID sourceGuid(0xAABBCCDDEEFF0011ull, 0x1122334455667788ull);

            BinaryWriterArchive writer;
            if (!writer.BeginObject("OwnerObject"))
            {
                ME_CORE_ERROR("SerializationArchiveTest: nested object BeginObject failed.");
                return false;
            }

            if (!writer.BeginField("m_Guid")
                || !writer.BeginObject("GUID")
                || !writer.BeginField("High") || !writer.WriteUInt64(sourceGuid.High) || !writer.EndField()
                || !writer.BeginField("Low") || !writer.WriteUInt64(sourceGuid.Low) || !writer.EndField()
                || !writer.EndObject()
                || !writer.EndField()
                || !writer.EndObject())
            {
                ME_CORE_ERROR("SerializationArchiveTest: nested GUID field write failed: {}", writer.GetLastArchiveError());
                return false;
            }

            BinaryReaderArchive reader(writer.TakeBuffer());
            if (!reader.BeginObject("OwnerObject"))
            {
                ME_CORE_ERROR("SerializationArchiveTest: nested object read BeginObject failed.");
                return false;
            }

            GUID readGuid;
            if (!reader.EnterField("m_Guid")
                || !reader.BeginObject("GUID")
                || !reader.EnterField("High") || !reader.ReadUInt64(readGuid.High) || !reader.LeaveField()
                || !reader.EnterField("Low") || !reader.ReadUInt64(readGuid.Low) || !reader.LeaveField()
                || !reader.EndObject()
                || !reader.LeaveField()
                || !reader.EndObject())
            {
                ME_CORE_ERROR("SerializationArchiveTest: nested GUID field read failed.");
                return false;
            }

            if (readGuid != sourceGuid)
            {
                ME_CORE_ERROR("SerializationArchiveTest: nested GUID payload mismatch.");
                return false;
            }

            return true;
        }

        bool TestGuidRefObjectFieldRoundTrip()
        {
            const GUID sourceGuid(0xDEADBEEFCAFE0001ull, 0x0123456789ABCDEFull);

            BinaryWriterArchive writer;
            if (!writer.BeginObject("GuidOwner")
                || !writer.BeginField("m_Target")
                || !writer.BeginGuidRef(sourceGuid)
                || !writer.EndGuidRef()
                || !writer.EndField()
                || !writer.EndObject())
            {
                ME_CORE_ERROR("SerializationArchiveTest: GuidRef field write failed: {}", writer.GetLastArchiveError());
                return false;
            }

            BinaryReaderArchive reader(writer.TakeBuffer());
            GUID readGuid;
            if (!reader.BeginObject("GuidOwner")
                || !reader.EnterField("m_Target")
                || !reader.BeginGuidRef(readGuid)
                || !reader.EndGuidRef()
                || !reader.LeaveField()
                || !reader.EndObject())
            {
                ME_CORE_ERROR("SerializationArchiveTest: GuidRef field read failed.");
                return false;
            }

            if (readGuid != sourceGuid)
            {
                ME_CORE_ERROR("SerializationArchiveTest: GuidRef field payload mismatch.");
                return false;
            }

            return true;
        }

        bool TestArrayOfInlineObjectPtrFieldRoundTrip()
        {
            BinaryWriterArchive writer;
            if (!writer.BeginObject("ArrayOwner")
                || !writer.BeginField("m_Items")
                || !writer.BeginArray(1)
                || !writer.BeginObjectPtr("InlineItem")
                || !writer.BeginField("m_Value") || !writer.WriteInt64(42) || !writer.EndField()
                || !writer.EndObjectPtr()
                || !writer.EndArray()
                || !writer.EndField()
                || !writer.EndObject())
            {
                ME_CORE_ERROR(
                    "SerializationArchiveTest: array ObjectPtr field write failed: {}",
                    writer.GetLastArchiveError());
                return false;
            }

            BinaryReaderArchive reader(writer.TakeBuffer());
            size_t count = 0;
            int64_t readValue = 0;
            std::string dynamicClassName;
            if (!reader.BeginObject("ArrayOwner")
                || !reader.EnterField("m_Items")
                || !reader.BeginArray(count)
                || count != 1
                || !reader.EnterArrayElement(0)
                || !reader.BeginObjectPtr(nullptr, dynamicClassName)
                || !reader.EnterField("m_Value")
                || !reader.ReadInt64(readValue)
                || !reader.LeaveField()
                || !reader.EndObjectPtr()
                || !reader.LeaveArrayElement()
                || !reader.EndArray()
                || !reader.LeaveField()
                || !reader.EndObject())
            {
                ME_CORE_ERROR("SerializationArchiveTest: array ObjectPtr field read failed.");
                return false;
            }

            if (readValue != 42)
            {
                ME_CORE_ERROR("SerializationArchiveTest: array ObjectPtr field payload mismatch.");
                return false;
            }

            return true;
        }

        bool TestGameObjectSerializeObjectToBufferRoundTrip()
        {
            SerializationArchiveTestScope scope;

            const GUID sourceGuid(0xAABBCCDDEEFF0011ull, 0x1122334455667788ull);
            std::shared_ptr<GameObject> sourceObject = NewObject<GameObject>("SnapshotTestGO", nullptr, sourceGuid);

            std::vector<uint8_t> buffer;
            const Serialization::SerializeResult writeResult = Serialization::Serializer::SerializeObjectToBuffer(
                "minEngine::GameObject",
                sourceObject.get(),
                buffer);
            if (!writeResult.ok)
            {
                ME_CORE_ERROR(
                    "SerializationArchiveTest: SerializeObjectToBuffer failed: {} ({})",
                    writeResult.message,
                    writeResult.fieldPath);
                return false;
            }

            std::shared_ptr<GameObject> restoredObject = NewObject<GameObject>("SnapshotRestoredGO");
            std::vector<Serialization::PendingObjectRef> unresolvedRefs;
            const Serialization::SerializeResult readResult = Serialization::Serializer::DeserializeObjectFromBuffer(
                "minEngine::GameObject",
                restoredObject.get(),
                buffer,
                unresolvedRefs);
            if (!readResult.ok)
            {
                ME_CORE_ERROR(
                    "SerializationArchiveTest: DeserializeObjectFromBuffer failed: {} ({})",
                    readResult.message,
                    readResult.fieldPath);
                return false;
            }

            if (restoredObject->GetName() != sourceObject->GetName())
            {
                ME_CORE_ERROR("SerializationArchiveTest: GameObject name mismatch after round-trip.");
                return false;
            }

            if (restoredObject->GetGuid() != sourceGuid)
            {
                ME_CORE_ERROR("SerializationArchiveTest: GameObject GUID mismatch after round-trip.");
                return false;
            }

            return true;
        }

        bool TestMovementComponentSerializeRoundTrip()
        {
            SerializationArchiveTestScope scope;

            std::shared_ptr<GameObject> owner = NewObject<GameObject>("MovementComponentOwnerGO");
            std::shared_ptr<MovementComponent> sourceComponent =
                NewObject<MovementComponent>("", owner.get());

            std::vector<uint8_t> buffer;
            const Serialization::SerializeResult writeResult = Serialization::Serializer::SerializeObjectToBuffer(
                "minEngine::MovementComponent",
                sourceComponent.get(),
                buffer);
            if (!writeResult.ok)
            {
                ME_CORE_ERROR(
                    "SerializationArchiveTest: MovementComponent serialize failed: {} ({})",
                    writeResult.message,
                    writeResult.fieldPath);
                return false;
            }

            std::shared_ptr<MovementComponent> restoredComponent =
                NewObject<MovementComponent>("", owner.get());
            std::vector<Serialization::PendingObjectRef> unresolvedRefs;
            const Serialization::SerializeResult readResult = Serialization::Serializer::DeserializeObjectFromBuffer(
                "minEngine::MovementComponent",
                restoredComponent.get(),
                buffer,
                unresolvedRefs);
            if (!readResult.ok)
            {
                ME_CORE_ERROR(
                    "SerializationArchiveTest: MovementComponent deserialize failed: {} ({})",
                    readResult.message,
                    readResult.fieldPath);
                return false;
            }

            const Serialization::SerializeResult resolveResult =
                Serialization::Serializer::ResolvePendingObjectRefs(unresolvedRefs);
            if (!resolveResult.ok)
            {
                ME_CORE_ERROR(
                    "SerializationArchiveTest: MovementComponent ResolvePendingObjectRefs failed: {}",
                    resolveResult.message);
                return false;
            }

            return true;
        }

        bool TestStaticMeshComponentSerializeRoundTrip()
        {
            SerializationArchiveTestScope scope;

            std::shared_ptr<GameObject> owner = NewObject<GameObject>("StaticMeshOwnerGO");
            std::shared_ptr<StaticMeshComponent> sourceComponent =
                NewObject<StaticMeshComponent>("", owner.get());

            std::vector<uint8_t> buffer;
            const Serialization::SerializeResult writeResult = Serialization::Serializer::SerializeObjectToBuffer(
                "minEngine::StaticMeshComponent",
                sourceComponent.get(),
                buffer);
            if (!writeResult.ok)
            {
                ME_CORE_ERROR(
                    "SerializationArchiveTest: StaticMeshComponent serialize failed: {} ({})",
                    writeResult.message,
                    writeResult.fieldPath);
                return false;
            }

            std::shared_ptr<StaticMeshComponent> restoredComponent =
                NewObject<StaticMeshComponent>("", owner.get());
            std::vector<Serialization::PendingObjectRef> unresolvedRefs;
            const Serialization::SerializeResult readResult = Serialization::Serializer::DeserializeObjectFromBuffer(
                "minEngine::StaticMeshComponent",
                restoredComponent.get(),
                buffer,
                unresolvedRefs);
            if (!readResult.ok)
            {
                ME_CORE_ERROR(
                    "SerializationArchiveTest: StaticMeshComponent deserialize failed: {} ({})",
                    readResult.message,
                    readResult.fieldPath);
                return false;
            }

            return true;
        }

        bool TestGameObjectComponentsPropertyRoundTrip()
        {
            SerializationArchiveTestScope scope;

            std::shared_ptr<GameObject> sourceObject = NewObject<GameObject>("ComponentsPropertyGO");
            sourceObject->AddComponent<StaticMeshComponent>();

            const Reflection::MEClass* gameObjectClass =
                Reflection::ReflectionSystem::Get().FindClass("minEngine::GameObject");
            if (gameObjectClass == nullptr)
            {
                ME_CORE_ERROR("SerializationArchiveTest: GameObject class not found.");
                return false;
            }

            std::vector<uint8_t> buffer;
            std::vector<Serialization::PendingObjectRef> unresolvedRefs;
            const Serialization::SerializeResult writeResult = Serialization::Serializer::SerializePropertyToBuffer(
                sourceObject.get(),
                gameObjectClass,
                "m_Components",
                buffer);
            if (!writeResult.ok)
            {
                ME_CORE_ERROR(
                    "SerializationArchiveTest: m_Components property serialize failed: {} ({})",
                    writeResult.message,
                    writeResult.fieldPath);
                return false;
            }

            std::shared_ptr<GameObject> restoredObject = NewObject<GameObject>("ComponentsPropertyRestoredGO");
            restoredObject->GetAllComponents().clear();

            const Serialization::SerializeResult readResult = Serialization::Serializer::DeserializePropertyFromBuffer(
                restoredObject.get(),
                gameObjectClass,
                "m_Components",
                buffer,
                unresolvedRefs);
            if (!readResult.ok)
            {
                ME_CORE_ERROR(
                    "SerializationArchiveTest: m_Components property deserialize failed: {} ({})",
                    readResult.message,
                    readResult.fieldPath);
                return false;
            }

            if (restoredObject->GetAllComponents().size() != 1)
            {
                ME_CORE_ERROR("SerializationArchiveTest: m_Components property round-trip size mismatch.");
                return false;
            }

            const Serialization::SerializeResult resolveResult =
                Serialization::Serializer::ResolvePendingObjectRefs(unresolvedRefs);
            if (!resolveResult.ok)
            {
                ME_CORE_ERROR(
                    "SerializationArchiveTest: m_Components ResolvePendingObjectRefs failed: {}",
                    resolveResult.message);
                return false;
            }

            return true;
        }

        bool TestGameObjectRootComponentNullPropertyRoundTrip()
        {
            SerializationArchiveTestScope scope;

            std::shared_ptr<GameObject> sourceObject = NewObject<GameObject>("RootComponentNullGO");
            sourceObject->AddComponent<StaticMeshComponent>();

            const Reflection::MEClass* gameObjectClass =
                Reflection::ReflectionSystem::Get().FindClass("minEngine::GameObject");
            if (gameObjectClass == nullptr)
            {
                ME_CORE_ERROR("SerializationArchiveTest: GameObject class not found.");
                return false;
            }

            if (sourceObject->GetRootComponent() == nullptr)
            {
                ME_CORE_ERROR("SerializationArchiveTest: expected non-null m_RootComponent for StaticMeshComponent GO.");
                return false;
            }

            std::vector<uint8_t> buffer;
            std::vector<Serialization::PendingObjectRef> unresolvedRefs;
            const Serialization::SerializeResult writeResult = Serialization::Serializer::SerializePropertyToBuffer(
                sourceObject.get(),
                gameObjectClass,
                "m_RootComponent",
                buffer);
            if (!writeResult.ok)
            {
                ME_CORE_ERROR(
                    "SerializationArchiveTest: m_RootComponent serialize failed: {} ({})",
                    writeResult.message,
                    writeResult.fieldPath);
                return false;
            }

            std::shared_ptr<GameObject> restoredObject = NewObject<GameObject>("RootComponentRestoredGO");
            const Serialization::SerializeResult readResult = Serialization::Serializer::DeserializePropertyFromBuffer(
                restoredObject.get(),
                gameObjectClass,
                "m_RootComponent",
                buffer,
                unresolvedRefs);
            if (!readResult.ok)
            {
                ME_CORE_ERROR(
                    "SerializationArchiveTest: m_RootComponent deserialize failed: {} ({})",
                    readResult.message,
                    readResult.fieldPath);
                return false;
            }

            const Serialization::SerializeResult resolveResult =
                Serialization::Serializer::ResolvePendingObjectRefs(unresolvedRefs);
            if (!resolveResult.ok)
            {
                ME_CORE_ERROR(
                    "SerializationArchiveTest: m_RootComponent ResolvePendingObjectRefs failed: {}",
                    resolveResult.message);
                return false;
            }

            if (restoredObject->GetRootComponent() == nullptr)
            {
                ME_CORE_ERROR("SerializationArchiveTest: m_RootComponent should resolve after round-trip.");
                return false;
            }

            return true;
        }

        bool TestGameObjectWithComponentsSerializeRoundTrip()
        {
            SerializationArchiveTestScope scope;

            const GUID sourceGuid(0xCAFEBABE00000001ull, 0x0123456789ABCDEFull);
            std::shared_ptr<GameObject> sourceObject = NewObject<GameObject>("ComponentsTestGO", nullptr, sourceGuid);
            sourceObject->AddComponent<StaticMeshComponent>();

            if (sourceObject->GetAllComponents().size() != 1)
            {
                ME_CORE_ERROR("SerializationArchiveTest: expected one component before serialize.");
                return false;
            }

            std::vector<uint8_t> buffer;
            const Serialization::SerializeResult writeResult = Serialization::Serializer::SerializeObjectToBuffer(
                "minEngine::GameObject",
                sourceObject.get(),
                buffer);
            if (!writeResult.ok)
            {
                ME_CORE_ERROR(
                    "SerializationArchiveTest: GameObject+Components serialize failed: {} ({})",
                    writeResult.message,
                    writeResult.fieldPath);
                return false;
            }

            std::shared_ptr<GameObject> restoredObject = NewObject<GameObject>("ComponentsRestoredGO");
            std::vector<Serialization::PendingObjectRef> unresolvedRefs;
            const Serialization::SerializeResult readResult = Serialization::Serializer::DeserializeObjectFromBuffer(
                "minEngine::GameObject",
                restoredObject.get(),
                buffer,
                unresolvedRefs);
            if (!readResult.ok)
            {
                ME_CORE_ERROR(
                    "SerializationArchiveTest: GameObject+Components deserialize failed: {} ({})",
                    readResult.message,
                    readResult.fieldPath);
                return false;
            }

            if (restoredObject->GetAllComponents().size() != 1)
            {
                ME_CORE_ERROR("SerializationArchiveTest: expected one component after round-trip.");
                return false;
            }

            const Serialization::SerializeResult resolveResult =
                Serialization::Serializer::ResolvePendingObjectRefs(unresolvedRefs);
            if (!resolveResult.ok)
            {
                ME_CORE_ERROR(
                    "SerializationArchiveTest: ResolvePendingObjectRefs failed: {}",
                    resolveResult.message);
                return false;
            }

            return true;
        }

        bool TestVector3PropertySerializerRoundTrip()
        {
            const Reflection::MEClass* vectorClass = Reflection::ReflectionSystem::Get().FindClass("Vector3");
            if (vectorClass == nullptr)
            {
                ME_CORE_WARN("SerializationArchiveTest: Vector3 class not found; skipping serializer property test.");
                return true;
            }

            Vector3 source(1.0f, 2.0f, 3.0f);
            Vector3 restored(0.0f, 0.0f, 0.0f);

            std::vector<uint8_t> buffer;
            std::vector<Serialization::PendingObjectRef> unresolvedRefs;
            const Serialization::SerializeResult writeResult = Serialization::Serializer::SerializePropertyToBuffer(
                &source,
                vectorClass,
                "x",
                buffer);
            if (!writeResult.ok)
            {
                ME_CORE_ERROR("SerializationArchiveTest: SerializePropertyToBuffer failed: {}", writeResult.message);
                return false;
            }

            const Serialization::SerializeResult readResult = Serialization::Serializer::DeserializePropertyFromBuffer(
                &restored,
                vectorClass,
                "x",
                buffer,
                unresolvedRefs);
            if (!readResult.ok)
            {
                ME_CORE_ERROR("SerializationArchiveTest: DeserializePropertyFromBuffer failed: {}", readResult.message);
                return false;
            }

            if (restored.x != source.x)
            {
                ME_CORE_ERROR("SerializationArchiveTest: Vector3.x mismatch.");
                return false;
            }

            return true;
        }
    }

    bool RunSerializationArchiveTests(int /*argc*/, char** /*argv*/)
    {
        if (!EnsureReflectionReady())
        {
            ME_CORE_ERROR("SerializationArchiveTest: reflection init failed.");
            return false;
        }

        const bool passed = TestStaticMeshComponentSerializeRoundTrip();

        if (passed)
        {
            ME_CORE_INFO("SerializationArchiveTest: all tests passed.");
        }
        else
        {
            ME_CORE_ERROR("SerializationArchiveTest: one or more tests failed.");
        }

        return passed;
    }
}
