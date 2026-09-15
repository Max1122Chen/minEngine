#pragma once

#include "Runtime/Core/Delegates/CallableScriptFunction.h"
#include "Runtime/Core/Delegates/DelegateHandle.h"

#include <type_traits>

namespace minEngine
{
    /// Non-template base so reflection can hold DynamicMulticastDelegate* without knowing arity.
    class DynamicMulticastDelegateBase
    {
    public:
        virtual ~DynamicMulticastDelegateBase() = default;

        virtual int GetArity() const = 0;
        virtual DelegateHandle AddScriptErased(CallableScriptFunction callable) = 0;
        virtual bool IsBound() const = 0;
        virtual size_t GetBindingCount() const = 0;
        virtual void Clear() = 0;
        virtual void Remove(DelegateHandle handle) = 0;
    };

    /// Detect DynamicMulticastDelegate<Args...> in CreatePropertyByType (specialized in DynamicMulticastDelegate.h).
    template <typename T>
    struct IsDynamicMulticastDelegateField : std::false_type
    {
    };
}
