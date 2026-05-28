#include "MEFunction.h"

#include "MEClass.h"
#include "Runtime/Core/Log/LogSystem.h"

#include <array>
#include <cstring>
#include <limits>
#include <sstream>

namespace minEngine::Reflection
{
    namespace
    {
        std::string BuildPropertyTypeSignature(const MEProperty* property)
        {
            if (property == nullptr)
            {
                return "null";
            }

            switch (property->GetCategory())
            {
                case MEPropertyCategory::Primitive:
                {
                    const MEPrimitiveProperty* primitive = static_cast<const MEPrimitiveProperty*>(property);
                    return "P:" + primitive->primitiveTypeName;
                }
                case MEPropertyCategory::Object:
                {
                    const MEObjectProperty* objectProperty = static_cast<const MEObjectProperty*>(property);
                    const MEClass* valueClass = objectProperty->GetValueClass();
                    return "O:" + std::string(valueClass != nullptr ? valueClass->GetName() : "UnresolvedObject");
                }
                case MEPropertyCategory::ObjectPtr:
                {
                    const MEObjectPtrProperty* ptrProperty = static_cast<const MEObjectPtrProperty*>(property);
                    const MEClass* valueClass = ptrProperty->GetValueClass();
                    const std::array<const char*, 4> ptrCategoryNames = {
                        "Invalid", "Raw", "Shared", "Weak"
                    };
                    const uint32_t ptrCategoryIndex = static_cast<uint32_t>(ptrProperty->GetPtrCategory());
                    const char* ptrCategoryName = ptrCategoryIndex < ptrCategoryNames.size()
                                                      ? ptrCategoryNames[ptrCategoryIndex]
                                                      : "UnknownPtr";
                    return "OP:" + std::string(ptrCategoryName) + ":"
                           + std::string(valueClass != nullptr ? valueClass->GetName() : "UnresolvedObject");
                }
                case MEPropertyCategory::Array:
                {
                    const MEArrayProperty* arrayProperty = static_cast<const MEArrayProperty*>(property);
                    return "A<" + BuildPropertyTypeSignature(arrayProperty->GetInnerProperty()) + ">";
                }
                default:
                    return "Unknown";
            }
        }
    } // namespace

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

        const uint32_t storageSize = GetParamStorageSize(param->Property, param->Role, param->PassKind);
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

        const uint32_t storageSize = GetParamStorageSize(param->Property, param->Role, param->PassKind);
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

    bool MEFunction::IsPointerSlot(MEParamRole role, MEParamPassKind passKind)
    {
        if (role == MEParamRole::Out)
        {
            return true;
        }

        return passKind == MEParamPassKind::Ref || passKind == MEParamPassKind::ConstRef;
    }

    uint32_t MEFunction::GetParamStorageSize(MEProperty* property, MEParamRole role, MEParamPassKind passKind)
    {
        if (IsPointerSlot(role, passKind))
        {
            return static_cast<uint32_t>(sizeof(void*));
        }
        return GetPropertyStorageSize(property);
    }

    uint32_t MEFunction::GetParamStorageAlignment(MEProperty* property, MEParamRole role, MEParamPassKind passKind)
    {
        if (IsPointerSlot(role, passKind))
        {
            return static_cast<uint32_t>(alignof(void*));
        }
        return GetPropertyStorageAlignment(property);
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
            if (param.Role == MEParamRole::Return
                && (param.PassKind == MEParamPassKind::Ref || param.PassKind == MEParamPassKind::ConstRef))
            {
                outError = "MEFunction '" + m_Name + "' return parameter '" + param.Property->GetName()
                           + "' cannot be Ref/ConstRef in current implementation.";
                return false;
            }

            const uint32_t storageSize = GetParamStorageSize(param.Property, param.Role, param.PassKind);
            const uint32_t storageAlignment = GetParamStorageAlignment(param.Property, param.Role, param.PassKind);
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

        m_SignatureText = BuildSignatureText();
        m_SignatureHash = HashSignatureText(m_SignatureText);

        return true;
    }

