#include "SerializationArchiveTest.h"

#include "BinaryArchive.h"
#include "Serializer.h"
#include "Runtime/Core/GUID/GUID.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Core/Reflection/Reflection.h"

namespace minEngine
{
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

    bool ShouldRunSerializationArchiveTestsOnly(int argc, char** argv)
    {
        for (int argIndex = 1; argIndex < argc; ++argIndex)
        {
            if (argv[argIndex] != nullptr && std::string_view(argv[argIndex]) == "--serialization-archive-test")
            {
                return true;
            }
        }
        return false;
    }

    bool RunSerializationArchiveTests(int /*argc*/, char** /*argv*/)
    {
        if (!EnsureReflectionReady())
        {
            ME_CORE_ERROR("SerializationArchiveTest: reflection init failed.");
            return false;
        }

        const bool passed = TestBoolRoundTrip()
            && TestStringRoundTrip()
            && TestGuidRefRoundTrip()
            && TestArrayRoundTrip()
            && TestObjectFieldsRoundTrip()
            && TestVector3PropertySerializerRoundTrip();

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
