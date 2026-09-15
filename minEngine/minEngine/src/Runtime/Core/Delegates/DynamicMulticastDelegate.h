#pragma once

#include "Runtime/Core/Delegates/DynamicMulticastDelegateBase.h"
#include "Runtime/Core/Delegates/MulticastDelegate.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Core/Object/ObjectManager.h"

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace minEngine
{
    /// Dynamic multicast: wraps Native MulticastDelegate (B1). One Broadcast table.
    template <typename... TArgs>
    class DynamicMulticastDelegate final : public DynamicMulticastDelegateBase
    {
    public:
        using NativeType = MulticastDelegate<TArgs...>;
        using CallbackType = typename NativeType::CallbackType;

        DynamicMulticastDelegate() = default;
        ~DynamicMulticastDelegate() override = default;

        DynamicMulticastDelegate(const DynamicMulticastDelegate&) = delete;
        DynamicMulticastDelegate& operator=(const DynamicMulticastDelegate&) = delete;

        DynamicMulticastDelegate(DynamicMulticastDelegate&&) noexcept = default;
        DynamicMulticastDelegate& operator=(DynamicMulticastDelegate&&) noexcept = default;

        int GetArity() const override { return static_cast<int>(sizeof...(TArgs)); }

        NativeType& GetNative() { return m_Native; }
        const NativeType& GetNative() const { return m_Native; }

        DelegateHandle AddLambda(CallbackType callback) { return m_Native.AddLambda(std::move(callback)); }

        template <typename TUserClass>
        DelegateHandle AddRaw(TUserClass* userObject, void (TUserClass::*method)(TArgs...))
        {
            return m_Native.AddRaw(userObject, method);
        }

        template <typename TUserClass>
        DelegateHandle AddMEObject(TUserClass* userObject, void (TUserClass::*method)(TArgs...))
        {
            return m_Native.AddMEObject(userObject, method);
        }

        /// Bind a reflected instance method by name (void(TArgs...) signature).
        DelegateHandle AddDynamic(MEObject* userObject, const char* functionName)
        {
            if (userObject == nullptr || functionName == nullptr || functionName[0] == '\0')
            {
                return DelegateHandle::Invalid();
            }

            Reflection::MEFunction* function = userObject->FindFunctionTyped<void, TArgs...>(functionName);
            if (function == nullptr)
            {
                ME_LOG(LogCore,
                       Error,
                       "DynamicMulticastDelegate::AddDynamic: function '{}' with matching signature not found on '{}'.",
                       functionName,
                       userObject->GetClass() != nullptr ? userObject->GetClass()->GetName() : "<null class>");
                return DelegateHandle::Invalid();
            }

            if (function->IsStatic())
            {
                ME_LOG(LogCore, Error, "DynamicMulticastDelegate::AddDynamic: static function '{}' is not supported.", functionName);
                return DelegateHandle::Invalid();
            }

            const GUID objectGuid = userObject->GetGuid();
            if (!objectGuid.IsValid())
            {
                return DelegateHandle::Invalid();
            }

            const std::string nameCopy(functionName);
            return m_Native.AddLambda(
                [objectGuid, nameCopy](TArgs... args)
                {
                    if (!ObjectManager::HasInstance())
                    {
                        return;
                    }

                    std::shared_ptr<MEObject> liveObject = FindObject(objectGuid);
                    if (!liveObject)
                    {
                        return;
                    }

                    liveObject->InvokeFunctionTyped(nameCopy, args...);
                });
        }

        DelegateHandle AddScript(CallableScriptFunction callable)
        {
            if (!callable.IsValid())
            {
                return DelegateHandle::Invalid();
            }

            auto sharedCallable = std::make_shared<CallableScriptFunction>(std::move(callable));
            return m_Native.AddLambda(
                [sharedCallable](TArgs... args)
                {
                    sharedCallable->Invoke(args...);
                });
        }

        DelegateHandle AddScriptErased(CallableScriptFunction callable) override
        {
            return AddScript(std::move(callable));
        }

        void Remove(DelegateHandle handle) override { m_Native.Remove(handle); }

        void RemoveAll(const void* userObject) { m_Native.RemoveAll(userObject); }

        void Clear() override { m_Native.Clear(); }

        bool IsBound() const override { return m_Native.IsBound(); }

        size_t GetBindingCount() const override { return m_Native.GetBindingCount(); }

        void Broadcast(TArgs... args) { m_Native.Broadcast(args...); }

    private:
        NativeType m_Native;
    };

    template <typename... TArgs>
    struct IsDynamicMulticastDelegateField<DynamicMulticastDelegate<TArgs...>> : std::true_type
    {
    };
}

/// Binds UserObject's MethodName (identifier) via AddDynamic. Example: ME_ADD_DYNAMIC(OnClicked, this, HandleClick)
#define ME_ADD_DYNAMIC(DelegateExpr, UserObject, MethodName) \
    (DelegateExpr).AddDynamic((UserObject), #MethodName)
