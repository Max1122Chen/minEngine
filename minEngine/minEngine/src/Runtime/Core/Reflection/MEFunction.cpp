#include "MEFunction.h"

#include "MEClass.h"

#include <cstring>
#include <limits>

namespace minEngine::Reflection
{
    uint32_t MEFunction::AlignUp(uint32_t value, uint32_t alignment)
    {
        if (alignment <= 1)
        {
            return value;
        }
        const uint32_t remainder = value % alignment;
        return remainder == 0 ? value : value + (alignment - remainder);
    }

    MEFunction::MEFunction(std::string inName)
        : m_Name(std::move(inName))
    {
    }

    const MEParamDescriptor* MEFunction::FindParam(const std::string& paramName) const
    {
        for (const MEParamDescriptor& param : m_Params)
        {
            if (param.Property != nullptr && param.Property->GetName() == paramName)
            {
                return &param;
            }
        }
        return nullptr;
    }

    bool MEFunction::CopyParamFromBuffer(const void* parms, const std::string& paramName, void* dest,
                                         size_t destSize) const
    {
        if (parms == nullptr || dest == nullptr || destSize == 0)
        {
            return false;
        }

        const MEParamDescriptor* param = FindParam(paramName);
        if (param == nullptr || param->Property == nullptr)
        {
            return false;
        }

        const uint32_t storageSize = GetPropertyStorageSize(param->Property);
        if (storageSize == 0 || storageSize != destSize)
        {
            return false;
        }

        const auto* base = static_cast<const uint8_t*>(parms);
        std::memcpy(dest, base + param->Offset, destSize);
        return true;
    }

    bool MEFunction::CopyParamToBuffer(void* parms, const std::string& paramName, const void* src, size_t srcSize) const
    {
        if (parms == nullptr || src == nullptr || srcSize == 0)
        {
            return false;
        }

        const MEParamDescriptor* param = FindParam(paramName);
        if (param == nullptr || param->Property == nullptr)
        {
            return false;
        }

        const uint32_t storageSize = GetPropertyStorageSize(param->Property);
        if (storageSize == 0 || storageSize != srcSize)
        {
            return false;
        }

        auto* base = static_cast<uint8_t*>(parms);
        std::memcpy(base + param->Offset, src, srcSize);
        return true;
    }

    const MEParamDescriptor* MEFunction::GetReturnParam() const
    {
        for (const MEParamDescriptor& param : m_Params)
        {
            if (param.IsReturn())
            {
                return &param;
            }
        }
        return nullptr;
    }

    bool MEFunction::AddParameter(MEProperty* property, MEParamRole role, MEParamPassKind passKind)
    {
        if (m_LayoutFinalized)
        {
            return false;
        }

        if (property == nullptr)
        {
            return false;
        }

        if (role == MEParamRole::Return && GetReturnParam() != nullptr)
        {
            return false;
        }

        MEParamDescriptor descriptor;
        descriptor.Property = property;
        descriptor.Role = role;
        descriptor.PassKind = passKind;
        m_Params.push_back(descriptor);
        return true;
    }

    uint32_t MEFunction::GetPropertyStorageSize(const MEProperty* property)
    {
        if (property == nullptr)
        {
            return 0;
        }
        const size_t storageSize = property->GetStorageSize();
        if (storageSize == 0 || storageSize > static_cast<size_t>(std::numeric_limits<uint32_t>::max()))
        {
            return 0;
        }
        return static_cast<uint32_t>(storageSize);
    }

    uint32_t MEFunction::GetPropertyStorageAlignment(const MEProperty* property)
    {
        if (property == nullptr)
        {
            return 0;
        }
        const size_t storageAlignment = property->GetStorageAlignment();
        if (storageAlignment == 0 || storageAlignment > static_cast<size_t>(std::numeric_limits<uint32_t>::max()))
        {
            return 0;
        }
        return static_cast<uint32_t>(storageAlignment);
    }

    bool MEFunction::FinalizeLayout(std::string& outError)
    {
        if (m_LayoutFinalized)
        {
            outError = "MEFunction layout already finalized.";
            return false;
        }

        uint32_t returnCount = 0;
        for (const MEParamDescriptor& param : m_Params)
        {
            if (param.IsReturn())
            {
                ++returnCount;
            }
        }

        if (returnCount > 1)
        {
            outError = "MEFunction '" + m_Name + "' has more than one return parameter.";
            return false;
        }

        uint32_t offset = 0;
        uint32_t maxAlignment = 1;
        m_ReturnValueOffset = -1;

        for (MEParamDescriptor& param : m_Params)
        {
            const uint32_t storageSize = GetPropertyStorageSize(param.Property);
            const uint32_t storageAlignment = GetPropertyStorageAlignment(param.Property);
            if (storageSize == 0)
            {
                outError = "MEFunction '" + m_Name + "' parameter '" + param.Property->GetName()
                           + "' has unsupported or unknown storage size.";
                return false;
            }
            if (storageAlignment == 0)
            {
                outError = "MEFunction '" + m_Name + "' parameter '" + param.Property->GetName()
                           + "' has unsupported or unknown storage alignment.";
                return false;
            }

            offset = AlignUp(offset, storageAlignment);
            param.Offset = offset;
            offset += storageSize;
            if (storageAlignment > maxAlignment)
            {
                maxAlignment = storageAlignment;
            }

            if (param.IsReturn())
            {
                m_ReturnValueOffset = static_cast<int32_t>(param.Offset);
            }
        }

        offset = AlignUp(offset, maxAlignment);
        m_ParmsSize = static_cast<uint16_t>(offset);
        m_NumParms = static_cast<uint8_t>(m_Params.size());
        m_LayoutFinalized = true;

        if (returnCount == 1)
        {
            m_FunctionFlags = static_cast<MEFunctionFlags>(
                static_cast<uint32_t>(m_FunctionFlags) | static_cast<uint32_t>(MEFunctionFlags::HasReturn));
        }

        return true;
    }

    bool MEClass::AddFunction(MEFunction* function)
    {
        if (function == nullptr)
        {
            return false;
        }

        if (function->GetOwnerClass() != this)
        {
            return false;
        }

        if (m_FunctionsByName.find(function->GetName()) != m_FunctionsByName.end())
        {
            return false;
        }

        m_Functions.push_back(function);
        m_FunctionsByName[function->GetName()] = function;
        return true;
    }

    MEFunction* MEClass::FindFunction(const std::string& functionName) const
    {
        auto iter = m_FunctionsByName.find(functionName);
        if (iter == m_FunctionsByName.end())
        {
            return nullptr;
        }
        return iter->second;
    }

} // namespace minEngine::Reflection
