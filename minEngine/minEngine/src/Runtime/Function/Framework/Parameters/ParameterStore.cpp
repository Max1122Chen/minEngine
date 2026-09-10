#include "Runtime/Function/Framework/Parameters/ParameterStore.h"

#include <cstring>

namespace minEngine
{
    void ParameterStore::BindLayout(const std::shared_ptr<const ParameterLayout>& layout)
    {
        m_Layout = layout;
        if (!m_Layout)
        {
            m_Memory.clear();
            return;
        }

        m_Memory.assign(m_Layout->GetTotalBytes(), 0);
        ResetToDefaults();
    }

    void ParameterStore::Clear()
    {
        m_Layout.reset();
        m_Memory.clear();
    }

    void ParameterStore::ResetToDefaults()
    {
        if (!m_Layout)
        {
            return;
        }

        const std::vector<uint8_t>& defaults = m_Layout->GetDefaultBlob();
        if (defaults.size() != m_Memory.size())
        {
            m_Memory.assign(m_Layout->GetTotalBytes(), 0);
        }

        if (defaults.empty())
        {
            if (!m_Memory.empty())
            {
                std::memset(m_Memory.data(), 0, m_Memory.size());
            }
            return;
        }

        std::memcpy(m_Memory.data(), defaults.data(), defaults.size());
    }

    bool ParameterStore::CopyFrom(const ParameterStore& other)
    {
        if (!m_Layout || !other.m_Layout || m_Layout.get() != other.m_Layout.get())
        {
            return false;
        }

        if (other.m_Memory.size() != m_Memory.size())
        {
            return false;
        }

        if (!m_Memory.empty())
        {
            std::memcpy(m_Memory.data(), other.m_Memory.data(), m_Memory.size());
        }
        return true;
    }

    ParameterKeyId ParameterStore::FindKeyId(std::string_view name) const
    {
        if (!m_Layout)
        {
            return kInvalidParameterKeyId;
        }
        return m_Layout->FindKeyId(name);
    }

    const ParameterLayoutEntry* ParameterStore::GetTypedEntry(
        ParameterKeyId keyId,
        ParameterValueType expectedType) const
    {
        if (!m_Layout)
        {
            return nullptr;
        }

        const ParameterLayoutEntry* entry = m_Layout->GetEntry(keyId);
        if (!entry || entry->Type != expectedType)
        {
            return nullptr;
        }

        if (static_cast<size_t>(entry->Offset) + entry->Size > m_Memory.size())
        {
            return nullptr;
        }

        return entry;
    }

    uint8_t* ParameterStore::GetMutableBytes(ParameterKeyId keyId, ParameterValueType expectedType)
    {
        const ParameterLayoutEntry* entry = GetTypedEntry(keyId, expectedType);
        if (!entry)
        {
            return nullptr;
        }
        return m_Memory.data() + entry->Offset;
    }

    const uint8_t* ParameterStore::GetConstBytes(ParameterKeyId keyId, ParameterValueType expectedType) const
    {
        const ParameterLayoutEntry* entry = GetTypedEntry(keyId, expectedType);
        if (!entry)
        {
            return nullptr;
        }
        return m_Memory.data() + entry->Offset;
    }

    bool ParameterStore::SetBool(ParameterKeyId keyId, bool value)
    {
        uint8_t* bytes = GetMutableBytes(keyId, ParameterValueType::Bool);
        if (!bytes)
        {
            return false;
        }
        *bytes = value ? 1 : 0;
        return true;
    }

    bool ParameterStore::SetInt32(ParameterKeyId keyId, int32_t value)
    {
        uint8_t* bytes = GetMutableBytes(keyId, ParameterValueType::Int32);
        if (!bytes)
        {
            return false;
        }
        std::memcpy(bytes, &value, sizeof(int32_t));
        return true;
    }

    bool ParameterStore::SetFloat(ParameterKeyId keyId, float value)
    {
        uint8_t* bytes = GetMutableBytes(keyId, ParameterValueType::Float);
        if (!bytes)
        {
            return false;
        }
        std::memcpy(bytes, &value, sizeof(float));
        return true;
    }

    bool ParameterStore::TryGetBool(ParameterKeyId keyId, bool& outValue) const
    {
        const uint8_t* bytes = GetConstBytes(keyId, ParameterValueType::Bool);
        if (!bytes)
        {
            return false;
        }
        outValue = (*bytes != 0);
        return true;
    }

    bool ParameterStore::TryGetInt32(ParameterKeyId keyId, int32_t& outValue) const
    {
        const uint8_t* bytes = GetConstBytes(keyId, ParameterValueType::Int32);
        if (!bytes)
        {
            return false;
        }
        std::memcpy(&outValue, bytes, sizeof(int32_t));
        return true;
    }

    bool ParameterStore::TryGetFloat(ParameterKeyId keyId, float& outValue) const
    {
        const uint8_t* bytes = GetConstBytes(keyId, ParameterValueType::Float);
        if (!bytes)
        {
            return false;
        }
        std::memcpy(&outValue, bytes, sizeof(float));
        return true;
    }

    bool ParameterStore::SetBoolByName(std::string_view name, bool value)
    {
        return SetBool(FindKeyId(name), value);
    }

    bool ParameterStore::SetInt32ByName(std::string_view name, int32_t value)
    {
        return SetInt32(FindKeyId(name), value);
    }

    bool ParameterStore::SetFloatByName(std::string_view name, float value)
    {
        return SetFloat(FindKeyId(name), value);
    }

    bool ParameterStore::TryGetBoolByName(std::string_view name, bool& outValue) const
    {
        return TryGetBool(FindKeyId(name), outValue);
    }

    bool ParameterStore::TryGetInt32ByName(std::string_view name, int32_t& outValue) const
    {
        return TryGetInt32(FindKeyId(name), outValue);
    }

    bool ParameterStore::TryGetFloatByName(std::string_view name, float& outValue) const
    {
        return TryGetFloat(FindKeyId(name), outValue);
    }
}
