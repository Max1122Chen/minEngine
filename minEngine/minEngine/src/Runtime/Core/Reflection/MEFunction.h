#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "EngineAPI.h"
#include "MEProperties.h"

namespace minEngine
{
    class MEObject;
}

namespace minEngine::Reflection
{
    class MEClass;

    class MEFunction;
    using MENativeThunkFn = void (*)(minEngine::MEObject* context, MEFunction* function, void* parms);

    enum class MEFunctionFlags : uint32_t
    {
        None = 0u,
        Native = 1u << 0,
        Static = 1u << 1,
        ConstMethod = 1u << 2,
        HasReturn = 1u << 3,
        HasOutParams = 1u << 4,
        Callable = 1u << 5,
    };

    enum class FunctionSpecifier : uint32_t
    {
        None = 0u,
        BlueprintCallable = 1u << 0,
        BlueprintPure = 1u << 1,
        Exec = 1u << 2,
        Deprecated = 1u << 3,
    };
    using FunctionSpecifierMask = uint32_t;
    using FunctionMetadata = std::unordered_map<std::string, std::string>;

    enum class MEParamPassKind : uint8_t
    {
        Value = 0,
        ConstValue = 1,
        Ref = 2,
        ConstRef = 3,
    };

    enum class MEParamRole : uint8_t
    {
        In = 0,
        Return = 1,
        Out = 2,
    };

    struct MEParamDescriptor
    {
        MEProperty* Property = nullptr;
        MEParamPassKind PassKind = MEParamPassKind::Value;
        MEParamRole Role = MEParamRole::In;
        uint32_t Offset = 0;

        bool IsReturn() const { return Role == MEParamRole::Return; }
        bool IsOut() const { return Role == MEParamRole::Out; }
    };

    class MINENGINE_API MEFunction
    {
        friend class ReflectionSystem;

    public:
        const std::string& GetName() const { return m_Name; }
        const MEClass* GetOwnerClass() const { return m_OwnerClass; }
        MEFunctionFlags GetFlags() const { return m_FunctionFlags; }

        uint16_t GetParmsSize() const { return m_ParmsSize; }
        uint8_t GetNumParms() const { return m_NumParms; }
        int32_t GetReturnValueOffset() const { return m_ReturnValueOffset; }

        const std::vector<MEParamDescriptor>& GetParams() const { return m_Params; }
        const MEParamDescriptor* GetReturnParam() const;

        bool IsStatic() const;
        bool IsConstMethod() const;
        bool HasReturn() const;

        void SetFlags(MEFunctionFlags flags) { m_FunctionFlags = flags; }
        void SetAnnotations(FunctionSpecifierMask inSpecifierMask, FunctionMetadata inMetadata)
        {
            m_FunctionSpecifierMask = inSpecifierMask;
            m_FunctionMetadata = std::move(inMetadata);
        }
        FunctionSpecifierMask GetSpecifierMask() const { return m_FunctionSpecifierMask; }
        const FunctionMetadata& GetMetadata() const { return m_FunctionMetadata; }

        bool AddParameter(MEProperty* property, MEParamRole role, MEParamPassKind passKind);
        bool FinalizeLayout(std::string& outError);

        const MEParamDescriptor* FindParam(const std::string& paramName) const;
        bool CopyParamFromBuffer(const void* parms, const std::string& paramName, void* dest, size_t destSize) const;
        bool CopyParamToBuffer(void* parms, const std::string& paramName, const void* src, size_t srcSize) const;

        void SetNativeThunk(MENativeThunkFn nativeThunk) { m_NativeThunk = nativeThunk; }
        MENativeThunkFn GetNativeThunk() const { return m_NativeThunk; }

    private:
        explicit MEFunction(std::string inName);

        void SetOwnerClass(MEClass* ownerClass) { m_OwnerClass = ownerClass; }

        static uint32_t AlignUp(uint32_t value, uint32_t alignment);
        static uint32_t GetPropertyStorageSize(const MEProperty* property);
        static uint32_t GetPropertyStorageAlignment(const MEProperty* property);
        static bool IsPointerSlot(MEParamRole role, MEParamPassKind passKind);
        static uint32_t GetParamStorageSize(MEProperty* property, MEParamRole role, MEParamPassKind passKind);
        static uint32_t GetParamStorageAlignment(MEProperty* property, MEParamRole role, MEParamPassKind passKind);

        std::string m_Name;
        MEClass* m_OwnerClass = nullptr;
        MEFunctionFlags m_FunctionFlags = MEFunctionFlags::None;
        uint16_t m_ParmsSize = 0;
        uint8_t m_NumParms = 0;
        int32_t m_ReturnValueOffset = -1;
        std::vector<MEParamDescriptor> m_Params;
        MENativeThunkFn m_NativeThunk = nullptr;
        FunctionSpecifierMask m_FunctionSpecifierMask = static_cast<FunctionSpecifierMask>(FunctionSpecifier::None);
        FunctionMetadata m_FunctionMetadata;
        bool m_LayoutFinalized = false;
    };

    inline bool MEFunction::IsStatic() const
    {
        return (static_cast<uint32_t>(m_FunctionFlags) & static_cast<uint32_t>(MEFunctionFlags::Static)) != 0;
    }

    inline bool MEFunction::IsConstMethod() const
    {
        return (static_cast<uint32_t>(m_FunctionFlags) & static_cast<uint32_t>(MEFunctionFlags::ConstMethod)) != 0;
    }

    inline bool MEFunction::HasReturn() const
    {
        return (static_cast<uint32_t>(m_FunctionFlags) & static_cast<uint32_t>(MEFunctionFlags::HasReturn)) != 0;
    }

} // namespace minEngine::Reflection
