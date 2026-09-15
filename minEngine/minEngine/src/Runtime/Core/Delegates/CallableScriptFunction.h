#pragma once

#include <type_traits>
#include <utility>

namespace minEngine
{
    /// Type-erased script/runtime callable stored in Dynamic multicast slots.
    /// Named to avoid colliding with ME_FUNCTION(ScriptCallable) specifier (CORE-F02).
    /// Core must not depend on sol2; Lua fills this in CORE-F21.
    class CallableScriptFunction
    {
    public:
        /// args[i] points to the i-th Broadcast argument (arity may be 0).
        using InvokeFn = void (*)(void* user, void* const* args, int arity);
        using DestroyFn = void (*)(void* user);

        CallableScriptFunction() = default;

        CallableScriptFunction(void* user, InvokeFn invoke, DestroyFn destroy)
            : m_User(user)
            , m_Invoke(invoke)
            , m_Destroy(destroy)
        {
        }

        ~CallableScriptFunction()
        {
            Reset();
        }

        CallableScriptFunction(const CallableScriptFunction&) = delete;
        CallableScriptFunction& operator=(const CallableScriptFunction&) = delete;

        CallableScriptFunction(CallableScriptFunction&& other) noexcept
        {
            MoveFrom(other);
        }

        CallableScriptFunction& operator=(CallableScriptFunction&& other) noexcept
        {
            if (this != &other)
            {
                Reset();
                MoveFrom(other);
            }
            return *this;
        }

        bool IsValid() const { return m_Invoke != nullptr; }

        template <typename... TArgs>
        void Invoke(TArgs&... args) const
        {
            if (m_Invoke == nullptr)
            {
                return;
            }

            if constexpr (sizeof...(TArgs) == 0)
            {
                m_Invoke(m_User, nullptr, 0);
            }
            else
            {
                void* argPtrs[] = { const_cast<void*>(static_cast<const void*>(&args))... };
                m_Invoke(m_User, argPtrs, static_cast<int>(sizeof...(TArgs)));
            }
        }

    private:
        void Reset()
        {
            if (m_Destroy != nullptr && m_User != nullptr)
            {
                m_Destroy(m_User);
            }
            m_User = nullptr;
            m_Invoke = nullptr;
            m_Destroy = nullptr;
        }

        void MoveFrom(CallableScriptFunction& other)
        {
            m_User = other.m_User;
            m_Invoke = other.m_Invoke;
            m_Destroy = other.m_Destroy;
            other.m_User = nullptr;
            other.m_Invoke = nullptr;
            other.m_Destroy = nullptr;
        }

        void* m_User = nullptr;
        InvokeFn m_Invoke = nullptr;
        DestroyFn m_Destroy = nullptr;
    };
}