    uint64_t MEFunction::HashSignatureText(std::string_view text)
    {
        constexpr uint64_t kFnvOffsetBasis = 1469598103934665603ull;
        constexpr uint64_t kFnvPrime = 1099511628211ull;
        uint64_t hash = kFnvOffsetBasis;
        for (char ch : text)
        {
            hash ^= static_cast<uint8_t>(ch);
            hash *= kFnvPrime;
        }
        return hash;
    }

    uint64_t MEFunction::ComputeSignatureHash(std::string_view signatureText)
    {
        return HashSignatureText(signatureText);
    }

    std::string MEFunction::BuildSignatureText() const
    {
        std::ostringstream signature;
        signature << "R(";
        const MEParamDescriptor* returnParam = GetReturnParam();
        if (returnParam != nullptr)
        {
            signature << BuildPropertyTypeSignature(returnParam->Property);
        }
        signature << ")-P(";

        bool firstParam = true;
        for (const MEParamDescriptor& param : m_Params)
        {
            if (param.IsReturn())
            {
                continue;
            }

            if (!firstParam)
            {
                signature << ",";
            }
            firstParam = false;
            signature << static_cast<uint32_t>(param.Role) << ":"
                      << static_cast<uint32_t>(param.PassKind) << ":"
                      << BuildPropertyTypeSignature(param.Property);
        }
        signature << ")";
        return signature.str();
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

        const std::string signatureKey = function->GetName() + "#" + std::to_string(function->GetSignatureHash());
        if (m_FunctionsBySignatureKey.find(signatureKey) != m_FunctionsBySignatureKey.end())
        {
            return false;
        }

        m_Functions.push_back(function);
        m_FunctionsByName[function->GetName()].push_back(function);
        m_FunctionsBySignatureKey[signatureKey] = function;
        return true;
    }

    MEFunction* MEClass::FindFunction(const std::string& functionName) const
    {
        const MEClass* current = this;
        while (current != nullptr)
        {
            if (MEFunction* owned = current->FindFunctionOwned(functionName))
            {
                return owned;
            }
            current = current->GetSuperClass();
        }
        return nullptr;
    }

    MEFunction* MEClass::FindFunctionBySignature(const std::string& functionName, uint64_t signatureHash) const
    {
        const MEClass* current = this;
        while (current != nullptr)
        {
            if (MEFunction* owned = current->FindFunctionBySignatureOwned(functionName, signatureHash))
            {
                return owned;
            }
            current = current->GetSuperClass();
        }
        return nullptr;
    }

    MEFunction* MEClass::FindFunctionOwned(const std::string& functionName) const
    {
        auto iter = m_FunctionsByName.find(functionName);
        if (iter == m_FunctionsByName.end() || iter->second.empty())
        {
            return nullptr;
        }
        return iter->second.front();
    }

    MEFunction* MEClass::FindFunctionBySignatureOwned(const std::string& functionName, uint64_t signatureHash) const
    {
        const std::string signatureKey = functionName + "#" + std::to_string(signatureHash);
        auto iter = m_FunctionsBySignatureKey.find(signatureKey);
        if (iter == m_FunctionsBySignatureKey.end())
        {
            return nullptr;
        }
        return iter->second;
    }

    bool MEClass::InvokeStaticFunction(MEFunction* function, void* parmsBuffer) const
    {
        if (function == nullptr)
        {
            ME_CORE_ERROR("MEClass::InvokeStaticFunction: null function.");
            return false;
        }

        if (!function->IsStatic())
        {
            ME_CORE_ERROR("MEClass::InvokeStaticFunction: '{}' is not static.", function->GetName());
            return false;
        }

        if (function->GetOwnerClass() != this)
        {
            ME_CORE_ERROR("MEClass::InvokeStaticFunction: owner class mismatch for '{}'.", function->GetName());
            return false;
        }

        if (parmsBuffer == nullptr && function->GetParmsSize() > 0)
        {
            ME_CORE_ERROR("MEClass::InvokeStaticFunction: null parms buffer for '{}'.", function->GetName());
            return false;
        }

        const MENativeThunkFn nativeThunk = function->GetNativeThunk();
        if (nativeThunk == nullptr)
        {
            ME_CORE_ERROR("MEClass::InvokeStaticFunction: no native thunk for '{}'.", function->GetName());
            return false;
        }

        nativeThunk(nullptr, function, parmsBuffer);
        return true;
    }

} // namespace minEngine::Reflection
