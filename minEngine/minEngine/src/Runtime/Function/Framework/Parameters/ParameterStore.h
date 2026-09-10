#pragma once

#include "Runtime/Function/Framework/Parameters/ParameterLayout.h"
#include "Runtime/Function/Framework/Parameters/ParameterValueType.h"

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace minEngine
{
    // Instance memory for one compiled ParameterLayout.
    class ParameterStore
    {
    public:
        void BindLayout(const std::shared_ptr<const ParameterLayout>& layout);
        void Clear();

        const ParameterLayout* GetLayout() const { return m_Layout.get(); }
        bool IsBound() const { return m_Layout != nullptr; }

        void ResetToDefaults();
        bool CopyFrom(const ParameterStore& other);

        ParameterKeyId FindKeyId(std::string_view name) const;

        bool SetBool(ParameterKeyId keyId, bool value);
        bool SetInt32(ParameterKeyId keyId, int32_t value);
        bool SetFloat(ParameterKeyId keyId, float value);

        bool TryGetBool(ParameterKeyId keyId, bool& outValue) const;
        bool TryGetInt32(ParameterKeyId keyId, int32_t& outValue) const;
        bool TryGetFloat(ParameterKeyId keyId, float& outValue) const;

        bool SetBoolByName(std::string_view name, bool value);
        bool SetInt32ByName(std::string_view name, int32_t value);
        bool SetFloatByName(std::string_view name, float value);

        bool TryGetBoolByName(std::string_view name, bool& outValue) const;
        bool TryGetInt32ByName(std::string_view name, int32_t& outValue) const;
        bool TryGetFloatByName(std::string_view name, float& outValue) const;

    private:
        const ParameterLayoutEntry* GetTypedEntry(ParameterKeyId keyId, ParameterValueType expectedType) const;
        uint8_t* GetMutableBytes(ParameterKeyId keyId, ParameterValueType expectedType);
        const uint8_t* GetConstBytes(ParameterKeyId keyId, ParameterValueType expectedType) const;

        std::shared_ptr<const ParameterLayout> m_Layout;
        std::vector<uint8_t> m_Memory;
    };
}
