#include "JsonArchive.h"

namespace minEngine::Serialization
{
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

    const Json& JsonWriterArchive::GetRoot() const
    {
        return m_Root;
    }

    Json&& JsonWriterArchive::MoveRoot()
    {
        return std::move(m_Root);
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

        return &m_Root;
    }

    bool JsonReaderArchive::BeginObject(const std::string& expectedTypeName)
    {
        const Json* value = CurrentValue();
        if (value == nullptr || !value->is_object())
        {
            return false;
        }

        if (!expectedTypeName.empty()
            && value->contains("$typeName")
            && (*value)["$typeName"].is_string()
            && (*value)["$typeName"].get<std::string>() != expectedTypeName)
        {
            return false;
        }

        m_ContextStack.push_back(value);
        return true;
    }

    bool JsonReaderArchive::EndObject()
    {
        if (m_ContextStack.empty() || !m_ContextStack.back()->is_object())
        {
            return false;
        }

        m_ContextStack.pop_back();
        return true;
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
}
