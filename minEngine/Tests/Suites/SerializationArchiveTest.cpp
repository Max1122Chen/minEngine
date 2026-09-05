#include "SerializationArchiveTest.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Core/Reflection/Reflection.h"

#include "BinaryArchive.h"
#include "JsonArchive.h"
#include "Serializer.h"
#include "Runtime/Core/GUID/GUID.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Components/MovementComponent.h"
#include "Runtime/Function/Framework/Components/StaticMeshComponent.h"
#include "Runtime/Function/Framework/Transform/Transform.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/Material/MaterialCompiler/MaterialCompileTypes.h"

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
            if (!writer.BeginObject("minEngine::GUID"))
            {
                ME_CORE_ERROR("SerializationArchiveTest: BeginObject failed: {}", writer.GetLastArchiveError());
                return false;
            }

            if (!writer.BeginField("High") || !writer.WriteUInt64(0x1111ull) || !writer.EndField())
            {
                ME_CORE_ERROR("SerializationArchiveTest: High field write failed.");
                return false;
            }

            if (!writer.BeginField("Low") || !writer.WriteUInt64(0x2222ull) || !writer.EndField())
            {
                ME_CORE_ERROR("SerializationArchiveTest: Low field write failed.");
                return false;
            }

            if (!writer.EndObject())
            {
                ME_CORE_ERROR("SerializationArchiveTest: EndObject failed.");
                return false;
            }

            BinaryReaderArchive reader(writer.TakeBuffer());
            if (!reader.BeginObject("minEngine::GUID"))
            {
                ME_CORE_ERROR("SerializationArchiveTest: BeginObject read failed: {}", reader.GetLastArchiveError());
                return false;
            }

            uint64_t high = 0;
            uint64_t low = 0;
            if (!reader.EnterField("High") || !reader.ReadUInt64(high) || !reader.LeaveField())
            {
                ME_CORE_ERROR("SerializationArchiveTest: High field read failed.");
                return false;
            }

            if (!reader.EnterField("Low") || !reader.ReadUInt64(low) || !reader.LeaveField())
            {
                ME_CORE_ERROR("SerializationArchiveTest: Low field read failed.");
                return false;
            }

            if (!reader.EndObject() || high != 0x1111ull || low != 0x2222ull)
            {
                ME_CORE_ERROR("SerializationArchiveTest: object payload mismatch.");
                return false;
            }

            return true;
        }

        bool TestSerializeObjectToBufferEmptyObjectRoundTrip()
        {
            BinaryWriterArchive writer;
            if (!writer.BeginObject("minEngine::GUID") || !writer.EndObject())
            {
                ME_CORE_ERROR("SerializationArchiveTest: failed to write empty object shell: {}", writer.GetLastArchiveError());
                return false;
            }

            const std::vector<uint8_t> buffer = writer.TakeBuffer();
            BinaryReaderArchive reader(buffer);
            if (!reader.BeginObject("minEngine::GUID") || !reader.EndObject())
            {
                ME_CORE_ERROR("SerializationArchiveTest: failed to read empty object shell: {}", reader.GetLastArchiveError());
                return false;
            }

            return true;
        }

        bool TestNestedGuidObjectFieldRoundTrip()
        {
            const GUID sourceGuid(0xAABBCCDDEEFF0011ull, 0x1122334455667788ull);

            BinaryWriterArchive writer;
            if (!writer.BeginObject("minEngine::MEObject"))
            {
                ME_CORE_ERROR("SerializationArchiveTest: nested object BeginObject failed: {}", writer.GetLastArchiveError());
                return false;
            }

            if (!writer.BeginField("m_Guid")
                || !writer.BeginObject("minEngine::GUID")
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
            if (!reader.BeginObject("minEngine::MEObject"))
            {
                ME_CORE_ERROR("SerializationArchiveTest: nested object read BeginObject failed: {}", reader.GetLastArchiveError());
                return false;
            }

            GUID readGuid;
            if (!reader.EnterField("m_Guid")
                || !reader.BeginObject("minEngine::GUID")
                || !reader.EnterField("High") || !reader.ReadUInt64(readGuid.High) || !reader.LeaveField()
                || !reader.EnterField("Low") || !reader.ReadUInt64(readGuid.Low) || !reader.LeaveField()
                || !reader.EndObject()
                || !reader.LeaveField()
                || !reader.EndObject())
            {
                ME_CORE_ERROR("SerializationArchiveTest: nested GUID field read failed: {}", reader.GetLastArchiveError());
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
            if (!writer.BeginObject("minEngine::GameObject")
                || !writer.BeginField("m_RootComponent")
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
            if (!reader.BeginObject("minEngine::GameObject")
                || !reader.EnterField("m_RootComponent")
                || !reader.BeginGuidRef(readGuid)
                || !reader.EndGuidRef()
                || !reader.LeaveField()
                || !reader.EndObject())
            {
                ME_CORE_ERROR("SerializationArchiveTest: GuidRef field read failed: {}", reader.GetLastArchiveError());
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
            // Hand-written framing uses GameObject.m_Components (array of ObjectPtr).
            BinaryWriterArchive writer;
            if (!writer.BeginObject("minEngine::GameObject")
                || !writer.BeginField("m_Components")
                || !writer.BeginArray(1)
                || !writer.BeginObjectPtr("minEngine::StaticMeshComponent")
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
            std::string dynamicClassName;
            if (!reader.BeginObject("minEngine::GameObject")
                || !reader.EnterField("m_Components")
                || !reader.BeginArray(count)
                || count != 1
                || !reader.EnterArrayElement(0)
                || !reader.BeginObjectPtr(nullptr, dynamicClassName)
                || !reader.EndObjectPtr()
                || !reader.LeaveArrayElement()
                || !reader.EndArray()
                || !reader.LeaveField()
                || !reader.EndObject())
            {
                ME_CORE_ERROR(
                    "SerializationArchiveTest: array ObjectPtr field read failed: {}",
                    reader.GetLastArchiveError());
                return false;
            }

            if (dynamicClassName.find("StaticMeshComponent") == std::string::npos)
            {
                ME_CORE_ERROR("SerializationArchiveTest: array ObjectPtr dynamic class mismatch: {}", dynamicClassName);
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

        bool TestTransformSerializeRoundTrip()
        {
            const Reflection::MEClass* transformClass = Reflection::ReflectionSystem::Get().FindClass<Transform>();
            if (transformClass == nullptr)
            {
                ME_CORE_WARN("SerializationArchiveTest: Transform class not found; skipping transform test.");
                return true;
            }

            Transform source(Vector3(1.0f, 2.0f, 3.0f), Vector3(15.0f, 30.0f, 45.0f), Vector3(2.0f, 1.0f, 0.5f));
            Transform restored;

            std::vector<uint8_t> buffer;
            const Serialization::SerializeResult writeResult = Serialization::Serializer::SerializePropertyToBuffer(
                &source,
                transformClass,
                "Rotation",
                buffer);
            if (!writeResult.ok)
            {
                ME_CORE_ERROR(
                    "SerializationArchiveTest: Transform Rotation serialize failed: {}",
                    writeResult.message);
                return false;
            }

            std::vector<Serialization::PendingObjectRef> unresolvedRefs;
            const Serialization::SerializeResult readResult = Serialization::Serializer::DeserializePropertyFromBuffer(
                &restored,
                transformClass,
                "Rotation",
                buffer,
                unresolvedRefs);
            if (!readResult.ok)
            {
                ME_CORE_ERROR(
                    "SerializationArchiveTest: Transform Rotation deserialize failed: {}",
                    readResult.message);
                return false;
            }

            if (!(source.Rotation == restored.Rotation))
            {
                ME_CORE_ERROR("SerializationArchiveTest: Transform Rotation mismatch after round-trip.");
                return false;
            }

            return true;
        }

        bool TestUint8EnumPropertyRoundTrip()
        {
            SerializationArchiveTestScope scope;

            const Reflection::MEClass* materialClass =
                Reflection::ReflectionSystem::Get().FindClass<Material>();
            if (materialClass == nullptr)
            {
                ME_CORE_ERROR("SerializationArchiveTest: Material class not found.");
                return false;
            }

            Material source;
            source.m_ShadingModel = MaterialShadingModel::BlinnPhong;
            source.m_BlendMode = MaterialBlendMode::Opaque;

            std::vector<uint8_t> shadingBuffer;
            const Serialization::SerializeResult writeShading =
                Serialization::Serializer::SerializePropertyToBuffer(
                    &source,
                    materialClass,
                    "m_ShadingModel",
                    shadingBuffer);
            if (!writeShading.ok)
            {
                ME_CORE_ERROR(
                    "SerializationArchiveTest: m_ShadingModel serialize failed: {}",
                    writeShading.message);
                return false;
            }

            Material restored;
            restored.m_ShadingModel = MaterialShadingModel::Unlit;
            restored.m_BlendMode = MaterialBlendMode::Translucent;
            std::vector<Serialization::PendingObjectRef> unresolvedRefs;
            const Serialization::SerializeResult readShading =
                Serialization::Serializer::DeserializePropertyFromBuffer(
                    &restored,
                    materialClass,
                    "m_ShadingModel",
                    shadingBuffer,
                    unresolvedRefs);
            if (!readShading.ok)
            {
                ME_CORE_ERROR(
                    "SerializationArchiveTest: m_ShadingModel deserialize failed: {}",
                    readShading.message);
                return false;
            }

            if (restored.m_ShadingModel != MaterialShadingModel::BlinnPhong)
            {
                ME_CORE_ERROR("SerializationArchiveTest: m_ShadingModel mismatch after round-trip.");
                return false;
            }

            // Neighbor uint8 enum must not be clobbered by size-mismatched enum codecs (TD-013).
            if (restored.m_BlendMode != MaterialBlendMode::Translucent)
            {
                ME_CORE_ERROR(
                    "SerializationArchiveTest: m_BlendMode neighbor corrupted by enum codec (TD-013).");
                return false;
            }

            return true;
        }

        bool TestJsonSchemaVersionAndUnknownFieldCompat()
        {
            SerializationArchiveTestScope scope;

            const GUID sourceGuid(0x1111222233334444ull, 0x5555666677778888ull);
            std::shared_ptr<GameObject> sourceObject = NewObject<GameObject>("JsonCompatGO", nullptr, sourceGuid);

            Json root;
            const Serialization::SerializeResult writeResult = Serialization::Serializer::SerializeObjectToJson(
                sourceObject.get(),
                root,
                Serialization::SerializerOptions{
                    .enumAsString = true,
                    .strictTypeCheck = false,
                    .skipUnknownField = true,
                    .writeSchemaVersion = true,
                    .schemaVersion = 1u,
                });
            if (!writeResult.ok || !root.is_object() || !root.contains("$schemaVersion"))
            {
                ME_CORE_ERROR("SerializationArchiveTest: JSON schemaVersion write failed.");
                return false;
            }
            if (root["$schemaVersion"].get<uint32_t>() != 1u)
            {
                ME_CORE_ERROR("SerializationArchiveTest: unexpected $schemaVersion value.");
                return false;
            }

            root["m_FutureOptionalField"] = 42;
            root["$unknownMeta"] = "x";

            std::shared_ptr<GameObject> restoredObject = NewObject<GameObject>("JsonCompatRestoredGO");
            std::vector<Serialization::PendingObjectRef> unresolvedRefs;
            Serialization::JsonReaderArchive reader(root);
            if (reader.GetReadSchemaVersion() != 1u)
            {
                ME_CORE_ERROR("SerializationArchiveTest: GetReadSchemaVersion mismatch.");
                return false;
            }

            const Serialization::SerializeResult readResult = Serialization::Serializer::DeserializeObjectFromJson(
                restoredObject.get(),
                root,
                unresolvedRefs,
                Serialization::SerializerOptions{
                    .enumAsString = true,
                    .strictTypeCheck = false,
                    .skipUnknownField = true,
                });
            if (!readResult.ok)
            {
                ME_CORE_ERROR(
                    "SerializationArchiveTest: JSON unknown-field load failed: {} ({})",
                    readResult.message,
                    readResult.fieldPath);
                return false;
            }

            if (restoredObject->GetGuid() != sourceGuid)
            {
                ME_CORE_ERROR("SerializationArchiveTest: JSON compat GUID mismatch.");
                return false;
            }

            return true;
        }

        bool TestJsonMissingFieldKeepsDefault()
        {
            SerializationArchiveTestScope scope;

            // Minimal object JSON without m_Name / m_Guid — should load with defaults under loose options.
            const Json root = Json::object();

            std::shared_ptr<GameObject> restoredObject = NewObject<GameObject>("JsonMissingFieldsGO");
            const std::string nameBefore = restoredObject->GetName();
            std::vector<Serialization::PendingObjectRef> unresolvedRefs;
            const Serialization::SerializeResult readResult = Serialization::Serializer::DeserializeObjectFromJson(
                restoredObject.get(),
                root,
                unresolvedRefs,
                Serialization::SerializerOptions{
                    .enumAsString = true,
                    .strictTypeCheck = false,
                    .skipUnknownField = true,
                    .writeSchemaVersion = false,
                });
            if (!readResult.ok)
            {
                ME_CORE_ERROR(
                    "SerializationArchiveTest: JSON missing-field load failed: {} ({})",
                    readResult.message,
                    readResult.fieldPath);
                return false;
            }

            if (restoredObject->GetName() != nameBefore)
            {
                ME_CORE_ERROR("SerializationArchiveTest: missing field should keep default name.");
                return false;
            }

            return true;
        }

        bool RunSerializationArchiveSmokeTestsImpl()
        {
            return TestStaticMeshComponentSerializeRoundTrip() && TestTransformSerializeRoundTrip()
                   && TestUint8EnumPropertyRoundTrip() && TestGameObjectSerializeObjectToBufferRoundTrip()
                   && TestGameObjectWithComponentsSerializeRoundTrip()
                   && TestNestedGuidObjectFieldRoundTrip() && TestGuidRefObjectFieldRoundTrip()
                   && TestArrayOfInlineObjectPtrFieldRoundTrip()
                   && TestJsonSchemaVersionAndUnknownFieldCompat() && TestJsonMissingFieldKeepsDefault();
        }

        bool RunSerializationArchivePrimitiveTestsImpl()
        {
            return TestBoolRoundTrip() && TestStringRoundTrip() && TestGuidRefRoundTrip()
                   && TestArrayRoundTrip() && TestObjectFieldsRoundTrip()
                   && TestSerializeObjectToBufferEmptyObjectRoundTrip();
        }
    }

    bool RunSerializationArchiveSmokeTests()
    {
        return RunSerializationArchiveSmokeTestsImpl();
    }

    bool RunSerializationArchivePrimitiveTests()
    {
        return RunSerializationArchivePrimitiveTestsImpl();
    }
}

#include "doctest.h"

#include "EngineTestFixture.h"

TEST_CASE("serialization-archive: static mesh round-trip [smoke][full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    CHECK(minEngine::RunSerializationArchiveSmokeTests());
}

TEST_CASE("serialization-archive: primitive archives [full]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    CHECK(minEngine::RunSerializationArchivePrimitiveTests());
}
