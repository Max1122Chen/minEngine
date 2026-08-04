#pragma once

#include "Runtime/Core/Delegates/DelegateHandle.h"
#include "Runtime/Core/Object/ObjectManager.h"

#include <algorithm>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

namespace minEngine
{
    /// Type-safe native multicast delegate (UE TMulticastDelegate-style).
    /// Not thread-safe. Broadcast is synchronous and uses an invocation snapshot
    /// so Add/Remove/Clear during Broadcast affect the next Broadcast only.
    template <typename... TArgs>
    class MulticastDelegate
    {
    public:
        using CallbackType = std::function<void(TArgs...)>;

        MulticastDelegate() = default;
        ~MulticastDelegate() = default;

        MulticastDelegate(const MulticastDelegate&) = delete;
        MulticastDelegate& operator=(const MulticastDelegate&) = delete;

        MulticastDelegate(MulticastDelegate&&) noexcept = default;
        MulticastDelegate& operator=(MulticastDelegate&&) noexcept = default;

        DelegateHandle AddLambda(CallbackType callback)
        {
            if (!callback)
            {
                return DelegateHandle::Invalid();
            }

            return EmplaceBinding(std::move(callback), nullptr, GUID{}, false);
        }

        template <typename TUserClass>
        DelegateHandle AddRaw(TUserClass* userObject, void (TUserClass::*method)(TArgs...))
        {
            if (userObject == nullptr || method == nullptr)
            {
                return DelegateHandle::Invalid();
            }

            return EmplaceBinding(
                [userObject, method](TArgs... args)
                {
                    (userObject->*method)(args...);
                },
                userObject,
                GUID{},
                false);
        }

        /// Weak-ish MEObject binding: stores GUID and skips invoke when FindObject misses.
        /// Prefer this over AddRaw for gameplay listeners that outlive subscribe sites.
        template <typename TUserClass>
        DelegateHandle AddMEObject(TUserClass* userObject, void (TUserClass::*method)(TArgs...))
        {
            static_assert(std::is_base_of_v<MEObject, TUserClass>,
                          "AddMEObject requires TUserClass to derive from MEObject");

            if (userObject == nullptr || method == nullptr)
            {
                return DelegateHandle::Invalid();
            }

            const GUID objectGuid = userObject->GetGuid();
            if (!objectGuid.IsValid())
            {
                return DelegateHandle::Invalid();
            }

            return EmplaceBinding(
                [objectGuid, method](TArgs... args)
                {
                    if (!ObjectManager::HasInstance())
                    {
                        return;
                    }

                    std::shared_ptr<TUserClass> liveObject = FindObjectAs<TUserClass>(objectGuid);
                    if (liveObject)
                    {
                        (liveObject.get()->*method)(args...);
                    }
                },
                userObject,
                objectGuid,
                true);
        }

        void Remove(DelegateHandle handle)
        {
            if (!handle.IsValid())
            {
                return;
            }

            m_Bindings.erase(
                std::remove_if(
                    m_Bindings.begin(),
                    m_Bindings.end(),
                    [handle](const Binding& binding)
                    {
                        return binding.Handle == handle;
                    }),
                m_Bindings.end());
        }

        /// Removes all Raw / MEObject bindings registered with this instance pointer.
        void RemoveAll(const void* userObject)
        {
            if (userObject == nullptr)
            {
                return;
            }

            m_Bindings.erase(
                std::remove_if(
                    m_Bindings.begin(),
                    m_Bindings.end(),
                    [userObject](const Binding& binding)
                    {
                        return binding.UserObject == userObject;
                    }),
                m_Bindings.end());
        }

        void Clear() { m_Bindings.clear(); }

        bool IsBound() const { return !m_Bindings.empty(); }

        size_t GetBindingCount() const { return m_Bindings.size(); }

        void Broadcast(TArgs... args)
        {
            const std::vector<Binding> snapshot = m_Bindings;
            for (const Binding& binding : snapshot)
            {
                if (binding.IsMEObjectBinding && !IsMEObjectBindingAlive(binding))
                {
                    continue;
                }

                if (binding.Callback)
                {
                    binding.Callback(args...);
                }
            }

            CompactStaleMEObjectBindings();
        }

    private:
        struct Binding
        {
            DelegateHandle Handle;
            CallbackType Callback;
            const void* UserObject = nullptr;
            GUID ObjectGuid;
            bool IsMEObjectBinding = false;
        };

        DelegateHandle EmplaceBinding(
            CallbackType callback,
            const void* userObject,
            const GUID& objectGuid,
            bool isMEObjectBinding)
        {
            Binding binding;
            binding.Handle = DelegateHandle::Create();
            binding.Callback = std::move(callback);
            binding.UserObject = userObject;
            binding.ObjectGuid = objectGuid;
            binding.IsMEObjectBinding = isMEObjectBinding;
            m_Bindings.push_back(std::move(binding));
            return m_Bindings.back().Handle;
        }

        static bool IsMEObjectBindingAlive(const Binding& binding)
        {
            if (!binding.IsMEObjectBinding || !binding.ObjectGuid.IsValid())
            {
                return false;
            }

            if (!ObjectManager::HasInstance())
            {
                return false;
            }

            return FindObject(binding.ObjectGuid) != nullptr;
        }

        void CompactStaleMEObjectBindings()
        {
            if (!ObjectManager::HasInstance())
            {
                return;
            }

            m_Bindings.erase(
                std::remove_if(
                    m_Bindings.begin(),
                    m_Bindings.end(),
                    [](const Binding& binding)
                    {
                        return binding.IsMEObjectBinding && !IsMEObjectBindingAlive(binding);
                    }),
                m_Bindings.end());
        }

        std::vector<Binding> m_Bindings;
    };
}
