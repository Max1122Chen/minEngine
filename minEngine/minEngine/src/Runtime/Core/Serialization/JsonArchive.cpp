#include "JsonArchive.h"
#include "Reflection/MEClass.h"
#include "Reflection/Reflection.h"
#include "Runtime/Core/Log/LogSystem.h"

#include <fstream>

namespace minEngine::Serialization
{
    namespace
    {
        constexpr const char* kSchemaVersionKey = "$schemaVersion";
        constexpr const char* kEngineVersionKey = "$engineVersion";
    }

    Json* JsonWriterArchive::AttachValue(Json&& value)
    {
        if (!m_HasRoot)
        {
            m_Root = std::move(value);
            m_HasRoot = true;
            return &m_Root;
        }

        if (m_Stack.empty())
        {
            return nullptr;
        }

        WriteContext& context = m_Stack.back();
        if (context.node == nullptr)
        {
            return nullptr;
        }

        if (context.isObject)
        {
            if (context.pendingFieldName.empty())
            {
                return nullptr;
            }

            const std::string fieldName = context.pendingFieldName;
            (*context.node)[fieldName] = std::move(value);
            context.pendingFieldName.clear();
            return &((*context.node)[fieldName]);
        }

        context.node->push_back(std::move(value));
        return &(context.node->back());
    }

    void JsonWriterArchive::ApplyRootSchemaVersion(uint32_t schemaVersion)
    {
        if (!m_HasRoot || !m_Root.is_object() || m_SchemaVersionApplied)
        {
            return;
        }

        m_Root[kSchemaVersionKey] = schemaVersion;
        m_SchemaVersionApplied = true;
    }

    void JsonWriterArchive::ApplyRootEngineVersion(const std::string& engineVersion)
    {
        if (!m_HasRoot || !m_Root.is_object() || m_EngineVersionApplied)
        {
            return;
        }

        m_Root[kEngineVersionKey] = engineVersion;
        m_EngineVersionApplied = true;
    }

    bool JsonWriterArchive::BeginObject(const std::string& typeName)
    {
        Json value = Json::object();
        if (!typeName.empty())
        {
            value["$typeName"] = typeName;
        }

        Json* inserted = AttachValue(std::move(value));
        if (inserted == nullptr || !inserted->is_object())
        {
            return false;
        }

        m_Stack.push_back(WriteContext{inserted, true, {}});
        return true;
    }

    bool JsonWriterArchive::EndObject()
    {
        if (m_Stack.empty() || !m_Stack.back().isObject)
        {
            return false;
        }

        m_Stack.pop_back();
        return true;
    }

    bool JsonWriterArchive::BeginObjectPtr(const std::string& typeName)
    {
        Json value = Json::object();
        if (!typeName.empty())
        {
            value["$ptr_typeName"] = typeName;
        }

        Json* inserted = AttachValue(std::move(value));
        if (inserted == nullptr || !inserted->is_object())
        {
            return false;
        }

        m_Stack.push_back(WriteContext{inserted, true, {}});
        return true;
    }

    bool JsonWriterArchive::EndObjectPtr()
    {
        return EndObject();
    }

    bool JsonWriterArchive::BeginGuidRef(const GUID& guid)
    {
        Json value = Json::object();
        value["$guid"] = {
            {"high", guid.High},
            {"low", guid.Low}
        };

        Json* inserted = AttachValue(std::move(value));
        if (inserted == nullptr || !inserted->is_object())
        {
            return false;
        }

        m_Stack.push_back(WriteContext{inserted, true, {}});
        return true;
    }

    bool JsonWriterArchive::EndGuidRef()
    {
        return EndObject();
    }

    bool JsonWriterArchive::BeginField(const std::string& fieldName)
    {
        if (m_Stack.empty() || !m_Stack.back().isObject || fieldName.empty())
        {
            return false;
        }

        m_Stack.back().pendingFieldName = fieldName;
        return true;
    }

    bool JsonWriterArchive::EndField()
    {
        if (m_Stack.empty() || !m_Stack.back().isObject)
        {
            return false;
        }

        m_Stack.back().pendingFieldName.clear();
        return true;
    }

