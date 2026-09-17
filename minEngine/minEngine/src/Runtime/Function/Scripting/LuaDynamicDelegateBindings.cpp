#include "LuaDynamicDelegateBindings.h"

#include "LuaScriptSystem.h"

#include "Runtime/Core/Delegates/CallableScriptFunction.h"
#include "Runtime/Core/Delegates/DelegateHandle.h"
#include "Runtime/Core/Delegates/DynamicMulticastDelegateBase.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Framework/Components/LuaComponent.h"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

namespace minEngine
{
    namespace
    {
        struct SolProtectedFunctionUser
        {
            sol::protected_function Function;
        };

        void InvokeSolProtectedFunction(void* user, void* const* /*args*/, int arity)
        {
            if (user == nullptr)
            {
                return;
            }

            auto* storage = static_cast<SolProtectedFunctionUser*>(user);
            if (!storage->Function.valid())
            {
                return;
            }

            // CORE-F21 MVP: only zero-argument Broadcast → Lua.
            if (arity != 0)
            {
                ME_LOG(LogScript,
                       Warn,
                       "MakeCallableScriptFunction: arity {} not pushed to Lua yet (MVP is 0).",
                       arity);
                return;
            }

            const sol::protected_function_result result = storage->Function();
            if (!result.valid())
            {
                const sol::error error = result;
                if (LuaScriptSystem::HasInstance())
                {
                    LuaScriptSystem::Get().ReportLuaError("DynamicMulticastDelegate::Add", error);
                }
            }
        }

        void DestroySolProtectedFunction(void* user)
        {
            delete static_cast<SolProtectedFunctionUser*>(user);
        }

        CallableScriptFunction MakeCallableScriptFunction(const sol::protected_function& function)
        {
            if (!function.valid())
            {
                return CallableScriptFunction{};
            }

            auto* user = new SolProtectedFunctionUser{function};
            return CallableScriptFunction(user, &InvokeSolProtectedFunction, &DestroySolProtectedFunction);
        }

        /// Object-first like AddMEObject: host then function. Lua: delegate:Add(self, fn).
        DelegateHandle AddScriptFromLua(DynamicMulticastDelegateBase& delegate,
                                        LuaComponent* host,
                                        sol::protected_function function)
        {
            if (!function.valid())
            {
                ME_LOG(LogScript, Error, "DynamicMulticastDelegate:Add: expected a Lua function.");
                return DelegateHandle::Invalid();
            }

            CallableScriptFunction callable = MakeCallableScriptFunction(function);
            const DelegateHandle handle = delegate.AddScript(std::move(callable));
            if (!handle.IsValid())
            {
                return handle;
            }

            if (host != nullptr)
            {
                host->TrackScriptDelegateBinding(&delegate, handle);
            }

            return handle;
        }
    } // namespace

    void LuaDynamicDelegateBindings::Register(sol::state& state)
    {
        state.new_usertype<DelegateHandle>(
            "DelegateHandle",
            sol::no_constructor,
            "IsValid",
            &DelegateHandle::IsValid);

        state.new_usertype<DynamicMulticastDelegateBase>(
            "DynamicMulticastDelegate",
            sol::no_constructor,
            "Add",
            [](DynamicMulticastDelegateBase& self, LuaComponent* host, sol::protected_function function)
            {
                return AddScriptFromLua(self, host, std::move(function));
            },
            "Remove",
            &DynamicMulticastDelegateBase::Remove,
            "Clear",
            &DynamicMulticastDelegateBase::Clear,
            "IsBound",
            &DynamicMulticastDelegateBase::IsBound,
            "GetBindingCount",
            &DynamicMulticastDelegateBase::GetBindingCount,
            "GetArity",
            &DynamicMulticastDelegateBase::GetArity);
    }
}
