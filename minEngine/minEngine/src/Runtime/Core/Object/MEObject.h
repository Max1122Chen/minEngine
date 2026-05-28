#pragma once
#include "Core.h"
#include "Runtime/Core/GUID/GUID.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Core/Reflection/MEFunction.h"
#include "Runtime/Core/Reflection/MEFunctionFrame.h"

#include <type_traits>
#include <vector>

namespace minEngine::Reflection
{
    class ReflectionSystem;
    class MEClass;
    class MEFunction;
}

namespace minEngine::Serialization
{
    class Serializer;
}

namespace minEngine
{
    
    ME_CLASS()
    class MEObject
    {
        ME_GENERATED_BODY(MEObject)
        // Friend declaration for engine core classes 
        friend class Reflection::ReflectionSystem;
        friend class Reflection::MEClass;
        friend class ObjectManager;
        friend class Serialization::Serializer;
        friend class AssetManager;
        // Friend declaration for editor classes
        friend class Editor;
    public:
        virtual ~MEObject();

        const Reflection::MEClass* GetClass() const { return m_Class; }
        const std::string& GetName() const { return m_Name; }

        const GUID& GetGuid() const { return m_Guid; }

        const MEObject* GetOuter() const { return m_Outer; }
        void SetOuter(MEObject* inOuter) { m_Outer = inOuter; }

        bool IsA(const Reflection::MEClass* classInfo) const
        {
            return (m_Class != nullptr) && m_Class->IsA(classInfo);
        }

        bool InvokeFunction(Reflection::MEFunction* function, void* parmsBuffer);
        bool InvokeFunctionByName(const std::string& functionName, void* parmsBuffer);
        bool InvokeFunction(const std::string& functionName, uint64_t signatureHash, void* parmsBuffer);

        template<typename TReturn, typename... TArgs>
        Reflection::MEFunction* FindFunctionTyped(const std::string& functionName) const
        {
            if (m_Class == nullptr)
            {
                return nullptr;
            }

            const uint64_t signatureHash = Reflection::MEFunction::BuildSignatureHashForTypes<TReturn, TArgs...>();
            return m_Class->FindFunctionBySignature(functionName, signatureHash);
        }

        template<typename... TArgs>
        bool InvokeFunctionTyped(const std::string& functionName, TArgs&&... args)
        {
            Reflection::MEFunction* function = FindFunctionTyped<void, TArgs...>(functionName);
            if (function == nullptr)
            {
                return false;
            }

            Reflection::MEFunctionFrame frame(*function);
            if (!SetTypedInputArgs(frame, *function, std::forward<TArgs>(args)...))
            {
                return false;
            }

            return InvokeFunction(functionName, function->GetSignatureHash(), frame.GetBuffer());
        }

        template<typename TReturn, typename... TArgs>
        bool InvokeFunctionTyped(const std::string& functionName, TReturn& outReturnValue, TArgs&&... args)
        {
            Reflection::MEFunction* function = FindFunctionTyped<TReturn, TArgs...>(functionName);
            if (function == nullptr)
            {
                return false;
            }

            Reflection::MEFunctionFrame frame(*function);
            if (!SetTypedInputArgs(frame, *function, std::forward<TArgs>(args)...))
            {
                return false;
            }

            if (!InvokeFunction(functionName, function->GetSignatureHash(), frame.GetBuffer()))
            {
                return false;
            }

            using RawReturnType = minEngine::RemoveCvRefT<TReturn>;
            if constexpr (std::is_trivially_copyable_v<RawReturnType>)
            {
                return frame.GetParam("ReturnValue", outReturnValue);
            }
            else
            {
                const RawReturnType* returnPtr = nullptr;
                if (!frame.GetParamValuePtr("ReturnValue", returnPtr) || returnPtr == nullptr)
                {
                    return false;
                }
                outReturnValue = *returnPtr;
                return true;
            }
        }

    protected:
        void SetClass(const Reflection::MEClass* inClass) { m_Class = inClass; }
        void SetName(const std::string& inName) { m_Name = inName; }
        void SetGuid(const GUID& inGuid) { m_Guid = inGuid; }
    protected:
        const Reflection::MEClass* m_Class = nullptr;

        ME_PROPERTY(Invisible)
        std::string m_Name;
        ME_PROPERTY(Invisible)
        GUID m_Guid;
        MEObject* m_Outer = nullptr;

    private:
        template<typename... TArgs>
        static bool SetTypedInputArgs(Reflection::MEFunctionFrame& frame,
                                      const Reflection::MEFunction& function,
                                      TArgs&&... args)
        {
            std::vector<const Reflection::MEParamDescriptor*> inputParams;
            inputParams.reserve(function.GetParams().size());
            for (const Reflection::MEParamDescriptor& param : function.GetParams())
            {
                if (param.Role == Reflection::MEParamRole::In)
                {
                    inputParams.push_back(&param);
                }
            }

            if (inputParams.size() != sizeof...(TArgs))
            {
                return false;
            }

            return SetTypedInputArgsImpl(frame, inputParams, std::index_sequence_for<TArgs...>{},
                                         std::forward<TArgs>(args)...);
        }

        template<typename... TArgs, size_t... TIndices>
        static bool SetTypedInputArgsImpl(Reflection::MEFunctionFrame& frame,
                                          const std::vector<const Reflection::MEParamDescriptor*>& inputParams,
                                          std::index_sequence<TIndices...>,
                                          TArgs&&... args)
        {
            bool allOk = true;
            auto setOne = [&](const Reflection::MEParamDescriptor* param, auto&& arg) -> bool
            {
                if (param == nullptr || param->Property == nullptr)
                {
                    return false;
                }

                const std::string& paramName = param->Property->GetName();
                switch (param->PassKind)
                {
                    case Reflection::MEParamPassKind::Ref:
                    {
                        using ArgType = decltype(arg);
                        if constexpr (!std::is_lvalue_reference_v<ArgType> || std::is_const_v<std::remove_reference_t<ArgType>>)
                        {
                            return false;
                        }
                        return frame.SetParamRef(paramName, arg);
                    }
                    case Reflection::MEParamPassKind::ConstRef:
                        return frame.SetParamConstRef(paramName, arg);
                    case Reflection::MEParamPassKind::Value:
                    case Reflection::MEParamPassKind::ConstValue:
                        return frame.SetParam(paramName, arg);
                    default:
                        return false;
                }
            };

            ((allOk = allOk && setOne(inputParams[TIndices], std::forward<TArgs>(args))), ...);
            return allOk;
        }
    };

    
}

#include "MEObject.gen.h"