    bool JsonWriterArchive::BeginArray(size_t /*count*/)
    {
        Json* inserted = AttachValue(Json::array());
        if (inserted == nullptr || !inserted->is_array())
        {
            return false;
        }

        m_Stack.push_back(WriteContext{inserted, false, {}});
        return true;
    }

    bool JsonWriterArchive::EndArray()
    {
        if (m_Stack.empty() || m_Stack.back().isObject)
        {
            return false;
        }

        m_Stack.pop_back();
        return true;
    }

    bool JsonWriterArchive::WriteNull()
    {
        return AttachValue(Json()) != nullptr;
    }

    bool JsonWriterArchive::WriteBool(bool value)
    {
        return AttachValue(Json(value)) != nullptr;
    }

    bool JsonWriterArchive::WriteInt64(int64_t value)
    {
        return AttachValue(Json(value)) != nullptr;
    }

    bool JsonWriterArchive::WriteUInt64(uint64_t value)
    {
        return AttachValue(Json(value)) != nullptr;
    }

    bool JsonWriterArchive::WriteDouble(double value)
    {
        return AttachValue(Json(value)) != nullptr;
    }

    bool JsonWriterArchive::WriteString(const std::string& value)
    {
        return AttachValue(Json(value)) != nullptr;
    }

    void JsonWriterArchive::ResetWriteState()
    {
        ResetRoot();
        m_LastArchiveError.clear();
    }

    bool JsonWriterArchive::WriteToFile(const std::string& filePath)
    {
        m_LastArchiveError.clear();

        if (!m_HasRoot)
        {
            m_LastArchiveError = "archive root is empty";
            return false;
        }

        std::ofstream output(filePath, std::ios::trunc);
        if (!output.is_open())
        {
            m_LastArchiveError = "failed to open file for writing";
            return false;
        }

        output << m_Root.dump(4);
        if (!output.good())
        {
            m_LastArchiveError = "failed to write JSON content to file";
            return false;
        }

        return true;
    }

    void JsonReaderArchive::BindRoot(const Json& root)
    {
        m_Root = &root;
        m_ContextStack.clear();
        m_ValueStack.clear();
        m_ObjectConsumedKeys.clear();
        m_LastArchiveError.clear();
        ParseRootDiskMeta(root);
    }

    bool JsonReaderArchive::IsKnownMetaKey(const std::string& key)
    {
        return key == "$typeName"
            || key == "$ptr_typeName"
            || key == "$guid"
            || key == kSchemaVersionKey
            || key == kEngineVersionKey;
    }

    void JsonReaderArchive::ParseRootDiskMeta(const Json& root)
    {
        m_ReadSchemaVersion = 0;
        m_ReadEngineVersion.clear();
        if (!root.is_object())
        {
            return;
        }

        if (root.contains(kSchemaVersionKey))
        {
            const Json& versionNode = root[kSchemaVersionKey];
            if (versionNode.is_number_unsigned() || versionNode.is_number_integer())
            {
                m_ReadSchemaVersion = versionNode.get<uint32_t>();
            }
            else
            {
                ME_LOG(LogSerialization, Warn, "JsonReaderArchive: invalid $schemaVersion type; treating as 0");
            }
        }

        if (root.contains(kEngineVersionKey))
        {
            const Json& engineNode = root[kEngineVersionKey];
            if (engineNode.is_string())
            {
                m_ReadEngineVersion = engineNode.get<std::string>();
            }
            else
            {
                ME_LOG(LogSerialization, Warn, "JsonReaderArchive: invalid $engineVersion type; treating as empty");
            }
        }
    }

    void JsonReaderArchive::PushObjectContext(const Json* object)
    {
        m_ContextStack.push_back(object);
        m_ObjectConsumedKeys.emplace_back();
    }

    void JsonReaderArchive::WarnUnreadObjectKeys(const Json& object,
                                                 const std::unordered_set<std::string>& consumedKeys) const
    {
        for (auto it = object.begin(); it != object.end(); ++it)
        {
            const std::string& key = it.key();
            if (consumedKeys.find(key) != consumedKeys.end())
            {
                continue;
            }
            if (IsKnownMetaKey(key))
            {
                continue;
            }
            if (!key.empty() && key[0] == '$')
            {
                ME_LOG(LogSerialization, Warn, "Json deserialize: unrecognized meta key '{}' skipped", key);
                continue;
            }

            ME_LOG(LogSerialization, Warn, "Json deserialize: unknown field '{}' skipped", key);
        }
    }

    const Json* JsonReaderArchive::CurrentValue() const
    {
        if (!m_ValueStack.empty())
        {
            return m_ValueStack.back();
        }

        if (!m_ContextStack.empty())
        {
            return m_ContextStack.back();
        }

        return m_Root;
    }

    bool JsonReaderArchive::BeginObject(const Reflection::MEClass* baseClassInfo)
    {
        const Json* value = CurrentValue();
        if (value == nullptr || !value->is_object())
        {
            return false;
        }

        // If baseClassInfo is provided, validate $typeName against the full inheritance chain.
        if (baseClassInfo != nullptr
            && value->contains("$typeName")
            && (*value)["$typeName"].is_string())
        {
            const std::string typeName = (*value)["$typeName"].get<std::string>();
            if (!Reflection::ReflectionSystem::Get().IsClassNameSameOrDerived(typeName, baseClassInfo))
            {
                return false;
            }
        }

        PushObjectContext(value);
        return true;
    }

    bool JsonReaderArchive::BeginObject(const std::string& expectedTypeName)
    {
        const Json* value = CurrentValue();
        if (value == nullptr || !value->is_object())
        {
            return false;
        }

        // If expectedTypeName is provided, check the $typeName field in the JSON object. If it exists and does not match expectedTypeName, return false.
        if (!expectedTypeName.empty()
            && value->contains("$typeName")
            && (*value)["$typeName"].is_string()
            && (*value)["$typeName"].get<std::string>() != expectedTypeName)
        {
            return false;
        }

        PushObjectContext(value);
        return true;
    }

    bool JsonReaderArchive::EndObject()
    {
        if (m_ContextStack.empty() || !m_ContextStack.back()->is_object() || m_ObjectConsumedKeys.empty())
        {
            return false;
        }

        WarnUnreadObjectKeys(*m_ContextStack.back(), m_ObjectConsumedKeys.back());
        m_ObjectConsumedKeys.pop_back();
        m_ContextStack.pop_back();
        return true;
    }

    bool JsonReaderArchive::BeginObjectPtr(const Reflection::MEClass* baseClassInfo, std::string& outClassName)
    {
        const Json* value = CurrentValue();
        if (value == nullptr || !value->is_object())
        {
            return false;
        }

        // Object-ptr inline node must carry $ptr_typeName.
        // This allows upper-layer probing logic to clearly distinguish it from GUID reference nodes.
        if (!value->contains("$ptr_typeName") || !(*value)["$ptr_typeName"].is_string())
        {
            return false;
        }

        const std::string typeName = (*value)["$ptr_typeName"].get<std::string>();
        if (baseClassInfo != nullptr
            && !Reflection::ReflectionSystem::Get().IsClassNameSameOrDerived(typeName, baseClassInfo))
        {
            return false;
        }

        outClassName = typeName;

        PushObjectContext(value);
        return true;
    }

    bool JsonReaderArchive::EndObjectPtr()
    {
        return EndObject();
    }

    bool JsonReaderArchive::BeginGuidRef(GUID& outGuid)
    {
        const Json* value = CurrentValue();
        if (value == nullptr || !value->is_object())
        {
            return false;
        }

        if (!value->contains("$guid") || !(*value)["$guid"].is_object())
        {
            return false;
        }

        const Json& guidNode = (*value)["$guid"];
        if (!guidNode.contains("high") || !guidNode.contains("low"))
        {
            return false;
        }

        const Json& highNode = guidNode["high"];
        const Json& lowNode = guidNode["low"];
        if ((!highNode.is_number_unsigned() && !highNode.is_number_integer())
            || (!lowNode.is_number_unsigned() && !lowNode.is_number_integer()))
        {
            return false;
        }

        outGuid.High = highNode.get<uint64_t>();
        outGuid.Low = lowNode.get<uint64_t>();

        PushObjectContext(value);
        return true;
    }

    bool JsonReaderArchive::EndGuidRef()
    {
        return EndObject();
    }

    bool JsonReaderArchive::EnterField(const std::string& fieldName)
    {
        if (m_ContextStack.empty() || !m_ContextStack.back()->is_object())
        {
            return false;
        }

        const Json& object = *m_ContextStack.back();
        auto iter = object.find(fieldName);
        if (iter == object.end())
        {
            return false;
        }

        if (!m_ObjectConsumedKeys.empty())
        {
            m_ObjectConsumedKeys.back().insert(fieldName);
        }

        m_ValueStack.push_back(&(*iter));
        return true;
    }

    bool JsonReaderArchive::LeaveField()
    {
        if (m_ValueStack.empty())
        {
            return false;
        }

        m_ValueStack.pop_back();
        return true;
    }

    bool JsonReaderArchive::BeginArray(size_t& outCount)
    {
        const Json* value = CurrentValue();
        if (value == nullptr || !value->is_array())
        {
            return false;
        }

        outCount = value->size();
        m_ContextStack.push_back(value);
        return true;
    }

    bool JsonReaderArchive::EnterArrayElement(size_t index)
    {
        if (m_ContextStack.empty() || !m_ContextStack.back()->is_array())
        {
            return false;
        }

        const Json& array = *m_ContextStack.back();
        if (index >= array.size())
        {
            return false;
        }

        m_ValueStack.push_back(&array[index]);
        return true;
    }

    bool JsonReaderArchive::LeaveArrayElement()
    {
        if (m_ValueStack.empty())
        {
            return false;
        }

        m_ValueStack.pop_back();
        return true;
    }

    bool JsonReaderArchive::EndArray()
    {
        if (m_ContextStack.empty() || !m_ContextStack.back()->is_array())
        {
            return false;
        }

        m_ContextStack.pop_back();
        return true;
    }

    bool JsonReaderArchive::ReadNull()
    {
        const Json* value = CurrentValue();
        return value != nullptr && value->is_null();
    }

    bool JsonReaderArchive::ReadBool(bool& outValue)
    {
        const Json* value = CurrentValue();
        if (value == nullptr || !value->is_boolean())
        {
            return false;
        }

        outValue = value->get<bool>();
        return true;
    }

    bool JsonReaderArchive::ReadInt64(int64_t& outValue)
    {
        const Json* value = CurrentValue();
        if (value == nullptr || !value->is_number_integer())
        {
            return false;
        }

        outValue = value->get<int64_t>();
        return true;
    }

    bool JsonReaderArchive::ReadUInt64(uint64_t& outValue)
    {
        const Json* value = CurrentValue();
        if (value == nullptr || !value->is_number_unsigned())
        {
            return false;
        }

        outValue = value->get<uint64_t>();
        return true;
    }

    bool JsonReaderArchive::ReadDouble(double& outValue)
    {
        const Json* value = CurrentValue();
        if (value == nullptr || !value->is_number())
        {
            return false;
        }

        outValue = value->get<double>();
        return true;
    }

    bool JsonReaderArchive::ReadString(std::string& outValue)
    {
        const Json* value = CurrentValue();
        if (value == nullptr || !value->is_string())
        {
            return false;
        }

        outValue = value->get<std::string>();
        return true;
    }

    void JsonReaderArchive::ResetReadState()
    {
        m_ContextStack.clear();
        m_ValueStack.clear();
        m_ObjectConsumedKeys.clear();
        m_ReadSchemaVersion = 0;
        m_ReadEngineVersion.clear();
        m_LastArchiveError.clear();
    }

    bool JsonReaderArchive::ReadFromFile(const std::string& filePath)
    {
        ResetReadState();

        std::ifstream input(filePath);
        if (!input.is_open())
        {
            m_LastArchiveError = "failed to open file for reading";
            return false;
        }

        try
        {
            input >> m_OwnedRoot;
        }
        catch (const std::exception& e)
        {
            m_LastArchiveError = std::string("failed to parse JSON: ") + e.what();
            return false;
        }

        m_Root = &m_OwnedRoot;
        ParseRootDiskMeta(m_OwnedRoot);
        return true;
    }
}
